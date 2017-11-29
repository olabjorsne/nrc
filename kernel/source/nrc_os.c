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

#define NRC_OS_STACK_SIZE (4096)

enum nrc_os_state {
    NRC_OS_S_INVALID = 0,
    NRC_OS_S_INITIALIZED,
    NRC_OS_S_STARTED
};

struct nrc_os_event {
    struct nrc_os_event *next;
    struct nrc_os_event *previous;
    nrc_node_id_t       to_node_id;
    u32_t               event;
    uint8_t             prio;
};

struct nrc_os_msg_hdr {
    struct nrc_os_msg_hdr   *next;
    nrc_node_id_t           to_node_id;
    uint8_t                 prio;
    uint8_t                 padding[3];
    u32_t                   total_size;
    u32_t                   dead_beef;
};

struct nrc_os_msg_tail {
    u32_t dead_beef;
};

struct nrc_os_node {
    struct nrc_node     *node;
    struct nrc_node_api *api;
    const s8_t          *cfg_id;
    struct nrc_os_event *event;
};

struct nrc_os {
    enum nrc_os_state           state;

    nrc_port_thread_t           thread;
    nrc_port_sema_t             sema;

    u16_t                       node_list_max_cnt;
    u16_t                       node_list_cnt;
    struct nrc_os_node          *node_list;

    struct nrc_os_msg_hdr       *msg_list_head;
    struct nrc_os_msg_hdr       *msg_list_tail;
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

s32_t nrc_os_register_node(struct nrc_node *node, struct nrc_node_api *api, const s8_t *cfg_id)
{
    s32_t result = NRC_PORT_RES_OK;
    
    if (_os.node_list_cnt == _os.node_list_max_cnt) {
        u32_t   i;
        u32_t   size = sizeof(struct nrc_os_node*) * (8 + _os.node_list_max_cnt);

        struct nrc_os_node *p = (struct nrc_os_node*)nrc_port_heap_alloc(size);
        assert(p != NULL);

        memset(p, 0, size);

        for (i = 0; i < _os.node_list_max_cnt; i++) {
            p[i] = _os.node_list[i];
        }

        nrc_port_heap_free(_os.node_list);
        _os.node_list = p;
        _os.node_list_max_cnt += 8;
    }

    _os.node_list[_os.node_list_cnt].api = api;
    _os.node_list[_os.node_list_cnt].node = node;
    _os.node_list[_os.node_list_cnt].cfg_id = cfg_id;
    _os.node_list[_os.node_list_cnt].event = (struct nrc_os_event*)nrc_port_heap_alloc(sizeof(struct nrc_os_event));
    assert(_os.node_list[_os.node_list_cnt].event != NULL);

    memset(_os.node_list[_os.node_list_cnt].event, 0, sizeof(struct nrc_os_event));

    return result;
}

s32_t nrc_os_get_node_id(const s8_t *cfg_id, nrc_node_id_t *id)
{
    s32_t       result = NRC_PORT_RES_ERROR;
    
    *id = 0xFFFFFFFF;

    for (uint32_t i = 0; (i < _os.node_list_cnt) && (result == NRC_PORT_RES_ERROR); i++) {
        if (strcmp(cfg_id, _os.node_list[i].cfg_id) == 0) {
            *id = i | 0xBB000000;
            result = NRC_PORT_RES_OK;
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

    struct nrc_os_msg_hdr       *header = (struct nrc_os_msg_hdr*)nrc_port_heap_fast_alloc(total_size);
    struct nrc_msg_hdr          *msg = (struct nrc_msg_hdr*)(header + 1);
    struct nrc_os_msg_tail      *tail = (struct nrc_os_msg_tail*)((uint8_t*)msg + size);

    memset(&header, 0, sizeof(header));
    memset(&msg, 0, sizeof(struct nrc_msg_hdr));

    header->total_size = total_size;
    header->dead_beef = 0xDEADBEEF;
    tail->dead_beef = 0xDEADBEEF;
    
    return msg;
}

struct nrc_msg_hdr* nrc_os_msg_clone(struct nrc_msg_hdr *msg)
{
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

        //TODO: Check ->deadBeef x 2

        msg = msg->next;

        nrc_port_heap_fast_free(os_msg_header);
    }
}


s32_t nrc_os_send_msg(nrc_node_id_t id, struct nrc_msg_hdr *msg, enum nrc_os_prio prio)
{
    s32_t result = NRC_PORT_RES_ERROR;

    if (((id & 0xFF000000) == 0xBB000000) && (msg != 0)) {
        id &= 0x00FFFFFF;

        struct nrc_os_msg_hdr *header = (struct nrc_os_msg_hdr*)msg - 1;

        header->prio = prio;

        if (_os.msg_list_head == 0) {
            _os.msg_list_head = header;
            _os.msg_list_tail = header;
        }
        else {
            struct nrc_os_msg_hdr *pos = _os.msg_list_head;

            while ((header->prio < pos->prio) && (pos->next != 0)) {
            }
        }
    }
    return result;
}

s32_t nrc_os_set_evt(nrc_node_id_t id, u32_t event_mask, enum nrc_os_prio prio)
{
    s32_t status = 0;
    
    return status;
}