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

#include "nrc_data_in.h"
#include "nrc_os.h"
#include "nrc_assert.h"

#include <string.h>

#define NRC_DIN_TYPE    (0x43898476)

// Callback function for data available message types; Called by reader of the data
static u32_t read_data(void *self, u8_t *buf, u32_t buf_size);

static s32_t timeout(struct nrc_din *self);
static s32_t data_available(struct nrc_din *self);
static s32_t send_data_available(struct nrc_din *self);
static s32_t send_buf(struct nrc_din *self);
static s32_t send_json(struct nrc_din *self);

s32_t nrc_din_start(
    struct nrc_din              *self,
    struct nrc_din_node_pars    node_pars,
    struct nrc_din_stream_api   stream_api)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT(self != NULL);
    NRC_ASSERT(node_pars.cfg_msg_type != NULL);
    NRC_ASSERT(node_pars.node != NULL);
    NRC_ASSERT(stream_api.id != NULL);
    NRC_ASSERT(stream_api.clear != NULL);
    NRC_ASSERT(stream_api.get_bytes != NULL);
    NRC_ASSERT(stream_api.read != NULL);

    self->node_pars = node_pars;
    self->stream_api = stream_api;
    self->type = NRC_DIN_TYPE;

    self->msg_str = NULL;
    self->str_len = 0;
    self->json_cnt = 0;

    if (strcmp(node_pars.cfg_msg_type, "dataavailable") == 0) {
        self->msg_type = NRC_DIN_MSG_TYPE_DA;
    }
    else if (strcmp(node_pars.cfg_msg_type, "buf") == 0) {
        self->msg_type = NRC_DIN_MSG_TYPE_BUF;
    }
    else if (strcmp(node_pars.cfg_msg_type, "json") == 0) {
        self->msg_type = NRC_DIN_MSG_TYPE_JSON;
    }
    else {
        self->msg_type = NRC_MSG_TYPE_INVALID;

        result = NRC_R_INVALID_CFG;
    }

    return result;
}

s32_t nrc_din_stop(struct nrc_din *self)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DIN_TYPE));

    nrc_os_msg_free(self->msg_str);
    self->msg_str = NULL;

    self->type = 0;
    self->msg_type = NRC_DIN_MSG_TYPE_INVALID;

    return result;
}

s32_t nrc_din_recv_evt(struct nrc_din *self, u32_t evt_mask)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DIN_TYPE));

    if (evt_mask &  NRC_DIN_EVT_DATA_AVAIL) {
        result = data_available(self);
    }
    if (evt_mask &  NRC_DIN_EVT_TIMEOUT) {
        result = timeout(self);
    }

    return result;
}

static s32_t data_available(struct nrc_din *self)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DIN_TYPE));

    switch (self->msg_type) {
    case NRC_DIN_MSG_TYPE_DA:
        result = send_data_available(self);
        break;
    case NRC_DIN_MSG_TYPE_BUF:
        result = send_buf(self);
        break;
    case NRC_DIN_MSG_TYPE_JSON:
        result = send_json(self);
        break;
    default:
        result = NRC_R_NOT_SUPPORTED;
        break;
    }

    return result;
}

static s32_t timeout(struct nrc_din *self)
{
    s32_t result = NRC_R_OK;

    // TODO:

    return result;
}

static s32_t send_data_available(struct nrc_din *self)
{
    s32_t                           result = NRC_R_OK;
    struct nrc_msg_data_available   *msg = NULL;

    msg = (struct nrc_msg_data_available*)nrc_os_msg_alloc(sizeof(struct nrc_msg_data_available));
    if (msg != NULL) {
        msg->hdr.next = NULL;
        msg->hdr.topic = self->node_pars.topic;
        msg->hdr.type = NRC_MSG_TYPE_DATA_AVAILABLE;

        msg->node = self; // Note: Not the super node but the data_in sub-node
        msg->read = read_data;

        result = nrc_os_send_msg_from(self->node_pars.node, msg, self->node_pars.prio);
    }
    else {
        result = NRC_R_OUT_OF_MEM;
    }

    return result;
}

static s32_t send_buf(struct nrc_din *self)
{
    s32_t               result = NRC_R_OK;
    struct nrc_msg_buf  *msg = NULL;
    u32_t               bytes_to_read = self->stream_api.get_bytes(self->stream_api.id);

    if (bytes_to_read > 0) {
        if (bytes_to_read > self->node_pars.max_size) {
            bytes_to_read = self->node_pars.max_size;
        }

        msg = (struct nrc_msg_buf*)nrc_os_msg_alloc(sizeof(struct nrc_msg_buf) + bytes_to_read);
        if (msg != NULL) {
            msg->hdr.next = NULL;
            msg->hdr.topic = self->node_pars.topic;
            msg->hdr.type = NRC_MSG_TYPE_BUF;

            msg->buf_size = self->stream_api.read(self->stream_api.id, msg->buf, bytes_to_read);

            result = nrc_os_send_msg_from(self->node_pars.node, msg, self->node_pars.prio);

            if (self->stream_api.get_bytes(self->stream_api.id) > 0) {
                result = nrc_os_send_evt(self->node_pars.node, NRC_DIN_EVT_DATA_AVAIL, self->node_pars.prio);
            }
        }
        else {
            // No memory left; clear data
            self->stream_api.clear(self->stream_api.id);
            result = NRC_R_OUT_OF_MEM;
            bytes_to_read = 0;
        }
    }

    return result;
}

static s32_t json_parse(s8_t *str, u32_t str_len) {

}

static s32_t send_json(struct nrc_din *self)
{
    s32_t   result = NRC_R_OK;
    u32_t   cnt = 0;
    bool_t  done = FALSE;

    if (self->msg_str == NULL) {
        self->msg_str = (struct nrc_msg_str*)nrc_os_msg_alloc(sizeof(struct nrc_msg_str) + self->node_pars.max_size);
        self->msg_str->hdr.topic = self->node_pars.topic;
        self->msg_str->hdr.type = NRC_MSG_TYPE_STRING;
        
        self->str_len = 0;
        self->json_cnt = 0;
    }

    // Read byte-by-byte until no more data or full JSON object or string buffer is full
    do {
        cnt = self->stream_api.read(self->stream_api.id, &self->msg_str->str[self->str_len], 1);
        if (cnt > 0) {
            if ((self->str_len == 0) && (self->msg_str->str[self->str_len] != '{')) {
                // Throw away
            }
            else {
                if (self->msg_str->str[self->str_len] == '{') {
                    self->json_cnt++;
                }
                else if (self->msg_str->str[self->str_len] == '}') {
                    NRC_ASSERT(self->json_cnt > 0);
                    self->json_cnt--;

                    if (self->json_cnt == 0) {
                        done = TRUE;
                    }
                }
                self->str_len++;
            }
        }
    } while (!done && (cnt > 0) && (self->str_len < (self->node_pars.max_size - 1)));

    if (done) {
        self->msg_str->str[self->str_len] = 0;

        // Send string message with (potential) json object
        result = nrc_os_send_msg_from(self->node_pars.node, self->msg_str, self->node_pars.prio);
        self->msg_str = NULL;

        // There might be more data to read
        if (self->stream_api.get_bytes(self->stream_api.id) > 0) {
            result = nrc_os_send_evt(self->node_pars.node, NRC_DIN_EVT_DATA_AVAIL, self->node_pars.prio);
        }
    }
    else if (self->str_len == (self->node_pars.max_size - 1)) {
        // TODO: buffer is too small?
        self->str_len = 0;
        self->json_cnt = 0;

        result = NRC_R_OUT_OF_MEM;
    }

    return result;
}

static u32_t read_data(void *slf, u8_t *buf, u32_t buf_size)
{
    u32_t           bytes_read = 0;
    struct nrc_din  *self = (struct nrc_din*)slf;

    if ((self != NULL) && (self->type == NRC_DIN_TYPE) && (self->msg_type != NRC_DIN_MSG_TYPE_INVALID)) {

        bytes_read = self->stream_api.read(self->stream_api.id, buf, buf_size);
    }

    return bytes_read;
}