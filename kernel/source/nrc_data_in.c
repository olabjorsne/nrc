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

    if (strcmp(node_pars.cfg_msg_type, "dataavailable") == 0) {
        self->msg_type = NRC_MSG_TYPE_DATA_AVAILABLE;
    }
    else if (strcmp(node_pars.cfg_msg_type, "buf") == 0) {
        self->msg_type = NRC_MSG_TYPE_BUF;
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

    self->type = 0;
    self->msg_type = NRC_MSG_TYPE_INVALID;

    return result;
}

s32_t nrc_din_data_available(
    struct nrc_din              *self,
    bool_t                      *more_to_read)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DIN_TYPE));
    NRC_ASSERT(more_to_read != NULL);

    *more_to_read = FALSE;

    if (self->msg_type == NRC_MSG_TYPE_DATA_AVAILABLE) {
        struct nrc_msg_data_available *msg = NULL;

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
    }
    else if (self->msg_type == NRC_MSG_TYPE_BUF) {
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

                if (self->stream_api.get_bytes(self->stream_api.id) > 0) {
                    *more_to_read = TRUE;
                }

                result = nrc_os_send_msg_from(self->node_pars.node, msg, self->node_pars.prio);
            }
            else {
                // No memory left; clear data
                self->stream_api.clear(self->stream_api.id);
                result = NRC_R_OUT_OF_MEM;
                bytes_to_read = 0;
            }
        }
    }
    else {
        result = NRC_R_ERROR;
    }

    return result;
}

static u32_t read_data(void *slf, u8_t *buf, u32_t buf_size)
{
    u32_t           bytes_read = 0;
    struct nrc_din  *self = (struct nrc_din*)slf;

    if ((self != NULL) && (self->type == NRC_DIN_TYPE) && (self->msg_type != NRC_MSG_TYPE_INVALID)) {

        bytes_read = self->stream_api.read(self->stream_api.id, buf, buf_size);
    }

    return bytes_read;
}