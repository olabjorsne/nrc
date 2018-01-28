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

#ifndef _NRC_OS_H_
#define _NRC_OS_H_

#include "nrc_types.h"
#include "nrc_defs.h"
#include "nrc_node.h"
#include "nrc_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nrc_os_register_node_pars {
    struct nrc_node_api *api;
    const s8_t          *cfg_id;
};

s32_t nrc_os_init(void);
s32_t nrc_os_deinit(void);

s32_t nrc_os_start(bool_t kernal_nodes_only);
s32_t nrc_os_stop(bool_t application_nodes_only);

nrc_node_t nrc_os_node_alloc(u32_t size);
nrc_node_t nrc_os_node_get(const s8_t *cfg_id);

s32_t nrc_os_node_register(bool_t kernal_node, nrc_node_t node, struct nrc_os_register_node_pars pars);

nrc_msg_t nrc_os_msg_alloc(u32_t size);
nrc_msg_t nrc_os_msg_clone(nrc_msg_t msg);
void nrc_os_msg_free(nrc_msg_t msg);

s32_t nrc_os_send_msg_to(nrc_node_t to, nrc_msg_t msg, s8_t prio);
s32_t nrc_os_send_msg_from(nrc_node_t from, nrc_msg_t msg, s8_t prio);
s32_t nrc_os_send_evt(nrc_node_t to, u32_t event_mask, s8_t prio);

#ifdef __cplusplus
}
#endif

#endif