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

s32_t nrc_cfg_init(u32_t *cfg_address);
s32_t nrc_cfg_deinit(void);

s32_t nrc_cfg_get_node(u32_t index, s8_t *cfg_type, s8_t *cfg_id, u32_t max_str_len);

s32_t nrc_cfg_get_str(s8_t *cfg_type, s8_t *cfg_id, s8_t *cfg_param_name, s8_t *str, uint32_t max_str_len);
s32_t nrc_cfg_get_int(s8_t *cfg_type, s8_t *cfg_id, s8_t *cfg_param_name, s32_t *value);

s32_t nrc_cfg_get_str_from_array(s8_t *cfg_type, s8_t *cfg_id, s8_t *cfg_arr_name, u8_t index, s8_t *str, uint32_t max_str_len);
s32_t nrc_cfg_get_int_from_array(s8_t *cfg_type, s8_t *cfg_id, s8_t *cfg_arr_name, u8_t index, s32_t *value);

#ifdef __cplusplus
}
#endif

#endif
