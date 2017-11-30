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

#include "nrc_os.h"
#include "nrc_port.h"
#include <assert.h>
#include <string.h>

#define NRC_OS_STACK_SIZE   (4096)
#define NRC_OS_NODE_TYPE    (0xA5A5)
#define NRC_OS_MSG_TYPE     (0x5A5A)

enum nrc_os_state {
    NRC_OS_S_INVALID = 0,
    NRC_OS_S_INITIALIZED,
    NRC_OS_S_STARTED
};

struct nrc_os_msg_hdr {
    struct nrc_os_msg_hdr   *next;
    nrc_node_id_t           to_node_id;
    s8_t                    prio;
    s8_t                    padding[3];
    u32_t                   total_size;
    u32_t                   type;
};

struct nrc_os_msg_tail {
    u32_t dead_beef;
};

struct nrc_os_node_hdr {
    struct nrc_os_node_hdr  *next;
    struct nrc_os_node_hdr  *previous;

    struct nrc_node_api *api;
    const s8_t          *cfg_id;

    u32_t               event;
    s8_t                prio;
    s8_t                padding[3];

    u32_t               type;
};

struct nrc_os {
    enum nrc_os_state           state;

    nrc_port_thread_t           thread;
    nrc_port_sema_t             sema;

    struct nrc_os_node_hdr      *node_list;
    struct nrc_os_msg_hdr       *msg_list;
};

static struct nrc_os _os;

static void nrc_os_thread_fcn(void)
{
    //printf("Enter NRC OS Thread\n");

    //printf("Exit NRC OS Thread\n");
}

s32_t nrc_os_init(void)
{
    s32_t result = 0;

    assert(sizeof(struct nrc_os_msg_hdr) % 4 == 0);
    assert(sizeof(struct nrc_os_msg_tail) % 4 == 0);

    memset(&_os, 0, sizeof(struct nrc_os));

    result = nrc_port_sema_init(0, &_os.sema);
    assert(result == NRC_PORT_RES_OK);

    result = nrc_port_thread_init(
        NRC_PORT_THREAD_PRIO_NORMAL,
        NRC_OS_STACK_SIZE,
        nrc_os_thread_fcn,
        &(_os.thread));
    assert(result == NRC_PORT_RES_OK);

    _os.state = NRC_OS_S_INITIALIZED;

    return result;
}

s32_t nrc_os_deinit(void)
{
    s32_t result = NRC_PORT_RES_OK;

    assert(_os.state == NRC_OS_S_INITIALIZED);

    //TODO: Dealloc all nodes, messages, events, etc..
    
    return result;
}

s32_t nrc_os_start(void)
{
    s32_t result;

    assert(_os.state == NRC_OS_S_INITIALIZED);

    result = nrc_port_thread_start(_os.thread);
    assert(result == NRC_PORT_RES_OK);

    _os.state = NRC_OS_S_STARTED;
    
    return result;
}

s32_t nrc_os_stop(void)
{
    s32_t result = NRC_PORT_RES_OK;

    assert(_os.state == NRC_OS_S_STARTED);

    //TODO:
    
    return result;
}

struct nrc_node_hdr* nrc_os_node_alloc(u32_t size)
{
    u32_t total_size = sizeof(struct nrc_os_node_hdr) + size;

    struct nrc_os_node_hdr  *os_node_hdr = (struct nrc_os_node_hdr*)nrc_port_heap_alloc(total_size);
    struct nrc_node_hdr     *node_hdr = 0;

    if (os_node_hdr != 0) {
        node_hdr = (struct nrc_node_hdr*)(os_node_hdr + 1);

        memset(os_node_hdr, 0, sizeof(struct nrc_os_node_hdr));
        memset(node_hdr, 0, sizeof(struct nrc_node_hdr));

        os_node_hdr->prio = S8_MAX_VALUE;
        os_node_hdr->type = NRC_OS_NODE_TYPE;
    }

    return node_hdr;
}

s32_t nrc_os_register_node(struct nrc_node_hdr *node_hdr, struct nrc_node_api *api, const s8_t *cfg_id)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if ((node_hdr != 0) && (api != 0) && (cfg_id != 0) &&
        (api->init != 0) && (api->deinit != 0) && (api->start != 0) && (api->stop != 0) &&
        (api->recv_msg != 0) && (api->recv_evt != 0)) {

        struct nrc_os_node_hdr *os_node_hdr = (struct nrc_os_node_hdr*)node_hdr - 1;

        if (os_node_hdr->type != NRC_OS_NODE_TYPE) {
            os_node_hdr->api = api;
            os_node_hdr->cfg_id = cfg_id;
            os_node_hdr->prio = S8_MAX_VALUE;
            os_node_hdr->event = 0;

            if (_os.node_list == 0) {
                _os.node_list = os_node_hdr;
                os_node_hdr->previous = 0;
            }
            else {
                struct nrc_os_node_hdr *node = _os.node_list;

                while (node->next != 0) {
                    node = node->next;
                }

                node->next = os_node_hdr;
                os_node_hdr->previous = node;
            }

            result = NRC_PORT_RES_OK;
        }
    }

    return result;
}

s32_t nrc_os_get_node_id(const s8_t *cfg_id, nrc_node_id_t *id)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if ((cfg_id != 0) && (id != 0)) {

        *id = 0;
        result = NRC_PORT_RES_NOT_FOUND;

        bool_t                  found = FALSE;
        struct nrc_os_node_hdr  *node = _os.node_list;

        while ((found == FALSE) && (node != 0)) {
            if (strncmp(cfg_id, node->cfg_id, NRC_MAX_CFG_NAME_LEN) == 0) {
                found = TRUE;
                *id = (nrc_node_id_t)node;
                result = NRC_PORT_RES_OK;
            }
            node = node->next;
        }
    }
     
    return result;
}

struct nrc_msg_hdr* nrc_os_msg_alloc(u32_t size)
{
    if ((size % 4) != 0) {
        size += 4 - (size % 4);
    }

    u32_t total_size = sizeof(struct nrc_os_msg_hdr) + size + sizeof(struct nrc_os_msg_tail);

    struct nrc_os_msg_hdr *header = (struct nrc_os_msg_hdr*)nrc_port_heap_fast_alloc(total_size);
    struct nrc_msg_hdr    *msg = 0;

    if (header != 0) {
        msg = (struct nrc_msg_hdr*)(header + 1);

        struct nrc_os_msg_tail *tail = (struct nrc_os_msg_tail*)((uint8_t*)msg + size);

        memset(&header, 0, sizeof(header));
        memset(&msg, 0, sizeof(struct nrc_msg_hdr));

        header->total_size = total_size;
        header->type = NRC_OS_MSG_TYPE;
        tail->dead_beef = 0xDEADBEEF;
    }
    
    return msg;
}

struct nrc_msg_hdr* nrc_os_msg_clone(struct nrc_msg_hdr *msg)
{
    //TODO: Check valid message

    struct nrc_os_msg_hdr *header = (struct nrc_os_msg_hdr*)msg - 1;
    struct nrc_os_msg_hdr *new_header = (struct nrc_os_msg_hdr*)nrc_port_heap_fast_alloc(header->total_size);

    memcpy(new_header, header, header->total_size);

    return (struct nrc_msg_hdr*)(new_header + 1);
}

void nrc_os_msg_free(struct nrc_msg_hdr *msg)
{
    struct nrc_os_msg_hdr   *os_msg_header;

    while (msg != 0) {
        os_msg_header = (struct nrc_os_msg_hdr*)msg - 1;

        //TODO: Check valid message

        msg = msg->next;

        nrc_port_heap_fast_free(os_msg_header);
    }
}


s32_t nrc_os_send_msg(nrc_node_id_t id, struct nrc_msg_hdr *msg, s8_t prio)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if ((id != 0) && (msg != 0)) {

        struct nrc_os_node_hdr  *os_node_hdr = (struct nrc_os_node_hdr*)id - 1;
        struct nrc_os_msg_hdr   *os_msg_hdr = (struct nrc_os_msg_hdr*)msg - 1;

        if ((os_node_hdr->type == NRC_OS_NODE_TYPE) && (os_msg_hdr->type == NRC_OS_MSG_TYPE)) {

            os_msg_hdr->prio = prio;

            if ((_os.msg_list == 0) || (os_msg_hdr->prio < _os.msg_list->prio)) {
                os_msg_hdr->next = _os.msg_list;
                _os.msg_list = os_msg_hdr;
            }
            else {
                struct nrc_os_msg_hdr *msg = _os.msg_list;

                while ((msg->next != 0) && (os_msg_hdr->prio >= msg->next->prio)) {
                    msg = msg->next;
                }
                os_msg_hdr->next = msg->next;
                msg->next = os_msg_hdr;
            }

            result = NRC_PORT_RES_OK;
        }
    }

    return result;
}

s32_t nrc_os_set_evt(nrc_node_id_t id, u32_t event_mask, enum nrc_os_prio prio)
{
    s32_t result = NRC_PORT_RES_NOT_SUPPORTED;
    
    return result;
}