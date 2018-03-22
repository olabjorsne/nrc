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

#include "nrc_data_out.h"
#include "nrc_os.h"
#include "nrc_assert.h"
#include "nrc_log.h"

#include <string.h>

#define NRC_DOUT_TYPE    (0xB54BB456)

static s32_t write_msg_buf(struct nrc_dout *self, struct nrc_msg_buf *msg);
static s32_t write_msg_data_avail(struct nrc_dout *self, struct nrc_msg_data_available *msg);

s32_t nrc_dout_start(
    struct nrc_dout              *self,
    struct nrc_dout_node_pars    node_pars,
    struct nrc_dout_stream_api   stream_api)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT(self != NULL);
    NRC_ASSERT(stream_api.id != NULL);
    NRC_ASSERT(stream_api.write != NULL);

    self->state = NRC_DOUT_S_IDLE;
    self->node_pars = node_pars;
    self->stream_api = stream_api;
    self->type = NRC_DOUT_TYPE;

    self->data_avail_buf = (u8_t*)nrc_port_heap_alloc(self->node_pars.buf_size);
    if (self->data_avail_buf == NULL) {
        result = NRC_R_OUT_OF_MEM;
    }

    return result;
}

s32_t nrc_dout_stop(struct nrc_dout *self)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DOUT_TYPE));
    NRC_ASSERT(self->state != NRC_DOUT_S_INVALID);

    self->state = NRC_DOUT_S_INVALID;
    self->type = 0;

    nrc_port_heap_free(self->data_avail_buf);
    self->data_avail_buf = NULL;

    return result;
}

s32_t nrc_dout_recv_msg(struct nrc_dout  *self, nrc_msg_t msg)
{
    s32_t               result = NRC_R_OK;
    struct nrc_msg_hdr  *msg_hdr = (struct nrc_msg_hdr*)msg;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DOUT_TYPE));
    
    switch (self->state) {
    case NRC_DOUT_S_IDLE:
        if (msg_hdr->type == NRC_MSG_TYPE_BUF) {
            result = write_msg_buf(self, (struct nrc_msg_buf*)msg_hdr);
        }
        else if (msg_hdr->type == NRC_MSG_TYPE_DATA_AVAILABLE) {
            result = write_msg_data_avail(self, (struct nrc_msg_data_available*)msg_hdr);
        }
        else {
            nrc_os_msg_free(msg);
        }
        break;

    case NRC_DOUT_S_TX_BUF:
    case NRC_DOUT_S_TX_DATA_AVAIL:
        // Busy writing already
        nrc_os_msg_free(msg);
        break;

    default:
        nrc_os_msg_free(msg);
        result = NRC_R_INVALID_STATE;
        break;
    }

    return result;
}

s32_t nrc_dout_write_complete(struct nrc_dout *self)
{
    s32_t result = NRC_R_OK;

    switch (self->state) {
    case NRC_DOUT_S_TX_BUF:
    {
        // Get next msg if any
        struct nrc_msg_buf *msg = (struct nrc_msg_buf*)self->msg_buf->hdr.next;

        // Free written msg but not linked ones
        self->msg_buf->hdr.next = NULL;
        nrc_os_msg_free(self->msg_buf);
        self->msg_buf = NULL;

        // Write next message if any
        self->state = NRC_DOUT_S_IDLE;
        if (msg != NULL) {
            result = write_msg_buf(self, msg);
        }
        break;
    }   
    case NRC_DOUT_S_TX_DATA_AVAIL:
        NRC_ASSERT(self->data_avail_buf != NULL);

        // Continue to write (using previous data available message)
        self->state = NRC_DOUT_S_IDLE;
        result = write_msg_data_avail(self, NULL);
        break;

    default:
        result = NRC_R_INVALID_STATE;
        break;
    }

    return result;
}

static s32_t write_msg_buf(struct nrc_dout *self, struct nrc_msg_buf *msg)
{
    s32_t result = NRC_R_OK;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DOUT_TYPE));
    NRC_ASSERT((msg != NULL) && (msg->hdr.type == NRC_MSG_TYPE_BUF));

    NRC_ASSERT(self->msg_buf == NULL);
    self->msg_buf = msg;

    result = self->stream_api.write(self->stream_api.id, msg->buf, msg->buf_size);

    if (result == NRC_R_OK) {
        self->state = NRC_DOUT_S_TX_BUF;
    }
    else {
        nrc_os_msg_free(msg);
        self->state = NRC_DOUT_S_IDLE;
    }

    return result;
}

static s32_t write_msg_data_avail(struct nrc_dout *self, struct nrc_msg_data_available *msg)
{
    s32_t result = NRC_R_OK;
    u32_t bytes = 0;

    NRC_ASSERT((self != NULL) && (self->type == NRC_DOUT_TYPE));

    if ((msg != NULL) && (msg->read != NULL) && (msg->node != NULL)) {
        self->read_fcn = msg->read;
        self->read_node = msg->node;
    }
    NRC_ASSERT((self->read_node != NULL) && (self->read_fcn != NULL));

    bytes = self->read_fcn(self->read_node, self->data_avail_buf, self->node_pars.buf_size);

    self->state = NRC_DOUT_S_IDLE;
    if (bytes > 0) {
        result = self->stream_api.write(self->stream_api.id, self->data_avail_buf, bytes);

        if (result == NRC_R_OK) {
            self->state = NRC_DOUT_S_TX_DATA_AVAIL;
        }
    }

    if (msg != NULL) {
        nrc_os_msg_free(msg);
    }

    return result;
}