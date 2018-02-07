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
#define NRC_N_SERIAL_OUT_TYPE            (0x01432FD2)

// Event bitmask for nrc callbacks
#define NRC_N_SERIAL_OUT_EVT_WRITE_COMPLETE  (1)
#define NRC_N_SERIAL_OUT_EVT_ERROR           (2)

// Default max buffer read size
#define NRC_N_SERIAL_OUT_MAX_BUF_SIZE (256)

// Node states
enum nrc_node_serial_out_state {
    NRC_N_SERIAL_OUT_S_INVALID = 0,      
    NRC_N_SERIAL_OUT_S_CREATED,                 // Created but not yet initialised. No memory allocated except the object itself.
    NRC_N_SERIAL_OUT_S_INITIALISED,             // Initialised and memory allocated. Ready to be started.
    NRC_N_SERIAL_OUT_S_STARTED,                 // Resources is started and node is now running.
    NRC_N_SERIAL_OUT_S_STARTED_TX_BUF,          // Resources is started and node is now running with outstanding write
    NRC_N_SERIAL_OUT_S_STARTED_TX_DATA_AVAIL,   // Resources is started and node is now running with outstanding write
    NRC_N_SERIAL_OUT_S_ERROR                    // Error occurred which node cannot recover from. Other nodes can continue to run.
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

    struct nrc_msg_buf              *msg_buf;       // Message for outstanding write

    nrc_node_t                      read_node;      // Node to read from
    nrc_msg_read_t                  read_fcn;       // Read function of data available msg
    u8_t                            *data_avail_buf;// Buffer for data available writes

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
static s32_t write_msg_buf(struct nrc_node_serial_out *self, struct nrc_msg_buf *msg);
static s32_t write_msg_data_avail(struct nrc_node_serial_out *self, struct nrc_msg_data_available *msg);

// Internal variables
const static s8_t*          _tag = "serial-out"; // Used in NRC_LOG function
static struct nrc_node_api  _api;                // Node API functions

// Register node to node factory; called at system start
void nrc_node_serial_out_register(void)
{
    s32_t result = nrc_factory_register_node_type("nrc-serial-out", nrc_node_serial_out_create);
    if (!OK(result)) {
        NRC_LOGE(_tag, "register: error %d", result);
    }
}

// Node constructor function
nrc_node_t nrc_node_serial_out_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_serial_out *self = NULL;

    if ((pars != NULL) && (strcmp("nrc-serial-out", pars->cfg_type) == 0)) {
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
            }
            if (OK(result)) {
                // Get cfg id of serial-port configuration node
                result = nrc_cfg_get_str(self->hdr.cfg_id, "serial", &self->cfg_serial_id);
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
            if (OK(result)) {
                result = nrc_cfg_get_int(self->hdr.cfg_id, "bufsize", &self->max_buf_size);
            }

            if (OK(result)) {
                self->data_avail_buf = (u8_t*)nrc_port_heap_alloc(self->max_buf_size);
                NRC_ASSERT(self->data_avail_buf != NULL);

                self->state = NRC_N_SERIAL_OUT_S_INITIALISED;
                NRC_LOGI(_tag, "init(%s): ok", self->hdr.cfg_id);
            }
            else {
                self->state = NRC_N_SERIAL_OUT_S_ERROR;
                NRC_LOGE(_tag, "init(%s): error %d", self->hdr.cfg_id, result);
            }
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
    struct nrc_node_serial_out  *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
 
    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_INITIALISED:
        case NRC_N_SERIAL_OUT_S_ERROR:
            // Free allocated memory (if any)
            nrc_port_heap_free(self->data_avail_buf);
            self->data_avail_buf = NULL;

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
        case NRC_N_SERIAL_OUT_S_STARTED_TX_BUF:
        case NRC_N_SERIAL_OUT_S_STARTED_TX_DATA_AVAIL:
            NRC_LOGW(_tag, "start(%d): already started", self->hdr.cfg_id);
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
        case NRC_N_SERIAL_OUT_S_STARTED_TX_BUF:
        case NRC_N_SERIAL_OUT_S_STARTED_TX_DATA_AVAIL:
            // TODO: cancel writing
            // Fall through ??

        case NRC_N_SERIAL_OUT_S_STARTED:
            // Stop any ongoing activites, free memory allocated in the start state
            result = nrc_serial_close_writer(self->serial);
            self->serial = NULL;

            if (self->msg_buf != NULL) {
                nrc_port_heap_free(self->msg_buf);
                self->msg_buf = NULL;
            }

            self->state = NRC_N_SERIAL_OUT_S_INITIALISED;
            result = NRC_R_OK;

            NRC_LOGI(_tag, "stop(%s): ok ", self->hdr.cfg_id);
            break;

        case NRC_N_SERIAL_OUT_S_INITIALISED:
            NRC_LOGW(_tag, "stop(%d): already stopped", self->hdr.cfg_id);
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
    struct nrc_node_serial_out  *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;
    struct nrc_msg_hdr          *msg_hdr = (struct nrc_msg_hdr*)msg;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE) && (msg_hdr != NULL)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_STARTED:
            if (msg_hdr->type == NRC_MSG_TYPE_BUF) {
                result = write_msg_buf(self, (struct nrc_msg_buf*)msg_hdr);
            }
            else if (msg_hdr->type == NRC_MSG_TYPE_DATA_AVAILABLE) {
                result = write_msg_data_avail(self, (struct nrc_msg_data_available*)msg_hdr);
            }
            else {
                NRC_LOGW(_tag, "recv_msg(%s): unknown msg type %d", self->hdr.cfg_id, msg_hdr->type);
                nrc_os_msg_free(msg);
            }
            break;

        case NRC_N_SERIAL_OUT_S_STARTED_TX_BUF:
        case NRC_N_SERIAL_OUT_S_STARTED_TX_DATA_AVAIL:
            nrc_os_msg_free(msg);
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
        case NRC_N_SERIAL_OUT_S_STARTED_TX_BUF:
            if ((event_mask & NRC_N_SERIAL_OUT_EVT_WRITE_COMPLETE) != 0) {
                struct nrc_msg_buf *msg = (struct nrc_msg_buf*)self->msg_buf->hdr.next;

                self->msg_buf->hdr.next = NULL; // If there are linked messages, do not free them
                nrc_os_msg_free(self->msg_buf);
                self->msg_buf = NULL;

                self->state = NRC_N_SERIAL_OUT_S_STARTED;

                if (msg != NULL) {
                    result = write_msg_buf(self, msg);
                } 
            }
            if ((event_mask & NRC_N_SERIAL_OUT_EVT_ERROR) != 0) {
                NRC_LOGW(_tag, "recv_evt(%s): serial error %d", self->hdr.cfg_id, nrc_serial_get_write_error(self->serial));
            }
            break;

        case NRC_N_SERIAL_OUT_S_STARTED_TX_DATA_AVAIL:
            NRC_ASSERT(self->data_avail_buf != NULL);

            if ((event_mask & NRC_N_SERIAL_OUT_EVT_WRITE_COMPLETE) != 0) {
                self->state = NRC_N_SERIAL_OUT_S_STARTED;
                if (nrc_serial_get_bytes(self->serial) > 0) {
                    result = write_msg_data_avail(self, NULL); // Continue to read and write
                }
            }
            if ((event_mask & NRC_N_SERIAL_OUT_EVT_ERROR) != 0) {
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

static s32_t write_msg_buf(struct nrc_node_serial_out *self, struct nrc_msg_buf *msg)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE));
    NRC_ASSERT((msg != NULL) && (msg->hdr.type == NRC_MSG_TYPE_BUF));
    
    NRC_ASSERT(self->msg_buf == NULL);
    self->msg_buf = msg;

    result = nrc_serial_write(self->serial, msg->buf, msg->buf_size);

    if (result == NRC_R_OK) {
        self->state = NRC_N_SERIAL_OUT_S_STARTED_TX_BUF;
    }
    else {
        NRC_LOGW(_tag, "write_msg_buf(%s): failed write", self->hdr.cfg_id);
        nrc_os_msg_free(msg);
        self->state = NRC_N_SERIAL_OUT_S_STARTED;
    }

    return result;
}

static s32_t write_msg_data_avail(struct nrc_node_serial_out *self, struct nrc_msg_data_available *msg)
{
    s32_t result = NRC_R_OK;
    u32_t bytes = 0;

    NRC_ASSERT((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE));

    if ((msg != NULL) && (msg->read != NULL) && (msg->node != NULL)) {
        self->read_fcn = msg->read;
        self->read_node = msg->node;
    }
    NRC_ASSERT((self->read_node != NULL) && (self->read_fcn != NULL));

    bytes = self->read_fcn(self->read_node, self->data_avail_buf, self->max_buf_size);

    self->state = NRC_N_SERIAL_OUT_S_STARTED;
    if (bytes > 0) {
        result = nrc_serial_write(self->serial, self->data_avail_buf, bytes);

        if (result == NRC_R_OK) {
            self->state = NRC_N_SERIAL_OUT_S_STARTED_TX_DATA_AVAIL;
        }
    }

    if (msg != NULL) {
        nrc_os_msg_free(msg);
    }

    return result;
}