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

#ifndef _NRC_CFG_H_
#define _NRC_CFG_H_

#include "nrc_types.h"
#include "nrc_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nrc_cfg_t nrc_cfg_t;

//Hack to publish the current configuration
extern nrc_cfg_t* curr_config;

s32_t nrc_cfg_init(void);

nrc_cfg_t * nrc_cfg_create(const u8_t *p_config, u32_t config_size);
void nrc_cfg_destroy(nrc_cfg_t* config);

s32_t nrc_cfg_get_node(nrc_cfg_t* config, u32_t index, const s8_t **cfg_type, const s8_t **cfg_id, const s8_t **cfg_name);

s32_t nrc_cfg_get_str(nrc_cfg_t* config, const s8_t *cfg_id, const s8_t *cfg_param_name, const  s8_t **str);
s32_t nrc_cfg_get_int(nrc_cfg_t* config, const s8_t *cfg_id, const s8_t *cfg_param_name, s32_t *value);
s32_t nrc_cfg_get_str_from_array(nrc_cfg_t* config, const s8_t *cfg_id, const s8_t *cfg_arr_name, u8_t index, const s8_t **str);
s32_t nrc_cfg_get_int_from_array(nrc_cfg_t* config, const s8_t *cfg_type, const s8_t *cfg_id, s8_t const *cfg_arr_name, u8_t index, s32_t *value);

#ifdef __cplusplus
}
#endif

#endif
