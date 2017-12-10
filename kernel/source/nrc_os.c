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
#include "nrc_log.h"
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
    nrc_node_t              to_node;
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

    u32_t               evt;
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

static void insert_node(struct nrc_os_node_hdr *node);
static void extract_node(struct nrc_os_node_hdr *node);
static void increased_node_prio(struct nrc_os_node_hdr *node);
static void start_registered_nodes(void);
static void init_registered_nodes(void);

static void nrc_os_thread_fcn(void);

static struct nrc_os _os;

s32_t nrc_os_init(void)
{
    s32_t result = 0;

    assert(sizeof(struct nrc_os_msg_hdr) % 4 == 0);
    assert(sizeof(struct nrc_os_msg_tail) % 4 == 0);
    assert(sizeof(struct nrc_os_node_hdr) % 4 == 0);

    memset(&_os, 0, sizeof(struct nrc_os));

    result = nrc_port_init();
    assert(result == NRC_PORT_RES_OK);

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
    s32_t result = NRC_PORT_RES_NOT_SUPPORTED;

    //TODO: Dealloc all nodes, messages, events, etc..
    
    return result;
}

s32_t nrc_os_start(void)
{
    s32_t result = NRC_R_INVALID_STATE;

    switch (_os.state) {
    case NRC_OS_S_INITIALIZED:
        // Initialize all registered nodes
        init_registered_nodes();

        // Start all registered nodes
        start_registered_nodes();

        _os.state = NRC_OS_S_STARTED;

        // Start msg and event handling thread
        result = nrc_port_thread_start(_os.thread);
        assert(result == NRC_PORT_RES_OK);
        break;
    default:
        NRC_LOGD("os", "nrc_os_start: invalid state %d", _os.state);
        break;
    }
    
    return result;
}

s32_t nrc_os_stop(void)
{
    s32_t result = NRC_PORT_RES_NOT_SUPPORTED;

    //TODO:
    
    return result;
}

nrc_node_t nrc_os_node_alloc(u32_t size)
{
    u32_t total_size = sizeof(struct nrc_os_node_hdr) + size;

    struct nrc_os_node_hdr  *os_node_hdr = (struct nrc_os_node_hdr*)nrc_port_heap_alloc(total_size);
    struct nrc_node_hdr     *node_hdr = NULL;

    if (os_node_hdr != NULL) {
        node_hdr = (struct nrc_node_hdr*)(os_node_hdr + 1);

        memset(os_node_hdr, 0, sizeof(struct nrc_os_node_hdr));
        memset(node_hdr, 0, sizeof(struct nrc_node_hdr));

        os_node_hdr->prio = S8_MAX_VALUE;
        os_node_hdr->type = NRC_OS_NODE_TYPE;
    }

    return node_hdr;
}

s32_t nrc_os_node_register(nrc_node_t node, struct nrc_os_register_node_pars pars)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if ((node != NULL) && (pars.api != NULL) && (pars.cfg_id != NULL) &&
        (pars.api->init != NULL) && (pars.api->deinit != NULL) && (pars.api->start != NULL) && (pars.api->stop != NULL) &&
        (pars.api->recv_msg != NULL) && (pars.api->recv_evt != NULL)) {

        struct nrc_os_node_hdr *os_node_hdr = (struct nrc_os_node_hdr*)node - 1;

        if (os_node_hdr->type == NRC_OS_NODE_TYPE) {

            os_node_hdr->api = pars.api;
            os_node_hdr->cfg_id = pars.cfg_id;
            os_node_hdr->prio = S8_MAX_VALUE;
            os_node_hdr->evt = 0;

            insert_node(os_node_hdr);

            result = NRC_PORT_RES_OK;
        }
    }

    return result;
}

nrc_node_t nrc_os_node_get(const s8_t *cfg_id)
{
    s32_t       result = NRC_PORT_RES_INVALID_IN_PARAM;
    nrc_node_t  node = NULL;

    if (cfg_id != NULL) {

        result = NRC_PORT_RES_NOT_FOUND;

        bool_t                  found = FALSE;
        struct nrc_os_node_hdr  *hdr = _os.node_list;

        while ((found == FALSE) && (hdr != NULL)) {
            if (strncmp(cfg_id, hdr->cfg_id, NRC_MAX_CFG_NAME_LEN) == 0) {
                found = TRUE;
                node = hdr;
                result = NRC_PORT_RES_OK;
            }
            hdr = hdr->next;
        }
    }
     
    return node;
}

nrc_msg_t nrc_os_msg_alloc(u32_t size)
{
    u32_t                   total_size = 0;
    struct nrc_os_msg_hdr   *hdr = NULL;
    struct nrc_msg_hdr      *msg = NULL;

    if ((size % 4) != 0) {
        size += 4 - (size % 4);
    }

    total_size = sizeof(struct nrc_os_msg_hdr) + size + sizeof(struct nrc_os_msg_tail);

    hdr = (struct nrc_os_msg_hdr*)nrc_port_heap_fast_alloc(total_size);

    if (hdr != NULL) {
        msg = (struct nrc_msg_hdr*)(hdr + 1);

        struct nrc_os_msg_tail *tail = (struct nrc_os_msg_tail*)((uint8_t*)msg + size);

        memset(hdr, 0, sizeof(hdr));
        memset(msg, 0, sizeof(struct nrc_msg_hdr));

        hdr->total_size = total_size;
        hdr->type = NRC_OS_MSG_TYPE;
        tail->dead_beef = 0xDEADBEEF;
    }
    
    return msg;
}

nrc_msg_t nrc_os_msg_clone(nrc_msg_t msg)
{
    //TODO: Check valid message

    struct nrc_os_msg_hdr *hdr = (struct nrc_os_msg_hdr*)msg - 1;
    struct nrc_os_msg_hdr *new_hdr = (struct nrc_os_msg_hdr*)nrc_port_heap_fast_alloc(hdr->total_size);

    memcpy(new_hdr, hdr, hdr->total_size);

    return (new_hdr + 1);
}

void nrc_os_msg_free(nrc_msg_t msg)
{
    struct nrc_msg_hdr      *hdr = (struct nrc_msg_hdr*)msg;
    struct nrc_os_msg_hdr   *os_msg_header;
    struct nrc_os_msg_tail  *os_msg_tail;

    while (hdr != NULL) {
        os_msg_header = (struct nrc_os_msg_hdr*)msg - 1;
        assert(os_msg_header->type == NRC_OS_MSG_TYPE);

        os_msg_tail = (struct nrc_os_msg_tail*)((u8_t*)os_msg_header + os_msg_header->total_size) - 1;
        assert(os_msg_tail->dead_beef == 0xDEADBEEF);

        hdr = hdr->next;

        nrc_port_heap_fast_free(os_msg_header);
    }
}

s32_t nrc_os_send_msg(nrc_node_t to, nrc_msg_t msg, s8_t prio)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if ((to != NULL) && (msg != NULL) && (prio < S8_MAX_VALUE)) {

        struct nrc_os_node_hdr  *os_node_hdr = (struct nrc_os_node_hdr*)to;
        struct nrc_os_msg_hdr   *os_msg_hdr = (struct nrc_os_msg_hdr*)msg - 1;

        if ((os_node_hdr->type == NRC_OS_NODE_TYPE) && (os_msg_hdr->type == NRC_OS_MSG_TYPE)) {

            os_msg_hdr->prio = prio;
            os_msg_hdr->to_node = to;

            if ((_os.msg_list == NULL) || (os_msg_hdr->prio < _os.msg_list->prio)) {
                os_msg_hdr->next = _os.msg_list;
                _os.msg_list = os_msg_hdr;
            }
            else {
                struct nrc_os_msg_hdr *msg = _os.msg_list;

                while ((msg->next != NULL) && (os_msg_hdr->prio >= msg->next->prio)) {
                    msg = msg->next;
                }
                os_msg_hdr->next = msg->next;
                msg->next = os_msg_hdr;
            }

            result = nrc_port_sema_signal(_os.sema);
            assert(result == NRC_PORT_RES_OK);

            result = NRC_PORT_RES_OK;
        }
    }

    return result;
}

s32_t nrc_os_send_evt(nrc_node_t to, u32_t event_mask, s8_t prio)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if ((to != NULL) && (event_mask != 0) && (prio < S8_MAX_VALUE)) {

        struct nrc_os_node_hdr *node = (struct nrc_os_node_hdr*)to - 1;

        if (node->type == NRC_OS_NODE_TYPE) {

            result = nrc_port_irq_disable();
            assert(result == NRC_PORT_RES_OK);

            node->evt = node->evt | event_mask;

            if (prio < node->prio) {
                node->prio = prio;
                increased_node_prio(node);
            }

            result = nrc_port_irq_enable();
            assert(result == NRC_PORT_RES_OK);

            result = nrc_port_sema_signal(_os.sema);
            assert(result == NRC_PORT_RES_OK);
        }
    }
    
    return result;
}

static void extract_node(struct nrc_os_node_hdr* node)
{
    assert(node != NULL);

    if (node->next != NULL) {
        node->next->previous = node->previous;
    }
    if (node->previous != NULL) {
        node->previous->next = node->next;
    }
    node->next = NULL;
    node->previous = NULL;
}

static void insert_node(struct nrc_os_node_hdr* node)
{
    assert(node != NULL);

    if (_os.node_list == NULL) {
        _os.node_list = node;
    }
    else {
        struct nrc_os_node_hdr *pos = _os.node_list;
        struct nrc_os_node_hdr *prev_pos = NULL;

        while ((pos != NULL) && (pos->prio <= node->prio)) {
            prev_pos = pos;
            pos = pos->next;
        }

        //One of pos and prev_pos must be non-zero.
        if (pos != NULL) {
            //Place node before pos
            node->next = pos;
            node->previous = pos->previous;
            pos->previous = node;

            if (node->previous != NULL) {
                node->previous->next = node;
            }
            else {
                _os.node_list = node;
            }
        }
        else {
            //Place node after prev_pos
            assert(prev_pos != NULL);

            node->next = prev_pos->next;
            prev_pos->next = node;
            node->previous = prev_pos;

            if (node->next != NULL) {
                node->next->previous = node;
            }
        }
    }
}

static void increased_node_prio(struct nrc_os_node_hdr *node)
{
    assert(node != NULL);

    if ((node->previous != NULL) && (node->prio < node->previous->prio))
    {
        extract_node(node);

        insert_node(node);
    }
}

static void clear_evt(struct nrc_os_node_hdr *node)
{
    assert(node != NULL);

    node->evt = 0;
    node->prio = S8_MAX_VALUE;

    extract_node(node);
    insert_node(node);
}

static void nrc_os_thread_fcn(void)
{
    s32_t   result;

    u32_t                   evt;
    s8_t                    evt_prio;
    struct nrc_os_node_hdr  *evt_node = NULL;

    struct nrc_os_msg_hdr   *msg = NULL;
    s8_t                    msg_prio;
    struct nrc_os_node_hdr  *msg_node = NULL;

    while (_os.state != NRC_OS_S_INVALID) {
        result = nrc_port_sema_wait(_os.sema, 0);
        assert(result == NRC_PORT_RES_OK);

        msg_prio = (_os.msg_list != NULL) ? _os.msg_list->prio : S8_MAX_VALUE;

        do {
            // Get first event (highest prio)
            result = nrc_port_irq_disable();
            evt_prio = _os.node_list->prio;
            if (evt_prio < S8_MAX_VALUE) {
                evt_node = _os.node_list;
                evt = _os.node_list->evt;
                clear_evt(evt_node);
            }
            result = nrc_port_irq_enable();

            // Send messages with higher prio than event
            while (msg_prio < evt_prio) {
                msg = _os.msg_list;
                _os.msg_list = msg->next;

                msg_node = (struct nrc_os_node_hdr*)(msg->to_node);

                if (msg_node->api->recv_msg != NULL) {
                    msg_node->api->recv_msg((msg_node + 1), (msg + 1));
                }
                else {
                    NRC_LOGI("nrc_os", "Receiving node %s has no recv_msg fcn", msg_node->cfg_id);
                    nrc_os_msg_free(msg + 1);
                }
                msg_prio = (_os.msg_list != NULL) ? _os.msg_list->prio : S8_MAX_VALUE;
            }

            // Send event
            if ((evt_prio < S8_MAX_VALUE) && (evt_node->api->recv_evt != 0)) {
                evt_node->api->recv_evt((evt_node + 1), evt);
            }

        } while ((evt_prio != S8_MAX_VALUE) || (msg_prio != S8_MAX_VALUE));
    }
}

static void init_registered_nodes(void)
{
    struct nrc_os_node_hdr *hdr = _os.node_list;

    while (hdr != NULL) {
        hdr->api->init(hdr + 1);

        hdr = hdr->next;
    }
}

static void start_registered_nodes(void)
{
    struct nrc_os_node_hdr *hdr = _os.node_list;

    while (hdr != NULL) {
        hdr->api->start(hdr + 1);

        hdr = hdr->next;
    }

}

