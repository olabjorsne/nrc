#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_node_factory.h"

#include "nrc_serial.h"

#include <string.h>
#include <assert.h>

// Event bitmask for nrc callbacks
#define NRC_N_SERIAL_IN_EVT_DATA_AVAIL  (1)
#define NRC_N_SERIAL_IN_EVT_ERROR       (2)

// Type number to check that node pointer is of correct type
#define NRC_N_SERIAL_IN_TYPE            (0x18FF346A)

// Node states
enum nrc_node_serial_in_state {
    NRC_N_SERIAL_IN_S_INVALID = 0,      
    NRC_N_SERIAL_IN_S_CREATED,      // Created and deinitialised or not yet initialised. No memory allocated.
    NRC_N_SERIAL_IN_S_INITIALISED,  // Initialised and memory allocated. Ready to be started.
    NRC_N_SERIAL_IN_S_STARTED,      // Node is now running.
    NRC_N_SERIAL_IN_S_ERROR         // Error occurred which node cannot recover from. Allows other nodes to run.
};

enum nrc_node_serial_in_msg {
    NRC_N_SERIAL_IN_MSG_DATA_AVAIL = 1, // Send data available messages to wires
    NRC_N_SERIAL_IN_MSG_BYTE_ARRAY      // Send byte array messages to wires
};

struct nrc_node_serial_in {
    struct nrc_node_hdr             hdr;        // General node header (mandatory)
    const s8_t                      *topic;     // Node topic from cfg
    const s8_t                      *cfg_id;    // Node id from cfg

    const s8_t                      *cfg_serial_id; // serial-port id from cfg

    enum nrc_node_serial_in_state   state;      // Node state
    enum nrc_node_serial_in_msg     msg_type;   // Type of message to send to wires
    nrc_serial_t                    serial;     // NRC serial port
    struct nrc_serial_reader        reader;     // Reader notification

    s8_t                            prio;

    u32_t                           type;
};

static nrc_node_t nrc_node_serial_in_create(struct nrc_node_factory_pars *pars);

static s32_t nrc_node_serial_in_init(nrc_node_t self);
static s32_t nrc_node_serial_in_deinit(nrc_node_t self);
static s32_t nrc_node_serial_in_start(nrc_node_t self);
static s32_t nrc_node_serial_in_stop(nrc_node_t self);
static s32_t nrc_node_serial_in_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_serial_in_recv_evt(nrc_node_t self, u32_t event_mask);

static u32_t read_data(void *node, u8_t *buf, u32_t buf_size); // For data_available callback
static s32_t send_data(nrc_node_t self);

const static s8_t*          _tag = "serial-in";
static struct nrc_node_api  _api;

void nrc_node_serial_in_register(void)
{
    s32_t status = nrc_factory_register("serial-in", nrc_node_serial_in_create);
    if (!OK(status)) {
        NRC_LOGE(_tag, "Registration to factory failed");
    }
}

nrc_node_t nrc_node_serial_in_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_serial_in *self = NULL;

    if ((pars != NULL) && (strcmp("serial-in", pars->cfg_type) == 0)) {
        self = (struct nrc_node_serial_in*)nrc_os_node_alloc(sizeof(struct nrc_node_serial_in));

        if (self != NULL) {
            memset(&_api, 0, sizeof(struct nrc_node_api));
            _api.init = nrc_node_serial_in_init;
            _api.deinit = nrc_node_serial_in_deinit;
            _api.start = nrc_node_serial_in_start;
            _api.stop = nrc_node_serial_in_stop;
            _api.recv_msg = nrc_node_serial_in_recv_msg;
            _api.recv_evt = nrc_node_serial_in_recv_evt;

            pars->api = &_api; // Return api functions for the node

            memset(self, 0, sizeof(struct nrc_node_serial_in));
            self->hdr.cfg_id = pars->cfg_id;
            self->hdr.cfg_type = pars->cfg_type;
            self->hdr.cfg_name = pars->cfg_name;
            self->type = NRC_N_SERIAL_IN_TYPE;
            self->reader.data_available_evt = NRC_N_SERIAL_IN_EVT_DATA_AVAIL;
            self->reader.error_evt = NRC_N_SERIAL_IN_EVT_DATA_AVAIL;
            self->reader.node = self;
            self->state = NRC_N_SERIAL_IN_S_CREATED;
        }
        else {
            NRC_LOGE(_tag, "create(%s): Out of memory", pars->cfg_id);
        }
    }

    return self;
}

static s32_t nrc_node_serial_in_init(nrc_node_t slf)
{
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
 
    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_IN_S_CREATED:
            result = nrc_serial_init();

            if (OK(result)) {
                result = nrc_cfg_get_str(curr_config, self->hdr.cfg_id, "topic", &self->topic);
                if (OK(result)) {
                    // Get id of serial-port configuration node
                    result = nrc_cfg_get_str(curr_config, self->hdr.cfg_id, "serial-port", &self->cfg_serial_id);
                }
                if (OK(result)) {
                    // Get msg type to send when there is data
                    s8_t *cfg_msg_type = NULL;
                    result = nrc_cfg_get_str(curr_config, self->hdr.cfg_id, "msgtype", &cfg_msg_type);

                    if (OK(result)) {
                        if (strcmp(cfg_msg_type, "dataavailable") == 0) {
                            self->msg_type = NRC_N_SERIAL_IN_MSG_DATA_AVAIL;
                        }
                        else {
                            self->msg_type = NRC_N_SERIAL_IN_MSG_BYTE_ARRAY;
                        }
                    }
                }
            }

            if (OK(result)) {
                self->state = NRC_N_SERIAL_IN_S_INITIALISED;
                NRC_LOGI(_tag, "init(%s): OK", self->hdr.cfg_id);
            }
            else {
                NRC_LOGW(_tag, "init(%s): Error %d", self->hdr.cfg_id, result);
            }
            break;

        case NRC_N_SERIAL_IN_S_INITIALISED:
        case NRC_N_SERIAL_IN_S_STARTED:
            // If init is called a second time it means there may be more wires to readout
            break;

        default:
            NRC_LOGW(_tag, "init(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_serial_in_deinit(nrc_node_t slf)
{
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_IN_S_INITIALISED:
        case NRC_N_SERIAL_IN_S_ERROR:
            // Free memory allocated in the init function

            self->state = NRC_N_SERIAL_IN_S_CREATED;
            break;

        default:
            NRC_LOGW(_tag, "deinit(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_serial_in_start(nrc_node_t slf)
{
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
 
    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_IN_S_INITIALISED:
            result = nrc_serial_open_reader(self->cfg_serial_id, self->reader, &self->serial);

            if (OK(result)) {
                self->state = NRC_N_SERIAL_IN_S_STARTED;
                NRC_LOGI(_tag, "start(%s): OK", self->hdr.cfg_id);

                // Check if there is already data to read
                if (nrc_serial_get_bytes(self->serial) > 0) {
                    result = send_data(self);
                }
            }
            else {
                self->state = NRC_N_SERIAL_IN_S_ERROR;
                NRC_LOGW(_tag, "start(%s): Could not open serial port", self->hdr.cfg_id);
            }
            break;

        default:
            NRC_LOGD("inject", "start: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_serial_in_stop(nrc_node_t slf)
{
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_IN_S_STARTED:
            // Stop any ongoing activites, free stored messages, etc..
            result = nrc_serial_close_reader(self->serial);
            self->serial = NULL;

            self->state = NRC_N_SERIAL_IN_S_INITIALISED;
            result = NRC_R_OK;

            NRC_LOGD("inject", "stopped");
            break;

        default:
            NRC_LOGD("inject", "stop: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_serial_in_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE)) {
        // Forward message as is to wires
        result = nrc_os_send_msg_from(self, msg, self->prio);
    }
    else if (msg != NULL) {
        nrc_os_msg_free(msg);
    }

    return result;
}

static s32_t nrc_node_serial_in_recv_evt(nrc_node_t slf, u32_t event_mask)
{
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_IN_S_STARTED:
            break;

        default:
            NRC_LOGW(_tag, "recv_evt: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static u32_t read_data(void *node, u8_t *buf, u32_t buf_size)
{
    u32_t bytes_read = 0;

    return bytes_read;
}

static s32_t send_data(struct nrc_node_serial_in *self)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    assert(self != NULL);
    
    if (self->msg_type == NRC_N_SERIAL_IN_MSG_DATA_AVAIL) {
        struct nrc_msg_data_available *msg = NULL;

        msg = (struct nrc_msg_data_available*)nrc_os_msg_alloc(sizeof(struct nrc_msg_data_available));
        if (msg != NULL) {
            msg->hdr.next = NULL;
            msg->hdr.topic = self->topic;
            msg->hdr.type = NRC_MSG_TYPE_DATA_AVAILABLE;

            msg->node = self;
            msg->read = read_data;

            result = nrc_os_send_msg_from(self, msg, self->prio);
        }
    }
    else {
    }

    return result;
}

/*
static s32_t handle_data(struct nrc_node_serial_in *self)
{
    s32_t   result = NRC_R_INVALID_IN_PARAM;
    s32_t   i;
    u32_t   bytes = 0;

    if ((self != 0) && (self->type == NRC_N_SERIAL_IN_TYPE)) {

        bytes = nrc_serial_get_bytes(self->serial);
        
        if(bytes > 0) {

        if (self->msg_type == NRC_N_SERIAL_IN_MSG_DATA_AVAIL) {
            struct nrc_msg_data_available *msg = NULL;

            msg = (struct nrc_msg_data_available*)nrc_os_msg_alloc(sizeof(struct nrc_msg_data_available));
            if (msg != NULL) {
                msg->hdr.next = NULL;
                msg->hdr.topic = self->topic;
                msg->hdr.type = NRC_MSG_TYPE_DATA_AVAILABLE;

                msg->node = self;
                msg->read = read_data;

                result = send_data(msg);
            }
            else {
                result = NRC_R_OUT_OF_MEM;
            }
        }
        else {

        }
    }
}
*/
