#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_factory.h"
#include "nrc_assert.h"
#include "nrc_serial.h"

#include <string.h>

// Event bitmask for nrc callbacks
#define NRC_N_SERIAL_OUT_EVT_WRITE_COMPLETE  (1)
#define NRC_N_SERIAL_OUT_EVT_ERROR           (2)

// Type number to check that node pointer is of correct type
#define NRC_N_SERIAL_OUT_TYPE            (0x01432FD2)

// Default max buffer read size
#define NRC_N_SERIAL_OUT_MAX_BUF_SIZE (256)

// Node states
enum nrc_node_serial_out_state {
    NRC_N_SERIAL_OUT_S_INVALID = 0,      
    NRC_N_SERIAL_OUT_S_CREATED,      // Created but not yet initialised. No memory allocated except the object itself.
    NRC_N_SERIAL_OUT_S_INITIALISED,  // Initialised and memory allocated. Ready to be started.
    NRC_N_SERIAL_OUT_S_STARTED,      // Resources is started and node is now running.
    NRC_N_SERIAL_OUT_S_ERROR         // Error occurred which node cannot recover from. Other nodes can continue to run.
};

struct nrc_node_serial_out {
    struct nrc_node_hdr             hdr;            // General node header; from create function in pars

    const s8_t                      *topic;         // Node topic from cfg
    const s8_t                      *cfg_serial_id; // serial-port (configuration node) id from cfg
    s8_t                            prio;           // Node prio; used for sending messages; from cfg
    u32_t                           max_buf_size;   // Max write size

    enum nrc_node_serial_out_state  state;          // Node state
    nrc_serial_t                    serial;         // NRC serial port
    struct nrc_serial_writer        writer;         // Writer notification data

    u32_t                           type;           // Object type check; unique number for every object type
};

// Node create function registered in node factory with type string
static nrc_node_t nrc_node_serial_out_create(struct nrc_node_factory_pars *pars);

// Node api functions
static s32_t nrc_node_serial_out_init(nrc_node_t self);
static s32_t nrc_node_serial_out_deinit(nrc_node_t self);
static s32_t nrc_node_serial_out_start(nrc_node_t self);
static s32_t nrc_node_serial_out_stop(nrc_node_t self);
static s32_t nrc_node_serial_out_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_serial_out_recv_evt(nrc_node_t self, u32_t event_mask);

// Internal functions
static s32_t write_data(struct nrc_node_serial_out *self, struct nrc_msg_hdr *msg_hdr);

// Internal variables
const static s8_t*          _tag = "serial-out"; // Used in NRC_LOG function
static struct nrc_node_api  _api;                // Node API functions

// Register node to node factory; called at system start
void nrc_node_serial_out_register(void)
{
    s32_t result = nrc_factory_register_node_type("serial-out", nrc_node_serial_out_create);
    if (!OK(result)) {
        NRC_LOGE(_tag, "register: error %d", result);
    }
}

// Node constructor function
nrc_node_t nrc_node_serial_out_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_serial_out *self = NULL;

    if ((pars != NULL) && (strcmp("serial-out", pars->cfg_type) == 0)) {
        self = (struct nrc_node_serial_out*)nrc_os_node_alloc(sizeof(struct nrc_node_serial_out));

        if (self != NULL) {
            // Future additions to node API will be set to NULL until supported by node
            memset(&_api, 0, sizeof(struct nrc_node_api));
            _api.init = nrc_node_serial_out_init;
            _api.deinit = nrc_node_serial_out_deinit;
            _api.start = nrc_node_serial_out_start;
            _api.stop = nrc_node_serial_out_stop;
            _api.recv_msg = nrc_node_serial_out_recv_msg;
            _api.recv_evt = nrc_node_serial_out_recv_evt;

            pars->api = &_api; // Return api functions for the node

            memset(self, 0, sizeof(struct nrc_node_serial_out));

            // Mandatory for all nodes
            self->hdr.cfg_id = pars->cfg_id;
            self->hdr.cfg_type = pars->cfg_type;
            self->hdr.cfg_name = pars->cfg_name;

            // Node specific
            self->writer.write_complete_evt = NRC_N_SERIAL_OUT_EVT_WRITE_COMPLETE;
            self->writer.error_evt = NRC_N_SERIAL_OUT_EVT_ERROR;
            self->writer.node = self;
            self->max_buf_size = NRC_N_SERIAL_OUT_MAX_BUF_SIZE; // Default message buf size
            self->type = NRC_N_SERIAL_OUT_TYPE; // For node pointer validity check

            self->state = NRC_N_SERIAL_OUT_S_CREATED;

            NRC_LOGI(_tag, "create(%s): ok", pars->cfg_id);
        }
        else {
            NRC_LOGE(_tag, "create(%s): out of mem", pars->cfg_id);
        }
    }
    else {
        NRC_LOGE(_tag, "create: invalid in parameter");
    }

    return self;
}

// Initialize the node; allocate memory (if any); ready to start
static s32_t nrc_node_serial_out_init(nrc_node_t slf)
{
    struct nrc_node_serial_out   *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
 
    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_CREATED:
            result = nrc_serial_init();

            if (OK(result)) {
                // Read topic from configuration
                result = nrc_cfg_get_str(self->hdr.cfg_id, "topic", &self->topic);
                if (OK(result)) {
                    // Get cfg id of serial-port configuration node
                    result = nrc_cfg_get_str(self->hdr.cfg_id, "serial-port", &self->cfg_serial_id);
                }
                if (OK(result)) {
                    // Get node priority
                    s32_t prio;
                    result = nrc_cfg_get_int(self->hdr.cfg_id, "priority", &prio);
                    if (OK(result)) {
                        if ((prio >= S8_MIN_VALUE) && (prio <= S8_MAX_VALUE)) {
                            self->prio = (s8_t)prio;
                            self->writer.prio = self->prio;
                        }
                        else {
                            result = NRC_R_INVALID_CFG;
                        }
                    }
                }
            }

            if (OK(result)) {
                self->state = NRC_N_SERIAL_OUT_S_INITIALISED;
                NRC_LOGI(_tag, "init(%s): ok", self->hdr.cfg_id);
            }
            else {
                self->state = NRC_N_SERIAL_OUT_S_ERROR;
                NRC_LOGE(_tag, "init(%s): error %d", self->hdr.cfg_id, result);
            }
            break;

        case NRC_N_SERIAL_OUT_S_INITIALISED:
        case NRC_N_SERIAL_OUT_S_STARTED:
            // If init is called a second time it means there may be more wires to readout (if needed)
            result = NRC_R_OK;
            NRC_LOGI(_tag, "init(%s): ok", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "init(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_OUT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "init: invalid in parameter");
    }

    return result;
}

static s32_t nrc_node_serial_out_deinit(nrc_node_t slf)
{
    struct nrc_node_serial_out   *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
 
    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_INITIALISED:
        case NRC_N_SERIAL_OUT_S_ERROR:
            // Free allocated memory (if any)

            self->state = NRC_N_SERIAL_OUT_S_CREATED;
            NRC_LOGI(_tag, "deinit(%s): ok", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "deinit(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_OUT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "deinit: invalid in parameter");
    }

    return result;
}

static s32_t nrc_node_serial_out_start(nrc_node_t slf)
{
    struct nrc_node_serial_out  *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
 
    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_INITIALISED:
            result = nrc_serial_open_writer(self->cfg_serial_id, self->writer, &self->serial);

            if (OK(result)) {
                self->state = NRC_N_SERIAL_OUT_S_STARTED;
                NRC_LOGI(_tag, "start(%s): OK", self->hdr.cfg_id);
            }
            else {
                self->state = NRC_N_SERIAL_OUT_S_ERROR;
                NRC_LOGE(_tag, "start(%s): could not open serial %s", self->hdr.cfg_id, self->cfg_serial_id);
            }
            break;

        case NRC_N_SERIAL_OUT_S_STARTED:
            NRC_LOGI(_tag, "start(%d): already started", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "start(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_OUT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "start: invalid in parameter");
    }

    return result;
}

static s32_t nrc_node_serial_out_stop(nrc_node_t slf)
{
    struct nrc_node_serial_out   *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_STARTED:
            // Stop any ongoing activites, free memory allocated in the start state
            result = nrc_serial_close_writer(self->serial);
            self->serial = NULL;

            // No memory to free

            self->state = NRC_N_SERIAL_OUT_S_INITIALISED;
            result = NRC_R_OK;

            NRC_LOGI(_tag, "stop(%s): result ", self->hdr.cfg_id, result);
            break;

        case NRC_N_SERIAL_OUT_S_INITIALISED:
            NRC_LOGI(_tag, "stop(%d): already stopped", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "stop(%s): invalid state", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_OUT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "stop: invalid in parameter");
    }

    return result;
}

static s32_t nrc_node_serial_out_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_serial_out   *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
    struct nrc_msg_hdr          *msg_hdr = (struct nrc_msg_hdr*)msg;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE) && (msg_hdr != NULL)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_STARTED:
            result = write_data(self, msg_hdr);

            //TODO:
            
            NRC_LOGV(_tag, "recv_msg(%s): type %d", self->hdr.cfg_id, msg_hdr->type);   
            break;
        default:
            nrc_os_msg_free(msg);
            NRC_LOGW(_tag, "recv_msg(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        nrc_os_msg_free(msg);
        NRC_LOGE(_tag, "recv_msg: invalid in parameter");
    }

    return result;
}

static s32_t nrc_node_serial_out_recv_evt(nrc_node_t slf, u32_t event_mask)
{
    struct nrc_node_serial_out  *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_STARTED:
            if ((event_mask | NRC_N_SERIAL_OUT_EVT_WRITE_COMPLETE) != 0) {
                result = write_data(self, NULL);

                //TODO:
            }
            if ((event_mask | NRC_N_SERIAL_OUT_EVT_ERROR) != 0) {
                NRC_LOGW(_tag, "recv_evt(%s): serial error %d", self->hdr.cfg_id, nrc_serial_get_write_error(self->serial));
            }
            break;

        default:
            NRC_LOGW(_tag, "recv_evt(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "recv_evt: invalid in parameter");
    }

    return result;
}

static s32_t write_data(struct nrc_node_serial_out *self, struct nrc_msg_hdr *msg_hdr)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT(self != NULL);
    
    switch (msg_hdr->type) {
    case NRC_MSG_TYPE_BUF:
        break;
    case NRC_MSG_TYPE_DATA_AVAILABLE:
        break;
    default:
        nrc_os_msg_free(msg_hdr);
        NRC_LOGW(_tag, "write_data(%s): unknown msg %d", self->hdr.cfg_id, msg_hdr->type);
        break;
    }

    return result;
}