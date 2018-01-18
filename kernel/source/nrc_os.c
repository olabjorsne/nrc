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
    NRC_OS_S_CREATED,
    NRC_OS_S_INITIALIZED,
    NRC_OS_S_STARTED_KERNAL,
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

    bool_t              kernal_node;

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
static void start_registered_nodes(bool_t kernal_nodes_only);
static void init_registered_nodes(bool_t kernal_nodes_only);
static void stop_registered_nodes(bool_t application_nodes_only);
static void deinit_registered_nodes(bool_t application_nodes_only);

static void nrc_os_thread_fcn(void);

static struct nrc_os    _os;
static bool_t           _created = FALSE;

s32_t nrc_os_init(void)
{
    s32_t result = NRC_R_ERROR;

    if (_created == FALSE) {
        _created = TRUE;
        _os.state = NRC_OS_S_CREATED;
    }

    switch (_os.state) {
    case NRC_OS_S_CREATED:
        assert(sizeof(struct nrc_os_msg_hdr) % 4 == 0);
        assert(sizeof(struct nrc_os_msg_tail) % 4 == 0);
        assert(sizeof(struct nrc_os_node_hdr) % 4 == 0);

        memset(&_os, 0, sizeof(struct nrc_os));

        result = nrc_port_init();
        assert(result == NRC_R_OK);

        result = nrc_port_sema_init(0, &_os.sema);
        assert(result == NRC_R_OK);

        result = nrc_port_thread_init(
            NRC_PORT_THREAD_PRIO_NORMAL,
            NRC_OS_STACK_SIZE,
            nrc_os_thread_fcn,
            &(_os.thread));
        assert(result == NRC_R_OK);

        _os.state = NRC_OS_S_INITIALIZED;
        result = NRC_R_OK;
        break;

    default:
        NRC_LOGD("os", "init: invalid state %d", _os.state);
        result = NRC_R_INVALID_STATE;
        break;
    }

    return result;
}

s32_t nrc_os_deinit(void)
{
    s32_t result = NRC_R_NOT_SUPPORTED;

    //TODO: Dealloc all nodes, messages, events, etc..
    
    return result;
}

s32_t nrc_os_start(bool_t kernal_nodes_only)
{
    s32_t result = NRC_R_INVALID_STATE;

    switch (_os.state) {
    case NRC_OS_S_INITIALIZED:
        if (kernal_nodes_only == TRUE) {
            // Init and start kernal nodes only
            init_registered_nodes(TRUE);
            start_registered_nodes(TRUE);

            _os.state = NRC_OS_S_STARTED_KERNAL;
        }
        else {
            // Init and start all nodes
            init_registered_nodes(TRUE);
            init_registered_nodes(FALSE);
            start_registered_nodes(TRUE);
            start_registered_nodes(FALSE);

            _os.state = NRC_OS_S_STARTED;
        }

        // Start msg and event handling thread
        result = nrc_port_thread_start(_os.thread);
        assert(result == NRC_R_OK);
        break;

    case NRC_OS_S_STARTED_KERNAL:
        if (kernal_nodes_only == FALSE) {
            // Init and start all remaining nodes
            init_registered_nodes(FALSE);
            start_registered_nodes(FALSE);

            _os.state = NRC_OS_S_STARTED;
        }
        else {
            NRC_LOGD("os", "nrc_os_start: invalid state %d", _os.state);
            result = NRC_R_INVALID_STATE;
        }
        break;
    default:
        NRC_LOGD("os", "nrc_os_start: invalid state %d", _os.state);
        result = NRC_R_INVALID_STATE;
        break;
    }
    
    return result;
}

s32_t nrc_os_stop(bool_t application_nodes_only)
{
    s32_t result = NRC_R_NOT_SUPPORTED;

    switch (_os.state) {
    case NRC_OS_S_STARTED:
        if (application_nodes_only == TRUE) {
            // Stop and deinit application nodes (not kernal nodes)
            stop_registered_nodes(FALSE);
            deinit_registered_nodes(FALSE);

            _os.state = NRC_OS_S_STARTED_KERNAL;
        }
        else {
            // Stop and deinit all nodes
            stop_registered_nodes(FALSE);
            stop_registered_nodes(TRUE);
            deinit_registered_nodes(FALSE);
            deinit_registered_nodes(TRUE);

            _os.state = NRC_OS_S_INITIALIZED;
        }
        break;

    case NRC_OS_S_STARTED_KERNAL:
        if (application_nodes_only == FALSE) {
            // Stop and deinit remaining nodes
            stop_registered_nodes(TRUE); 
            deinit_registered_nodes(TRUE);

            _os.state = NRC_OS_S_INITIALIZED;
        }
        else {
            NRC_LOGD("os", "nrc_os_stop: invalid state %d", _os.state);
            result = NRC_R_INVALID_STATE;
        }
        break;
    default:
        NRC_LOGD("os", "nrc_os_start: invalid state %d", _os.state);
        result = NRC_R_INVALID_STATE;
        break;
    }
    
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

s32_t nrc_os_node_register(bool_t kernal_node, nrc_node_t node, struct nrc_os_register_node_pars pars)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if ((node != NULL) && (pars.api != NULL) && (pars.cfg_id != NULL) &&
        (pars.api->init != NULL) && (pars.api->deinit != NULL) && (pars.api->start != NULL) && (pars.api->stop != NULL)) {

        struct nrc_os_node_hdr *os_node_hdr = (struct nrc_os_node_hdr*)node - 1;

        if (os_node_hdr->type == NRC_OS_NODE_TYPE) {

            os_node_hdr->api = pars.api;
            os_node_hdr->cfg_id = pars.cfg_id;
            os_node_hdr->kernal_node = kernal_node;
            os_node_hdr->prio = S8_MAX_VALUE;
            os_node_hdr->evt = 0;

            insert_node(os_node_hdr);

            result = NRC_R_OK;
        }
    }

    return result;
}

nrc_node_t nrc_os_node_get(const s8_t *cfg_id)
{
    nrc_node_t  node = NULL;

    if (cfg_id != NULL) {
        bool_t                  found = FALSE;
        struct nrc_os_node_hdr  *hdr = _os.node_list;

        while ((found == FALSE) && (hdr != NULL)) {
            if (strncmp(cfg_id, hdr->cfg_id, NRC_MAX_CFG_NAME_LEN) == 0) {
                found = TRUE;
                node = hdr + 1;
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
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if ((to != NULL) && (msg != NULL) && (prio < S8_MAX_VALUE)) {

        struct nrc_os_node_hdr  *os_node_hdr = (struct nrc_os_node_hdr*)to - 1;
        struct nrc_os_msg_hdr   *os_msg_hdr = (struct nrc_os_msg_hdr*)msg - 1;

        if ((os_node_hdr->type == NRC_OS_NODE_TYPE) && (os_msg_hdr->type == NRC_OS_MSG_TYPE)) {

            os_msg_hdr->prio = prio;
            os_msg_hdr->to_node = os_node_hdr;

            if ((_os.msg_list == NULL) || (os_msg_hdr->prio < _os.msg_list->prio)) {
                // Insert new message first in list
                os_msg_hdr->next = _os.msg_list;
                _os.msg_list = os_msg_hdr;
            }
            else {
                struct nrc_os_msg_hdr *msg = _os.msg_list;

                // Find location where new message shall be inserted.
                // After msg and before msg->next
                while ((msg->next != NULL) && (os_msg_hdr->prio >= msg->next->prio)) {
                    msg = msg->next;
                }
                os_msg_hdr->next = msg->next;
                msg->next = os_msg_hdr;
            }

            // Signal to inform thread that a new msg is available
            result = nrc_port_sema_signal(_os.sema);
            assert(result == NRC_R_OK);

            result = NRC_R_OK;
        }
    }

    return result;
}

s32_t nrc_os_send_evt(nrc_node_t to, u32_t event_mask, s8_t prio)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if ((to != NULL) && (event_mask != 0) && (prio < S8_MAX_VALUE)) {

        struct nrc_os_node_hdr *node = (struct nrc_os_node_hdr*)to - 1;

        if (node->type == NRC_OS_NODE_TYPE) {

            // Disable IRQ during insertion of event
            result = nrc_port_irq_disable();
            assert(result == NRC_R_OK);

            // Set event bit(s)
            node->evt = node->evt | event_mask;

            // If new event is higher prio than previous ones make sure to update place in list
            if (prio < node->prio) {
                node->prio = prio;
                increased_node_prio(node);
            }

            // Enable IRQs again
            result = nrc_port_irq_enable();
            assert(result == NRC_R_OK);

            // Signal thread that a new event is available
            result = nrc_port_sema_signal(_os.sema);
            assert(result == NRC_R_OK);
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
    else {
        _os.node_list = node->next;
    }
    node->next = NULL;
    node->previous = NULL;
}

static void insert_node(struct nrc_os_node_hdr* node)
{
    assert(node != NULL);

    if (_os.node_list == NULL) {
        _os.node_list = node;

        node->next = NULL;
        node->previous = NULL;
    }
    else {
        struct nrc_os_node_hdr *pos = _os.node_list;
        struct nrc_os_node_hdr *prev_pos = NULL;

        // Find correct location after prev_pos and before pos
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
            assert(prev_pos->next == NULL);

            node->next = NULL;
            prev_pos->next = node;
            node->previous = prev_pos;
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
    struct nrc_os_node_hdr  *evt_node = NULL;

    struct nrc_os_msg_hdr   *msg = NULL;
    s8_t                    msg_prio;
    struct nrc_os_node_hdr  *msg_node = NULL;

    while (_os.state > NRC_OS_S_INITIALIZED) {
        // Wait for msg or event
        result = nrc_port_sema_wait(_os.sema, 0);
        assert(result == NRC_R_OK);

        do {
            // Get prio of highest priority msg (if any)
            msg_prio = (_os.msg_list != NULL) ? _os.msg_list->prio : S8_MAX_VALUE;

            // Get node event with higher prio than highest prio msg (if any)
            result = nrc_port_irq_disable();
            // Node list is sorted with highest event priority first
            if ((_os.node_list->prio < S8_MAX_VALUE) && (_os.node_list->prio <= msg_prio)) {
                evt_node = _os.node_list;
                evt = _os.node_list->evt;

                // Clear event and re-order node to back of list
                clear_evt(evt_node);
            }
            else {
                evt_node = NULL;
            }
            result = nrc_port_irq_enable();

            // If there is an event with highest prio, call node recv_evt function
            if (evt_node != NULL) {
                if (evt_node->api->recv_evt != NULL) {
                    evt_node->api->recv_evt((evt_node + 1), evt);
                }
                else {
                    NRC_LOGI("nrc_os", "Receiving node %s has no recv_evt fcn", evt_node->cfg_id);
                }
            }
            // Else if there is a msg with highest prio, call node recv_msg function
            else if (msg_prio < S8_MAX_VALUE) {
                // Msg with highest prio is first in list

                // Extract first msg
                msg = _os.msg_list;
                _os.msg_list = msg->next;

                msg_node = msg->to_node;

                if (msg_node->api->recv_msg != NULL) {
                    msg_node->api->recv_msg((msg_node + 1), (msg + 1));
                }
                else {
                    NRC_LOGI("nrc_os", "Receiving node %s has no recv_msg fcn", msg_node->cfg_id);
                    nrc_os_msg_free(msg + 1);
                }
            }

        } while ((evt_node != NULL) || (msg_prio < S8_MAX_VALUE));
    }
}

static void init_registered_nodes(bool_t kernal_node)
{
    struct nrc_os_node_hdr *hdr = _os.node_list;

    while (hdr != NULL) {
        if (kernal_node == hdr->kernal_node) {
            hdr->api->init(hdr + 1);
        }
        hdr = hdr->next;
    }
}

static void start_registered_nodes(bool_t kernal_node)
{
    struct nrc_os_node_hdr *hdr = _os.node_list;

    while (hdr != NULL) {
        if (kernal_node == hdr->kernal_node) {
            hdr->api->start(hdr + 1);
        }
        hdr = hdr->next;
    }
}
static void deinit_registered_nodes(bool_t kernal_node)
{
    struct nrc_os_node_hdr *hdr = _os.node_list;

    while (hdr != NULL) {
        if (kernal_node == hdr->kernal_node) {
            hdr->api->deinit(hdr + 1);
        }
        hdr = hdr->next;
    }
}

static void stop_registered_nodes(bool_t kernal_node)
{
    struct nrc_os_node_hdr *hdr = _os.node_list;

    while (hdr != NULL) {
        if (kernal_node == hdr->kernal_node) {
            hdr->api->stop(hdr + 1);
        }
        hdr = hdr->next;
    }
}

