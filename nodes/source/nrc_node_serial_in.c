#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_factory.h"
#include "nrc_assert.h"
#include "nrc_serial.h"

#include <string.h>

// Type number to check that node pointer is of correct type
#define NRC_N_SERIAL_IN_TYPE            (0x18FF346A)

// Event bitmask for nrc_serial callbacks
#define NRC_N_SERIAL_IN_EVT_DATA_AVAIL  (1)
#define NRC_N_SERIAL_IN_EVT_ERROR       (2)

// Default max buffer read size
#define NRC_N_SERIAL_IN_MAX_BUF_SIZE (256)

// Node states
enum nrc_node_serial_in_state {
    NRC_N_SERIAL_IN_S_INVALID = 0,      
    NRC_N_SERIAL_IN_S_CREATED,      // Created but not yet initialised. No memory allocated except the object itself.
    NRC_N_SERIAL_IN_S_INITIALISED,  // Initialised and memory allocated. Ready to be started.
    NRC_N_SERIAL_IN_S_STARTED,      // Resources is started and node is now running.
    NRC_N_SERIAL_IN_S_ERROR         // Error occurred which node cannot recover from. Other nodes can continue to run.
};

enum nrc_node_serial_in_msg {
    NRC_N_SERIAL_IN_MSG_DATA_AVAIL = 1, // Send data available messages to wires
    NRC_N_SERIAL_IN_MSG_BYTE_ARRAY      // Send byte array messages to wires
};

// Serial-in node structure
struct nrc_node_serial_in {
    struct nrc_node_hdr             hdr;            // General node header; from create function in pars

    const s8_t                      *topic;         // Node topic from cfg
    const s8_t                      *cfg_serial_id; // serial (configuration) node id from cfg
    s8_t                            prio;           // Node prio; used for sending messages; from cfg
    enum nrc_node_serial_in_msg     msg_type;       // Msg type to send; from cfg
    u32_t                           max_buf_size;   // Max buf size for buf messages

    enum nrc_node_serial_in_state   state;          // Node state
    nrc_serial_t                    serial;         // NRC serial port
    struct nrc_serial_reader        reader;         // Reader notification data for nrc_serial callback evt

    u32_t                           type;           // Object type check; unique number for every object type
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

// Function sent in data available message; Called by reader of the data
static u32_t read_data(void *self, u8_t *buf, u32_t buf_size);

// Internal functions
static s32_t send_data(struct nrc_node_serial_in *self);

// Internal variables
const static s8_t*          _tag = "serial-in"; // Used in NRC_LOG function
static struct nrc_node_api  _api;               // Node API functions

// Register node to node factory; called at system start
void nrc_node_serial_in_register(void)
{
    s32_t result = nrc_factory_register_node_type("nrc-serial-in", nrc_node_serial_in_create);
    if (!OK(result)) {
        NRC_LOGE(_tag, "register: error %d", result);
    }
}

// Node constructor function
nrc_node_t nrc_node_serial_in_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_serial_in *self = NULL;

    if ((pars != NULL) && (strcmp("nrc-serial-in", pars->cfg_type) == 0)) {
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
            self->reader.error_evt = NRC_N_SERIAL_IN_EVT_ERROR;
            self->reader.node = self;
            self->msg_type = NRC_N_SERIAL_IN_MSG_BYTE_ARRAY;    // Default message type
            self->max_buf_size = NRC_N_SERIAL_IN_MAX_BUF_SIZE;  // Default message buf size

            // Object type check
            self->type = NRC_N_SERIAL_IN_TYPE; // For node pointer validity check

            self->state = NRC_N_SERIAL_IN_S_CREATED;

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

// Read configuration; allocate memory (if any); initialize resources (if any); 
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
            }
            if (OK(result)) {
                // Get cfg id of serial configuration node
                result = nrc_cfg_get_str(self->hdr.cfg_id, "serial", &self->cfg_serial_id);
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

                        result = nrc_cfg_get_int(self->hdr.cfg_id, "bufsize", &self->max_buf_size);
                    }
                }
            }
            if (OK(result)) {
                // Get node priority
                s32_t prio;
                result = nrc_cfg_get_int(self->hdr.cfg_id, "priority", &prio);
                if (OK(result)) {
                    if ((prio >= S8_MIN_VALUE) && (prio <= S8_MAX_VALUE)) {
                        self->prio = (s8_t)prio;
                        self->reader.prio = self->prio;
                    }
                    else {
                        result = NRC_R_INVALID_CFG;
                    }
                }
            }

            if (OK(result)) {
                self->state = NRC_N_SERIAL_IN_S_INITIALISED;
            }
            else {
                self->state = NRC_N_SERIAL_IN_S_ERROR;
                NRC_LOGE(_tag, "init(%s): error %d", self->hdr.cfg_id, result);
            }
            break;

        default:
            NRC_LOGE(_tag, "init(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
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
            // Free allocated memory (if any)

            self->state = NRC_N_SERIAL_IN_S_CREATED;
            break;

        default:
            NRC_LOGE(_tag, "deinit(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
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

                // Check if there is already data to read
                if (nrc_serial_get_bytes(self->serial) > 0) {
                    result = send_data(self);
                }
            }
            else {
                self->state = NRC_N_SERIAL_IN_S_ERROR;
                NRC_LOGE(_tag, "start(%s): could not open serial %s", self->hdr.cfg_id, self->cfg_serial_id);
            }
            break;

        case NRC_N_SERIAL_IN_S_STARTED:
            NRC_LOGW(_tag, "start(%d): already started", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "start(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
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
            // Stop any ongoing activites, free memory allocated in the start state
            result = nrc_serial_close_reader(self->serial);
            self->serial = NULL;

            // No memory to free

            self->state = NRC_N_SERIAL_IN_S_INITIALISED;
            result = NRC_R_OK;
            break;

        case NRC_N_SERIAL_IN_S_INITIALISED:
            NRC_LOGW(_tag, "stop(%d): already stopped", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "stop(%s): invalid state", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_IN_S_ERROR;
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
        switch (self->state) {
        case NRC_N_SERIAL_IN_S_STARTED:
            // Should not receive messages (other than for simulation/tests).
            // Forward message to wires
            NRC_LOGV(_tag, "recv_msg(%s)", self->hdr.cfg_id);
            result = nrc_os_send_msg_from(self, msg, self->prio);    
            break;
        default:
            nrc_os_msg_free(msg);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
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
            if ((event_mask & NRC_N_SERIAL_IN_EVT_DATA_AVAIL) != 0) {
                result = send_data(self);
            }
            if ((event_mask & NRC_N_SERIAL_IN_EVT_ERROR) != 0) {
                NRC_LOGW(_tag, "recv_evt(%s): serial error %d", self->hdr.cfg_id, nrc_serial_get_read_error(self->serial));
            }
            break;

        default:
            NRC_LOGW(_tag, "recv_evt(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t send_data(struct nrc_node_serial_in *self)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT(self != NULL);
    
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
                result = NRC_R_OUT_OF_MEM;
                bytes_to_read = 0;
            }
        }
    }

    return result;
}

static u32_t read_data(void *slf, u8_t *buf, u32_t buf_size)
{
    u32_t                       bytes_read = 0;
    struct nrc_node_serial_in   *self = (struct nrc_node_serial_in*)slf;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_IN_TYPE) &&
        (self->state == NRC_N_SERIAL_IN_S_STARTED)) {

        bytes_read = nrc_serial_read(self->serial, buf, buf_size);
    }

    return bytes_read;
}
