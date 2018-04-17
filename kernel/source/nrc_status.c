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
#include "nrc_status.h"
#include "nrc_types.h"
#include "nrc_os.h"
#include "nrc_msg.h"
#include "nrc_node.h"
#include "nrc_assert.h"
#include "nrc_log.h"
#include <string.h>

struct nrc_status_listener {
    struct nrc_status_listener  *next;

    struct nrc_node_hdr     *node_hdr;
    const s8_t              *group;
};

static bool_t                       _initialised = FALSE;
static struct nrc_status_listener   *_listener = NULL;

s32_t nrc_status_init(void)
{
    s32_t result = NRC_R_OK;

    if (!_initialised) {
        _initialised = TRUE;
        _listener = NULL;
    }

    return result;
}

s32_t nrc_status_set(const s8_t *group, nrc_node_t node, struct nrc_status status)
{
    s32_t                       result = NRC_R_OK;
    struct nrc_status_listener  *listener = _listener;
    struct nrc_msg_status       *msg;

    while ((listener != NULL) && (result == NRC_R_OK)) {
        if ((listener->group == NULL) || (strcmp(listener->group, group) == 0)) {
            msg = (struct nrc_msg_status*)nrc_os_msg_alloc(sizeof(struct nrc_msg_status) + (u32_t)strlen(status.text + 1));

            if (msg != NULL) {
                msg->hdr.next = NULL;
                msg->hdr.topic = status.topic;
                msg->hdr.type = NRC_MSG_TYPE_STATUS;

                msg->node = node;
                msg->status = status.status;
                strcpy(msg->text, status.text);

                result = nrc_os_send_msg_to(listener->node_hdr, msg, status.prio);
            }
            else {
                result = NRC_R_OUT_OF_MEM;
            }
        }
        listener = listener->next;
    }

    return result;
}

s32_t nrc_status_start_listen(const s8_t *group, nrc_node_t listener_node)
{
    s32_t result = NRC_R_ERROR;

    NRC_ASSERT(listener_node != NULL);

    if (_initialised) {
        struct nrc_status_listener *listener = (struct nrc_status_listener*)nrc_port_heap_alloc(sizeof(struct nrc_status_listener));
    
        if (listener != NULL) {
            listener->next = _listener;
            _listener = listener;

            listener->group = group;
            listener->node_hdr = (struct nrc_node_hdr*)listener_node;

            result = NRC_R_OK;
        }
        else {
            result = NRC_R_OUT_OF_MEM;
        }
    }

    return result;
}

s32_t nrc_status_stop_listen(nrc_node_t listener_node)
{
    s32_t result = NRC_R_ERROR;

    NRC_ASSERT(listener_node != NULL);

    if (_initialised) {
        struct nrc_status_listener *current = _listener;
        struct nrc_status_listener *previous = NULL;

        while ((current != NULL) && (current != listener_node)) {
            previous = current;
            current = current->next;
        }

        if (current == listener_node) {
            if (previous == NULL) {
                _listener = current->next;
            }
            else {
                previous->next = current->next;
            }
            nrc_port_heap_free(current);

            result = NRC_R_OK;
        }
    }

    return result;
}

