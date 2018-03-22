/**
* Copyright 2017 Tomas Frisberg & Ola Bjorsne
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http ://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_factory.h"
#include "nrc_assert.h"
#include "nrc_serial.h"
#include "nrc_data_out.h"

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
    NRC_N_SERIAL_OUT_S_ERROR                    // Error occurred which node cannot recover from. Other nodes can continue to run.
};

struct nrc_node_serial_out {
    struct nrc_node_hdr             hdr;            // General node header; from create function in pars

    const s8_t                      *topic;         // Node topic from cfg
    const s8_t                      *cfg_serial_id; // serial-port (configuration node) id from cfg
    s8_t                            prio;           // Node prio; used for sending messages; from cfg
    u32_t                           max_buf_size;   // Max write size

    enum nrc_node_serial_out_state  state;          // Node state
    struct nrc_dout                 *data_out;      // Data-out sub-node
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
                self->data_out = (struct nrc_dout*)nrc_port_heap_alloc(sizeof(struct nrc_dout));
                if (self->data_out != NULL) {
                    memset(self->data_out, 0, sizeof(struct nrc_dout));
                }
                else {
                    result = NRC_R_OUT_OF_MEM;
                }
            }

            if (OK(result)) {
                self->state = NRC_N_SERIAL_OUT_S_INITIALISED;
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
            nrc_port_heap_free(self->data_out);
            self->data_out = NULL;

            self->state = NRC_N_SERIAL_OUT_S_CREATED;
            break;

        default:
            NRC_LOGE(_tag, "deinit(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_OUT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
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
                struct nrc_dout_node_pars pars = {self->max_buf_size};
                struct nrc_dout_stream_api api = {self->serial, nrc_serial_write};

                result = nrc_dout_start(self->data_out, pars, api);
            }

            if (OK(result)) {
                self->state = NRC_N_SERIAL_OUT_S_STARTED;
            }
            else {
                self->state = NRC_N_SERIAL_OUT_S_ERROR;
                NRC_LOGE(_tag, "start(%s): could not open serial %s", self->hdr.cfg_id, self->cfg_serial_id);
            }
            break;

        case NRC_N_SERIAL_OUT_S_STARTED:
            NRC_LOGW(_tag, "start(%d): already started", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "start(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SERIAL_OUT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
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

            result = nrc_dout_stop(self->data_out);

            self->state = NRC_N_SERIAL_OUT_S_INITIALISED;
            result = NRC_R_OK;
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

    return result;
}

static s32_t nrc_node_serial_out_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_serial_out  *self = (struct nrc_node_serial_out*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SERIAL_OUT_TYPE)) {
        switch (self->state) {
        case NRC_N_SERIAL_OUT_S_STARTED:
            result = nrc_dout_recv_msg(self->data_out, msg);
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
            if ((event_mask & NRC_N_SERIAL_OUT_EVT_WRITE_COMPLETE) != 0) {
                result = nrc_dout_write_complete(self->data_out);
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

    return result;
}
