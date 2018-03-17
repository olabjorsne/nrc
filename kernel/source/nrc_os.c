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
#include "nrc_assert.h"
#include "nrc_cfg.h"
#include <string.h>

#include "nrc_factory.h"  // TODO: Move factory functions to nrc_os??
 
#define NRC_OS_STACK_SIZE   (4096)
#define NRC_OS_NODE_TYPE    (0xA5A5)
#define NRC_OS_MSG_TYPE     (0x5A5A)

#define NRC_OS_MSG_DEFAULT_PRIO (16)

enum nrc_os_state {
    NRC_OS_S_INVALID = 0,
    NRC_OS_S_CREATED,
    NRC_OS_S_INITIALIZED,
    NRC_OS_S_STARTED
};

struct nrc_os_register_node_pars {
    struct nrc_node_api *api;
    const s8_t          *cfg_id;
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

    u32_t               wire_cnt;
    nrc_node_t          *wire;

    u32_t               evt;
    s8_t                prio;
    s8_t                padding[3];

    u32_t               type;
};

struct nrc_os {
    enum nrc_os_state           state;

    struct nrc_cfg_t            *flow_cfg;

    nrc_port_thread_t           thread;
    nrc_port_sema_t             sema;
    nrc_port_mutex_t            mutex;

    struct nrc_os_node_hdr      *node_list;
    struct nrc_os_msg_hdr       *msg_list;
};

static void insert_node(struct nrc_os_node_hdr *node);
static void extract_node(struct nrc_os_node_hdr *node);
static void increased_node_prio(struct nrc_os_node_hdr *node);

static s32_t register_node(nrc_node_t node, struct nrc_os_register_node_pars pars);

static void init_registered_nodes(void);
static void deinit_registered_nodes(void);
static void start_registered_nodes(void);
static void stop_registered_nodes(void);
static void free_registered_nodes(void);
static void free_messages(void);

static s32_t get_wires(struct nrc_os_node_hdr *node);

static s32_t parse_flow_cfg(void);

static void nrc_os_thread_fcn(void);

static struct nrc_os    _os;
static bool_t           _created = FALSE;
static const s8_t       *_tag = "nrc_os";

s32_t nrc_os_init(void)
{
    s32_t result = NRC_R_ERROR;

    if (!_created) {
        _created = TRUE;
        _os.state = NRC_OS_S_CREATED;
    }

    switch (_os.state) {
    case NRC_OS_S_CREATED:
        NRC_ASSERT(sizeof(struct nrc_os_msg_hdr) % 4 == 0);
        NRC_ASSERT(sizeof(struct nrc_os_msg_tail) % 4 == 0);
        NRC_ASSERT(sizeof(struct nrc_os_node_hdr) % 4 == 0);

        memset(&_os, 0, sizeof(struct nrc_os));

        result = nrc_port_init();
        NRC_ASSERT(result == NRC_R_OK);

        result = nrc_port_sema_init(0, &_os.sema);
        NRC_ASSERT(result == NRC_R_OK);

        result = nrc_port_mutex_init(&_os.mutex);
        NRC_ASSERT(result == NRC_R_OK);

        result = nrc_port_thread_init(
            NRC_PORT_THREAD_PRIO_NORMAL,
            NRC_OS_STACK_SIZE,
            nrc_os_thread_fcn,
            &(_os.thread));
        NRC_ASSERT(result == NRC_R_OK);

        _os.state = NRC_OS_S_INITIALIZED;
        result = NRC_R_OK;
        break;

    default:
        NRC_LOGW(_tag, "init: invalid state %d", _os.state);
        result = NRC_R_INVALID_STATE;
        break;
    }

    return result;
}

static void start_flow(struct nrc_cfg_t *flow_cfg)
{
    s32_t result;

    _os.flow_cfg = flow_cfg;
    result = nrc_cfg_set_active(flow_cfg);
    NRC_ASSERT(result == NRC_R_OK);

    // Parse configuration; instantiate nodes
    result = parse_flow_cfg();
    NRC_ASSERT(result == NRC_R_OK);

    // Init and start all nodes
    init_registered_nodes();
    start_registered_nodes();
}

static void stop_flow(void)
{
    // Stop and deinit nodes
    stop_registered_nodes();
    deinit_registered_nodes();

    // Free node objects, messages and deactivate configuration
    free_registered_nodes();
    free_messages();

    // Deactivate current configuration
    //nrc_cfg_destroy(_os.flow_cfg); // TODO: function not (yet) implemented
}

s32_t nrc_os_start(struct nrc_cfg_t *flow_cfg)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if (flow_cfg != NULL) {
        switch (_os.state) {
        case NRC_OS_S_INITIALIZED:
            NRC_ASSERT(_os.flow_cfg == NULL);

            start_flow(flow_cfg);

            _os.state = NRC_OS_S_STARTED;

            // Start msg and event handling thread
            result = nrc_port_thread_start(_os.thread);
            NRC_ASSERT(result == NRC_R_OK);
            break;

        case NRC_OS_S_STARTED:
            stop_flow();
            start_flow(flow_cfg);
            break;

        default:
            NRC_LOGD("os", "nrc_os_start: invalid state %d", _os.state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    
    return result;
}

/*
s32_t nrc_os_stop(bool_t application_nodes_only)
{
    s32_t result = NRC_R_NOT_SUPPORTED;

    switch (_os.state) {
    case NRC_OS_S_STARTED:
        if (application_nodes_only) {
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
        if (!application_nodes_only) {
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
*/

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

static s32_t register_node(nrc_node_t node, struct nrc_os_register_node_pars pars)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if ((node != NULL) && (pars.api != NULL) && (pars.cfg_id != NULL) &&
        (pars.api->init != NULL) && (pars.api->deinit != NULL) && (pars.api->start != NULL) && (pars.api->stop != NULL)) {

        struct nrc_os_node_hdr *os_node_hdr = (struct nrc_os_node_hdr*)node - 1;

        if (os_node_hdr->type == NRC_OS_NODE_TYPE) {

            os_node_hdr->api = pars.api;
            os_node_hdr->cfg_id = pars.cfg_id;
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

        while ((!found) && (hdr != NULL)) {
            if (strcmp(cfg_id, hdr->cfg_id) == 0) {
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
        hdr->prio = NRC_OS_MSG_DEFAULT_PRIO;
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
        NRC_ASSERT(os_msg_header->type == NRC_OS_MSG_TYPE);

        os_msg_tail = (struct nrc_os_msg_tail*)((u8_t*)os_msg_header + os_msg_header->total_size) - 1;
        NRC_ASSERT(os_msg_tail->dead_beef == 0xDEADBEEF);

        hdr = hdr->next;

        nrc_port_heap_fast_free(os_msg_header);
    }
}

s32_t nrc_os_send_msg_to(nrc_node_t to, nrc_msg_t msg, s8_t prio)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if ((to != NULL) && (msg != NULL)) {

        struct nrc_os_node_hdr  *os_node_hdr = (struct nrc_os_node_hdr*)to - 1;
        struct nrc_os_msg_hdr   *os_msg_hdr = (struct nrc_os_msg_hdr*)msg - 1;

        if ((os_node_hdr->type == NRC_OS_NODE_TYPE) && (os_msg_hdr->type == NRC_OS_MSG_TYPE)) {

            if (prio < S8_MAX_VALUE) {
                os_msg_hdr->prio = prio;
            }
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
            NRC_ASSERT(result == NRC_R_OK);

            result = NRC_R_OK;
        }
    }

    return result;
}

s32_t nrc_os_send_msg_from(nrc_node_t from, nrc_msg_t msg, s8_t prio)
{
    s32_t                   result = NRC_R_INVALID_IN_PARAM;
    u32_t                   i;
    nrc_msg_t               msg_clone = NULL;

    if ((from != NULL) && (msg != NULL)) {
        struct nrc_os_node_hdr  *node = (struct nrc_os_node_hdr*)from - 1;

        if (node->wire_cnt == 0) {
            nrc_os_msg_free(msg);
            result = NRC_R_OK;
        }
        else if (node->wire_cnt == 1) {
            result = nrc_os_send_msg_to(node->wire[0], msg, prio);
        }
        else {
            // Send cloned messages to all wires but one
            for (i = 0; i < (node->wire_cnt - 1); i++) {
                    msg_clone = nrc_os_msg_clone(msg);
                    if (msg_clone != NULL) {
                        result = nrc_os_send_msg_to(node->wire[i], msg_clone, prio);
                        if (!OK(result)) {
                            NRC_LOGW(_tag, "send_msg_from: Error %d", result);
                        }
                    }
                    else {
                        NRC_LOGW(_tag, "send_msg_from: Out of memory");
                    }               
            }
            // Send original message to last wire
            result = nrc_os_send_msg_to(node->wire[node->wire_cnt - 1], msg, prio);
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
            //result = nrc_port_irq_disable();
            //NRC_ASSERT(result == NRC_R_OK);

            // Set event bit(s)
            node->evt = node->evt | event_mask;

            // If new event is higher prio than previous ones make sure to update place in list
            if (prio < node->prio) {
                node->prio = prio;
                increased_node_prio(node);
            }

            // Enable IRQs again
            //result = nrc_port_irq_enable();
            //NRC_ASSERT(result == NRC_R_OK);

            // Signal thread that a new event is available
            result = nrc_port_sema_signal(_os.sema);
            NRC_ASSERT(result == NRC_R_OK);
        }
    }
    
    return result;
}

s32_t nrc_os_lock(void)
{
    return nrc_port_mutex_lock(_os.mutex, 0);
}

s32_t nrc_os_unlock(void)
{
    return nrc_port_mutex_unlock(_os.mutex);
}

static void extract_node(struct nrc_os_node_hdr* node)
{
    NRC_ASSERT(node != NULL);

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
    NRC_ASSERT(node != NULL);

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
            NRC_ASSERT(prev_pos != NULL);
            NRC_ASSERT(prev_pos->next == NULL);

            node->next = NULL;
            prev_pos->next = node;
            node->previous = prev_pos;
        }
    }
}

static void increased_node_prio(struct nrc_os_node_hdr *node)
{
    NRC_ASSERT(node != NULL);

    if ((node->previous != NULL) && (node->prio < node->previous->prio))
    {
        extract_node(node);

        insert_node(node);
    }
}

static void clear_evt(struct nrc_os_node_hdr *node)
{
    NRC_ASSERT(node != NULL);

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
        NRC_ASSERT(result == NRC_R_OK);

        // Make sure thread locks the nrc layer
        result = nrc_os_lock();
        NRC_ASSERT(result == NRC_R_OK);

        // Get prio of highest priority msg (if any)
        msg_prio = (_os.msg_list != NULL) ? _os.msg_list->prio : S8_MAX_VALUE;

        // If there is an event with highest prio, call node recv_evt function
        if ((_os.node_list->prio < S8_MAX_VALUE) && (_os.node_list->prio <= msg_prio)) {
            evt_node = _os.node_list;
            evt = _os.node_list->evt;

            // Clear event and re-order node to back of list
            clear_evt(evt_node);

            // Call node recevie event function
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
       
        result = nrc_os_unlock();
        NRC_ASSERT(result == NRC_R_OK);
    }
}

static void init_registered_nodes(void)
{
    s32_t                   result;
    struct nrc_os_node_hdr  *hdr = _os.node_list;

    while (hdr != NULL) {
        result = get_wires(hdr);
        if (!OK(result)) {
            NRC_LOGE(_tag, "init_registered_nodes: Failed get_wires %s", hdr->cfg_id);
        }

        result = hdr->api->init(hdr + 1);
        NRC_LOGI(_tag, "init(%s): result %d", hdr->cfg_id, result);

        hdr = hdr->next;
    }
}

static void start_registered_nodes(void)
{
    struct nrc_os_node_hdr  *hdr = _os.node_list;
    s32_t                   result;

    while (hdr != NULL) {
        result = hdr->api->start(hdr + 1);
        NRC_LOGI(_tag, "start(%s): result %d", hdr->cfg_id, result);

        hdr = hdr->next;
    }
}
static void deinit_registered_nodes(void)
{
    struct nrc_os_node_hdr  *hdr = _os.node_list;
    s32_t                   result;

    while (hdr != NULL) {
        result = hdr->api->deinit(hdr + 1);
        NRC_LOGI(_tag, "deinit(%s): result %d", hdr->cfg_id, result);

        hdr = hdr->next;
    }
}

static void free_registered_nodes(void)
{
    struct nrc_os_node_hdr *hdr = _os.node_list;
    struct nrc_os_node_hdr *free_node = NULL;

    while (hdr != NULL) {
        free_node = hdr;
        hdr = hdr->next;

        nrc_port_heap_free(free_node);
    }

    _os.node_list = NULL;
}

static void free_messages(void)
{
    struct nrc_os_msg_hdr *msg = _os.msg_list;
    struct nrc_os_msg_hdr *free_msg = _os.msg_list;

    while (msg != NULL) {
        free_msg = msg;
        msg = msg->next;

        nrc_port_heap_fast_free(free_msg);
    }
}

static void stop_registered_nodes(void)
{
    struct nrc_os_node_hdr  *hdr = _os.node_list;
    s32_t                   result;

    while (hdr != NULL) {
        result = hdr->api->stop(hdr + 1);
        NRC_LOGI(_tag, "stop(%s): result %d", hdr->cfg_id, result);

        hdr = hdr->next;
    }
}

static s32_t get_wires(struct nrc_os_node_hdr *node)
{
    s32_t       result = NRC_R_OK;
    u32_t       i;
    s8_t        *cfg_wire = NULL;
    u32_t       cnt = 0;
    u32_t       max_wires = 0;
    nrc_node_t  *wire = NULL;

    if (node != NULL) {
        // Free previously allocated wires
        if (node->wire != NULL) {
            nrc_port_heap_free(node->wire);
        }
        node->wire_cnt = 0;

        // Get number of new wires
        while (OK(result)) {
            result = nrc_cfg_get_str_from_array(node->cfg_id, "wires", max_wires, &cfg_wire);
            if (OK(result)) {
                max_wires++;
            }
        }

        // Allocate wire arrary
        node->wire = (nrc_node_t*)nrc_port_heap_alloc(max_wires * sizeof(nrc_node_t));
        NRC_ASSERT(node->wire != NULL);

        // Read wires from nrc_cfg
        result = NRC_R_OK;
        for (i = 0; (i < max_wires) && OK(result); i++) {
            result = nrc_cfg_get_str_from_array(node->cfg_id, "wires", i, &cfg_wire);
            if (OK(result)) {
                wire = nrc_os_node_get(cfg_wire);
                if (wire != NULL) {
                    node->wire[cnt] = wire;
                    cnt++;
                }
            }
        }
        node->wire_cnt = cnt; // Might be lower than max_wires if nodes are not (yet) registered

        result = NRC_R_OK;
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

static s32_t parse_flow_cfg(void)
{
    s32_t                               result = NRC_R_OK;
    struct nrc_node_factory_pars        f_pars;
    struct nrc_os_register_node_pars    n_pars;
    nrc_node_t                          node;

    for (u32_t i = 0; OK(result); i++) {
        result = nrc_cfg_get_node(i, &f_pars.cfg_type, &f_pars.cfg_id, &f_pars.cfg_name);
        if (OK(result) && (f_pars.cfg_type != NULL) && (f_pars.cfg_id != NULL) && (f_pars.cfg_name != NULL)) {
            node = nrc_factory_create_node(&f_pars);
            if (node) {
                n_pars.api = f_pars.api;
                n_pars.cfg_id = f_pars.cfg_id;
                s32_t res = register_node(node, n_pars);
                NRC_LOGI(_tag, "register(%s - %s): result %d", f_pars.cfg_name, f_pars.cfg_id, res);
             }
            else {
                NRC_LOGE(_tag, "Node not supported: type=\"%s\", id=\"%s\", name=\"%s\"", f_pars.cfg_type, f_pars.cfg_id, f_pars.cfg_name);
            }
        }
    }
    return NRC_R_OK;
}