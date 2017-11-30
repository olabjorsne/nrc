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

s32_t nrc_os_init(void);
s32_t nrc_os_deinit(void);

s32_t nrc_os_start(void);
s32_t nrc_os_stop(void);

struct nrc_node_hdr* nrc_os_node_alloc(u32_t size);
s32_t nrc_os_register_node(struct nrc_node_hdr *node, struct nrc_node_api *api, const s8_t *cfg_id);

s32_t nrc_os_get_node_id(const s8_t *cfg_id, nrc_node_id_t *id);

struct nrc_msg_hdr* nrc_os_msg_alloc(u32_t size);
struct nrc_msg_hdr* nrc_os_msg_clone(struct nrc_msg_hdr *msg);
void nrc_os_msg_free(struct nrc_msg_hdr *msg);

s32_t nrc_os_send_msg(nrc_node_id_t id, struct nrc_msg_hdr *msg, s8_t prio);
s32_t nrc_os_set_evt(nrc_node_id_t id, u32_t event_mask, s8_t prio);

#ifdef __cplusplus
}
#endif

#endif