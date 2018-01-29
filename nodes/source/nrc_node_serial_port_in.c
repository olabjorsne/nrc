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

// Default max buffer read size
#define NRC_N_SERIAL_IN_MAX_BUF_SIZE (512)

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
    const s8_t                      *cfg_id;    // Node cfg id

    const s8_t                      *topic;         // Node topic read from from cfg
    const s8_t                      *cfg_serial_id; // serial-port id read from cfg

    enum nrc_node_serial_in_state   state;      // Node state
    enum nrc_node_serial_in_msg     msg_type;   // Msg type to send; read from cfg
    nrc_serial_t                    serial;     // NRC serial port
    struct nrc_serial_reader        reader;     // Reader notification
    u32_t                           max_buf_size; // Max buf size of buf msg

    s8_t                            prio;       // Node prio; use for sending messages

    u32_t                           type;       // Type check; unique number for every object type
};

// Node create function registered in node factory with type string
static nrc_node_t nrc_node_serial_in_create(struct nrc_node_factory_pars *pars);

// Node api functions
static s32_t nrc_node_serial_in_init(nrc_node_t self);
static s32_t nrc_node_serial_in_deinit(nrc_node_t self);
static s32_t nrc_node_serial_in_start(nrc_node_t self);
static s32_t nrc_node_serial_in_stop(nrc_node_t self);
static s32_t nrc_node_serial_in_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_serial_in_recv_evt(nrc_node_t self, u32_t event_mask);

// Internal functions
static u32_t read_data(void *node, u8_t *buf, u32_t buf_size);
static s32_t send_data(nrc_node_t self);

const static s8_t*          _tag = "serial-in"; // Used in NRC_LOG function
static struct nrc_node_api  _api;               // Node API functions

// Initial call to register node in node factory with its type
void nrc_node_serial_in_register(void)
{
    s32_t status = nrc_factory_register("serial-in", nrc_node_serial_in_create);
    if (!OK(status)) {
        NRC_LOGE(_tag, "Registration to factory failed");
    }
}

// Node constructor function
nrc_node_t nrc_node_serial_in_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_serial_in *self = NULL;

    if ((pars != NULL) && (strcmp("serial-in", pars->cfg_type) == 0)) {
        self = (struct nrc_node_serial_in*)nrc_os_node_alloc(sizeof(struct nrc_node_serial_in));

        if (self != NULL) {
            // Future additions to node API will be set to NULL until supported by node
            memset(&_api, 0, sizeof(struct nrc_node_api));
            _api.init = nrc_node_serial_in_init;
            _api.deinit = nrc_node_serial_in_deinit;
            _api.start = nrc_node_serial_in_start;
            _api.stop = nrc_node_serial_in_stop;
            _api.recv_msg = nrc_node_serial_in_recv_msg;
            _api.recv_evt = nrc_node_serial_in_recv_evt;

            pars->api = &_api; // Return api functions for the node

            memset(self, 0, sizeof(struct nrc_node_serial_in));

            // Mandatory for all nodes
            self->hdr.cfg_id = pars->cfg_id;
            self->hdr.cfg_type = pars->cfg_type;
            self->hdr.cfg_name = pars->cfg_name;

            // Node specific
            self->reader.data_available_evt = NRC_N_SERIAL_IN_EVT_DATA_AVAIL;
            self->reader.error_evt = NRC_N_SERIAL_IN_EVT_DATA_AVAIL;
            self->reader.node = self;
            self->max_buf_size = NRC_N_SERIAL_IN_MAX_BUF_SIZE; //TODO: configurable
            self->type = NRC_N_SERIAL_IN_TYPE; // For node pointer validity check

            self->state = NRC_N_SERIAL_IN_S_CREATED;

            NRC_LOGI(_tag, "create(%s): OK", pars->cfg_id);
        }
        else {
            NRC_LOGE(_tag, "create(%s): Out of memory", pars->cfg_id);
        }
    }
    else {
        NRC_LOGE(_tag, "create: Invalid in parameters");
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
                // Read topic from configuration
                result = nrc_cfg_get_str(self->hdr.cfg_id, "topic", &self->topic);
                if (OK(result)) {
                    // Get cfg id of serial-port configuration node
                    result = nrc_cfg_get_str(self->hdr.cfg_id, "serial-port", &self->cfg_serial_id);
                }
                if (OK(result)) {
                    // Get msg type to send; data available or byte array
                    s8_t *cfg_msg_type = NULL;
                    result = nrc_cfg_get_str(self->hdr.cfg_id, "msgtype", &cfg_msg_type);

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
                self->state = NRC_N_SERIAL_IN_S_ERROR;
                NRC_LOGE(_tag, "init(%s): Error %d", self->hdr.cfg_id, result);
            }
            break;

        case NRC_N_SERIAL_IN_S_INITIALISED:
        case NRC_N_SERIAL_IN_S_STARTED:
            // If init is called a second time it means there may be more wires to readout (if needed)
            result = NRC_R_OK;
            NRC_LOGI(_tag, "init(%s): OK", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "init(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "init: Invalid in parameter");
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
            // Free memory allocated in the init function (if any)

            self->state = NRC_N_SERIAL_IN_S_CREATED;
            NRC_LOGI(_tag, "deinit(%s): OK", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "deinit(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "deinit: Invalid in parameter");
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
                NRC_LOGE(_tag, "start(%s): Could not open serial port", self->hdr.cfg_id);
            }
            break;

        default:
            NRC_LOGE(_tag, "start(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "start: Invalid in parameter");
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

            NRC_LOGI(_tag, "stop(%s): OK", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "stop(%s): Invalid state", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "stop: Invalid in parameter");
    }

    return result;
}

static s32_t nrc_node_serial_in_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_IN_S_STARTED:
            // Should not receive messages (other than for simulation/tests).
            // Forward message to wires
            NRC_LOGV(_tag, "recv_msg(%s)", self->hdr.cfg_id);
            result = nrc_os_send_msg_from(self, msg, self->prio);    
            break;
        default:
            nrc_os_msg_free(msg);
            NRC_LOGW(_tag, "recv_msg(%s): Invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        nrc_os_msg_free(msg);
        NRC_LOGE(_tag, "recv_msg: Invalid in parameter");
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
            if ((event_mask | NRC_N_SERIAL_IN_EVT_DATA_AVAIL) != 0) {
                result = send_data(self);
            }
            if ((event_mask | NRC_N_SERIAL_IN_EVT_ERROR) != 0) {
                NRC_LOGW(_tag, "recv_evt(%s): Serial error %d", self->hdr.cfg_id, nrc_serial_get_read_error(self->serial));
            }
            break;

        default:
            NRC_LOGW(_tag, "recv_evt(%s): Invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "recv_evt: Invalid in parameter");
    }

    return result;
}

static s32_t send_data(struct nrc_node_serial_in *self)
{
    s32_t result = NRC_R_OK;

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
        else {
            result = NRC_R_OUT_OF_MEM;
        }
    }
    else {
        struct nrc_msg_buf  *msg = NULL;
        u32_t               bytes_to_read = nrc_serial_get_bytes(self->serial);

        while(bytes_to_read > 0) {
            if (bytes_to_read > self->max_buf_size) {
                bytes_to_read = self->max_buf_size;
            }

            msg = (struct nrc_msg_buf*)nrc_os_msg_alloc(sizeof(struct nrc_msg_buf) + bytes_to_read);
            if (msg != NULL) {
                msg->hdr.next = NULL;
                msg->hdr.topic = self->topic;
                msg->hdr.type = NRC_MSG_TYPE_BUF;

                msg->buf_size = nrc_serial_read(self->serial, msg->buf, bytes_to_read);

                result = nrc_os_send_msg_from(self, msg, self->prio);

                bytes_to_read = nrc_serial_get_bytes(self->serial);
            }
            else {
                // No memory left; clear data
                nrc_serial_clear(self->serial);
                NRC_LOGW(_tag, "send_data(%s): Out of memory", self->hdr.cfg_id);
                result = NRC_R_OUT_OF_MEM;
            }
        }
    }

    return result;
}

static u32_t read_data(void *slf, u8_t *buf, u32_t buf_size)
{
    u32_t                       bytes_read = 0;
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE) && (self->state == NRC_N_SERIAL_IN_S_STARTED)) {
        bytes_read = nrc_serial_read(self->serial, buf, buf_size);
    }

    return bytes_read;
}
