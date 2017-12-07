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

#ifndef _NRC_TIMER_H_
#define _NRC_TIMER_H_

#include "nrc_types.h"
#include "nrc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* nrc_timer_id_t;

struct nrc_timer_tag {
    nrc_node_id_t   node_id;
    u32_t           evt;
    s8_t            prio;

    nrc_timer_id_t  timer_id; // Will be set by nrc_set_timer if successful
};

s32_t nrc_timer_init(void);

s32_t nrc_set_timer(u32_t timeout, struct nrc_timer_tag *tag);
s32_t nrc_abort_timer(nrc_timer_id_t timer_id);

#ifdef __cplusplus
}
#endif

#endif
