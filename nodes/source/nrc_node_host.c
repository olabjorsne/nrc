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
#include "nrc_msg.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_factory.h"
#include <string.h>
#include "nrc_assert.h"
#include "nrc_status.h"
#include "jsmn.h"

#define NRC_N_HOST_TYPE             (0x56483887)
#define NRC_N_HOST_MAX_TOKENS       (100)

enum nrc_node_host_cmd {
    NRC_N_HOST_CMD_INVALID,
    NRC_N_HOST_CMD_START,
    NRC_N_HOST_CMD_STOP
};

enum nrc_node_host_state {
    NRC_N_HOST_S_INVALID,
    NRC_N_HOST_S_CREATED,
    NRC_N_HOST_S_INITIALISED,
    NRC_N_HOST_S_STARTED,
    NRC_N_HOST_S_ERROR
};

struct nrc_node_host {
    struct nrc_node_hdr         hdr;
    const s8_t                  *topic;

    enum nrc_node_host_state    state;
    s8_t                        prio;

    nrc_cfg_t                   *cfg;

    jsmn_parser                 json_parser;
    jsmntok_t                   json_tokens[NRC_N_HOST_MAX_TOKENS];
    s32_t                       num_tokens;

    u32_t                       type;
};

static nrc_node_t nrc_node_host_create(struct nrc_node_factory_pars *pars);
static s32_t nrc_node_host_init(nrc_node_t self);
static s32_t nrc_node_host_deinit(nrc_node_t self);
static s32_t nrc_node_host_start(nrc_node_t self);
static s32_t nrc_node_host_stop(nrc_node_t self);
static s32_t nrc_node_host_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_host_recv_evt(nrc_node_t self, u32_t event_mask);

const static s8_t*          _tag = "host";
static struct nrc_node_api  _api;
static struct nrc_node_host *_host = NULL;

void nrc_node_host_register(void)
{
    s32_t status = nrc_factory_register_node_type("nrc-host", nrc_node_host_create);
    if (!OK(status)) {
        NRC_LOGE(_tag, "register: error %d", status);
    }
}

nrc_node_t nrc_node_host_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_host* node = NULL;

    if ((pars != NULL) && (strcmp("nrc-host", pars->cfg_type) == 0)) {
        node = (struct nrc_node_host*)nrc_os_node_alloc(sizeof(struct nrc_node_host));

        if (node != NULL) {
            memset(node, 0, sizeof(struct nrc_node_host));
            node->hdr.cfg_id = pars->cfg_id;
            node->hdr.cfg_type = pars->cfg_type;
            node->hdr.cfg_name = pars->cfg_name;

            memset(&_api, 0, sizeof(struct nrc_node_api));
            _api.init = nrc_node_host_init;
            _api.deinit = nrc_node_host_deinit;
            _api.start = nrc_node_host_start;
            _api.stop = nrc_node_host_stop;
            _api.recv_msg = nrc_node_host_recv_msg;
            _api.recv_evt = nrc_node_host_recv_evt;

            pars->api = &_api;

            node->type = NRC_N_HOST_TYPE;
            node->state = NRC_N_HOST_S_CREATED;
        }
        else {
            NRC_LOGE(_tag, "create(%s): out of mem", pars->cfg_id);
        }
    }
    else {
        NRC_LOGE(_tag, "create: invalid in params");
    }

    return node;
}

static s32_t nrc_node_host_init(nrc_node_t slf)
{
    struct nrc_node_host    *self = (struct nrc_node_host*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((_host == NULL) && (self != NULL) && (self->type == NRC_N_HOST_TYPE)) {
        switch (self->state) {
        case NRC_N_HOST_S_CREATED:
            result = nrc_status_init();

            if (OK(result)) {
                result = nrc_cfg_init();
            }

            if (OK(result)) {
                result = nrc_cfg_get_str(self->hdr.cfg_id, "topic", &self->topic);
            }
            
            if (OK(result)) {
                // Get node priority
                s32_t prio;
                result = nrc_cfg_get_int(self->hdr.cfg_id, "priority", &prio);
                if (OK(result)) {
                    if ((prio >= S8_MIN_VALUE) && (prio <= S8_MAX_VALUE)) {
                        self->prio = (s8_t)prio;
                    }
                    else {
                        result = NRC_R_INVALID_CFG;
                    }
                }
            }

            if (OK(result)) {
                jsmn_init(&self->json_parser);

                _host = self; // Singleton

                self->state = NRC_N_HOST_S_INITIALISED;
            }
            else {
                self->state = NRC_N_HOST_S_ERROR;

                NRC_LOGE(_tag, "init(%s): to error state", self->hdr.cfg_id);
            }
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_host_deinit(nrc_node_t slf)
{
    struct nrc_node_host  *self = (struct nrc_node_host*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_HOST_TYPE)) {
        switch (self->state) {
        case NRC_N_HOST_S_INITIALISED:
        case NRC_N_HOST_S_ERROR:
            // Nothing to free

            _host = NULL;

            self->state = NRC_N_HOST_S_CREATED;
            result = NRC_R_OK;
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_host_start(nrc_node_t slf)
{
    struct nrc_node_host  *self = (struct nrc_node_host*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_HOST_TYPE)) {
        switch (self->state) {
        case NRC_N_HOST_S_INITIALISED:
            self->cfg = nrc_cfg_create(NULL, 0);

            // Register self as listener to all status messages
            if (self->cfg != NULL) {
                result = nrc_status_start_listen(NULL, self);
            }

            if (OK(result)) {
                self->state = NRC_N_HOST_S_STARTED;
            }
            else {
                self->state = NRC_N_HOST_S_ERROR;

                NRC_LOGE(_tag, "start(%s): error %d", self->hdr.cfg_id, result);
            }
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_host_stop(nrc_node_t slf)
{
    struct nrc_node_host  *self = (struct nrc_node_host*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_HOST_TYPE)) {
        switch (self->state) {
        case NRC_N_HOST_S_STARTED:
            //nrc_cfg_destroy(self->cfg); TODO
            self->cfg = NULL;

            result = nrc_status_stop_listen(self);

            self->state = NRC_N_HOST_S_INITIALISED;
            result = NRC_R_OK;
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static enum nrc_node_host_cmd get_cmd(jsmntok_t *tokens, s32_t num_tokens, s8_t *json)
{
    enum nrc_node_host_cmd cmd = NRC_N_HOST_CMD_INVALID;

    if ((num_tokens > 2) && 
        (tokens[1].type == JSMN_STRING) && (tokens[2].type == JSMN_STRING)) {

        s8_t *str = &json[tokens[1].start];

        if (strncmp(str, "cmd", 3) == 0) {
            str = &json[tokens[2].start];

            if (strncmp(str, "start", 5) == 0) {
                cmd = NRC_N_HOST_CMD_START;
            }
            else if (strncmp(str, "stop", 4) == 0) {
                cmd = NRC_N_HOST_CMD_STOP;
            }
        }

    }

    return cmd;
}

static s32_t cmd_start(struct nrc_node_host *self)
{
    s32_t result = NRC_R_OK;

    result = nrc_os_start(self->cfg);

    return result;
}

static s32_t node_cfg(struct nrc_node_host *self, const s8_t *n_cfg)
{
    s32_t       result = NRC_R_OK;
    const s8_t  *cfg_node_id = NULL;

    result = nrc_cfg_add_node(self->cfg, n_cfg, (u32_t)strlen(n_cfg), &cfg_node_id);

    return result;
}

static s32_t handle_json(struct nrc_node_host *self, struct nrc_msg_str *msg)
{
    s32_t result = NRC_R_OK;

    jsmn_init(&self->json_parser);
    self->num_tokens = jsmn_parse(&self->json_parser, msg->str, strlen(msg->str), self->json_tokens, NRC_N_HOST_MAX_TOKENS);

    if (self->num_tokens > 2) {
        // Command
        enum nrc_node_host_cmd cmd = get_cmd(self->json_tokens, self->num_tokens, msg->str);

        switch (cmd) {
        case NRC_N_HOST_CMD_START:
            result = cmd_start(self);
            break;
        case NRC_N_HOST_CMD_STOP:
            NRC_ASSERT(FALSE); // Not implemented
            break;
        default:
            // Assume node configuration
            result = node_cfg(self, msg->str);
            break;
        }
    }

    return result;
}

static s32_t handle_status(struct nrc_node_host *self, struct nrc_msg_status *msg)
{
    s32_t result = NRC_R_OK;

    return result;
}

static s32_t nrc_node_host_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_host    *self = (struct nrc_node_host*)slf;
    struct nrc_msg_hdr      *msg_hdr = (struct nrc_msg_hdr*)msg;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_HOST_TYPE) && (msg != NULL)) {

        switch (self->state) {
        case NRC_N_HOST_S_STARTED:
            if (msg_hdr->type == NRC_MSG_TYPE_STRING) {
                // JSON commands or nodes
                result = handle_json(self, (struct nrc_msg_str*)msg);
            }
            else if (msg_hdr->type == NRC_MSG_TYPE_STATUS) {
                // Status messages
                result = handle_status(self, (struct nrc_msg_status*)msg);
            }
            else {
                NRC_LOGW(_tag, "recv_msg(%s): unexpected msg %d", self->hdr.cfg_id);
            }
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    if (msg != NULL) {
        nrc_os_msg_free(msg);
    }

    return result;
}

static s32_t nrc_node_host_recv_evt(nrc_node_t slf, u32_t event_mask)
{
    struct nrc_node_host    *self = (struct nrc_node_host*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;
    struct nrc_msg_hdr      *hdr = NULL;

    if ((self != NULL) && (self->type == NRC_N_HOST_TYPE)) {
        switch (self->state) {
        case NRC_N_HOST_S_STARTED:
            NRC_LOGE(_tag, "recv_evt(%s): unextected evt %d", self->hdr.cfg_id, event_mask);
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

