/**
* Copyright 2018 Tomas Frisberg & Ola Bjorsne
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
#include "nrc_timer.h"
#include "nrc_msg.h"

#include <string.h>

// Type number to check that node pointer is of correct type
#define NRC_N_SPLIT_TYPE            (0x76382A8B)

// Default max buffer read size
#define NRC_N_SPLIT_MAX_BUF_SIZE (256)

#define NRC_N_SPLIT_EVT_TIMEOUT (1)

// Node states
enum nrc_node_split_state {
    NRC_N_SPLIT_S_INVALID = 0,
    NRC_N_SPLIT_S_CREATED,      // Created but not yet initialised. No memory allocated except the object itself.
    NRC_N_SPLIT_S_INITIALISED,  // Initialised and memory allocated. Ready to be started.
    NRC_N_SPLIT_S_STARTED,      // Resources is started and node is now running.
    NRC_N_SPLIT_S_ERROR         // Error occurred which node cannot recover from. Other nodes can continue to run.
};

enum nrc_node_split_parser_state {
    NRC_N_SPLITP_S_INVALID,
    NRC_N_SPLITP_S_WAIT_START,
    NRC_N_SPLITP_S_WAIT_END
};

enum nrc_node_slit_data_type {
    NRC_N_SPLIT_DATA_STRING,
    NRC_N_SPLIT_DATA_JSON
};

// Serial-in node structure
struct nrc_node_split {
    struct nrc_node_hdr             hdr;            // General node header; from create function in pars

    const s8_t                      *topic;         // Node topic from cfg
    enum nrc_node_slit_data_type    data_type;      // Type of data to parse
    u32_t                           max_buf_size;   // Max buf size for buf messages
    u8_t                            *buf;           // Buf to store and parse data
    u32_t                           buf_cnt;        // Number of bytes in buffer
    u32_t                           json_cnt;       // Count of starting/ending json {}
    nrc_timer_t                     timer;          // For timeout of packet end

    enum nrc_node_split_state        state;          // Node state
    enum nrc_node_split_parser_state state_parser;   // State of split parser
 
    u32_t                           type;           // Object type check; unique number for every object type
};

// Node create function registered in node factory with type string
static nrc_node_t nrc_node_split_create(struct nrc_node_factory_pars *pars);

// Node api functions
static s32_t nrc_node_split_init(nrc_node_t self);
static s32_t nrc_node_split_deinit(nrc_node_t self);
static s32_t nrc_node_split_start(nrc_node_t self);
static s32_t nrc_node_split_stop(nrc_node_t self);
static s32_t nrc_node_split_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_split_recv_evt(nrc_node_t self, u32_t event_mask);

static s32_t parse_json(struct nrc_node_split *self, struct nrc_msg_hdr *msg_hdr);
static s32_t parse_string(struct nrc_node_split *self, struct nrc_msg_hdr *msg_hdr);

// Internal variables
const static s8_t*          _tag = "split";  // Used in NRC_LOG function
static struct nrc_node_api  _api;           // Node API functions

// Register node to node factory; called at system start
void nrc_node_split_register(void)
{
    s32_t result = nrc_factory_register_node_type("nrc-split", nrc_node_split_create);
    if (!OK(result)) {
        NRC_LOGE(_tag, "register: error %d", result);
    }
}

// Node constructor function
nrc_node_t nrc_node_split_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_split *self = NULL;

    if ((pars != NULL) && (strcmp("nrc-split", pars->cfg_type) == 0)) {
        self = (struct nrc_node_split*)nrc_os_node_alloc(sizeof(struct nrc_node_split));

        if (self != NULL) {
            // Future additions to node API will be set to NULL until supported by node
            memset(&_api, 0, sizeof(struct nrc_node_api));
            _api.init = nrc_node_split_init;
            _api.deinit = nrc_node_split_deinit;
            _api.start = nrc_node_split_start;
            _api.stop = nrc_node_split_stop;
            _api.recv_msg = nrc_node_split_recv_msg;
            _api.recv_evt = nrc_node_split_recv_evt;

            pars->api = &_api; // Return api functions for the node

            memset(self, 0, sizeof(struct nrc_node_split));

            // Mandatory for all nodes
            self->hdr.cfg_id = pars->cfg_id;
            self->hdr.cfg_type = pars->cfg_type;
            self->hdr.cfg_name = pars->cfg_name;

            // Node specific
            self->max_buf_size = NRC_N_SPLIT_MAX_BUF_SIZE;  // Default max message buf size
                                                           
            self->type = NRC_N_SPLIT_TYPE; // For node pointer validity check

            self->state = NRC_N_SPLIT_S_CREATED;
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
static s32_t nrc_node_split_init(nrc_node_t slf)
{
    struct nrc_node_split   *self = (struct nrc_node_split*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SPLIT_TYPE)) {
        switch (self->state) {
        case NRC_N_SPLIT_S_CREATED:
            result = nrc_timer_init();

            if (OK(result)) {
                // Read topic from configuration
                result = nrc_cfg_get_str(self->hdr.cfg_id, "topic", &self->topic);
            }
             if (OK(result)) {
                // Get msg type to send; data available or byte array
                const s8_t *cfg_data_type = NULL;
                result = nrc_cfg_get_str(self->hdr.cfg_id, "datatype", &cfg_data_type);
                if (OK(result)) {
                    if (strcmp(cfg_data_type, "string")) {
                        self->data_type = NRC_N_SPLIT_DATA_STRING;
                    }
                    else if (strcmp(cfg_data_type, "json")) {
                        self->data_type = NRC_N_SPLIT_DATA_JSON;
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
                self->buf = (u8_t*)nrc_port_heap_alloc(self->max_buf_size);
                if (self->buf == NULL) {
                    result = NRC_R_OUT_OF_MEM;
                }
            }

            if (OK(result)) {
                self->state = NRC_N_SPLIT_S_INITIALISED;
            }
            else {
                self->state = NRC_N_SPLIT_S_ERROR;
                NRC_LOGE(_tag, "init(%s): error %d", self->hdr.cfg_id, result);
            }
            break;

        default:
            NRC_LOGE(_tag, "init(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SPLIT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_split_deinit(nrc_node_t slf)
{
    struct nrc_node_split   *self = (struct nrc_node_split*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SPLIT_TYPE)) {
        switch (self->state) {
        case NRC_N_SPLIT_S_INITIALISED:
        case NRC_N_SPLIT_S_ERROR:
            // Free allocated memory (if any)
            nrc_port_heap_free(self->buf);
            self->buf = NULL;
 
            self->state = NRC_N_SPLIT_S_CREATED;
            break;

        default:
            NRC_LOGE(_tag, "deinit(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SPLIT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_split_start(nrc_node_t slf)
{
    struct nrc_node_split   *self = (struct nrc_node_split*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SPLIT_TYPE)) {
        switch (self->state) {
        case NRC_N_SPLIT_S_INITIALISED:
            self->buf_cnt = 0;
            self->json_cnt = 0;
            break;

        case NRC_N_SPLIT_S_STARTED:
            NRC_LOGW(_tag, "start(%d): already started", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "start(%s): invalid state %d", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SPLIT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_split_stop(nrc_node_t slf)
{
    struct nrc_node_split   *self = (struct nrc_node_split*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SPLIT_TYPE)) {
        switch (self->state) {
        case NRC_N_SPLIT_S_STARTED:
            self->state = NRC_N_SPLIT_S_INITIALISED;
            result = NRC_R_OK;
            break;

        case NRC_N_SPLIT_S_INITIALISED:
            NRC_LOGW(_tag, "stop(%d): already stopped", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "stop(%s): invalid state", self->hdr.cfg_id, self->state);
            self->state = NRC_N_SPLIT_S_ERROR;
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_split_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_split   *self = (struct nrc_node_split*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;
    struct nrc_msg_hdr      *msg_hdr = (struct nrc_msg_hdr*)msg;

    if ((self != NULL) && (self->type == NRC_N_SPLIT_TYPE)) {
        switch (self->state) {
        case NRC_N_SPLIT_S_STARTED:
            if (self->data_type == NRC_N_SPLIT_DATA_JSON) {
                result = parse_json(self, msg_hdr);
            }
            else if (self->data_type == NRC_N_SPLIT_DATA_STRING) {
                result = parse_string(self, msg_hdr);
            }
            else {
                nrc_os_msg_free(msg);
                NRC_LOGW(_tag, "recv_msg(%s): msg type %d not supported", self->hdr.cfg_id, msg_hdr->type);
            }
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

static s32_t nrc_node_split_recv_evt(nrc_node_t slf, u32_t event_mask)
{
    struct nrc_node_split   *self = (struct nrc_node_split*)slf;
    s32_t                       result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_SPLIT_TYPE)) {
        switch (self->state) {
        case NRC_N_SPLIT_S_STARTED:
            if ((event_mask & NRC_N_SPLIT_EVT_TIMEOUT) != 0) {
                // TBD
            }
            else {
                NRC_LOGW(_tag, "recv_evt(%s): unknown event %d", self->hdr.cfg_id, event_mask);
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

static s32_t parse_json(struct nrc_node_split *self, struct nrc_msg_hdr *msg_hdr)
{
    s32_t result = NRC_R_OK;

    nrc_os_msg_free(msg_hdr);

    return result;
}

static s32_t parse_string(struct nrc_node_split *self, struct nrc_msg_hdr *msg_hdr)
{
    nrc_os_msg_free(msg_hdr);
    return NRC_R_NOT_SUPPORTED;
}


