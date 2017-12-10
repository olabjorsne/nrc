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

#ifndef _NRC_NODE_H_
#define _NRC_NODE_H_

#include "nrc_types.h"
#include "nrc_defs.h"
#include "nrc_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* nrc_node_t;

typedef s32_t (*nrc_node_init_t)(nrc_node_t self);
typedef s32_t (*nrc_node_deinit_t)(nrc_node_t self);
typedef s32_t (*nrc_node_start_t)(nrc_node_t self);
typedef s32_t (*nrc_node_stop_t)(nrc_node_t self);
typedef s32_t (*nrc_node_recv_msg_t)(nrc_node_t self, nrc_msg_t msg);
typedef s32_t (*nrc_node_recv_evt_t)(nrc_node_t self, u32_t event_mask);

struct nrc_node_api {
    nrc_node_init_t     init;
    nrc_node_deinit_t   deinit;
    nrc_node_start_t    start;
    nrc_node_stop_t     stop;
    nrc_node_recv_msg_t recv_msg;
    nrc_node_recv_evt_t recv_evt;
};

struct nrc_node_hdr {
    const s8_t          *cfg_type;
    const s8_t          *cfg_id;
    const s8_t          *cfg_name;
};

#ifdef __cplusplus
}
#endif

#endif