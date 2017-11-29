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

#ifndef _NRC_DEFS_H_
#define _NRC_DEFS_H_

#include "nrc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NRC_EMTPY_ARRAY (1) //If C99 supported empty array supported

#define NRC_MAX_CFG_NAME_LEN    (32) //Max string length for id, type, name, etc.. in cfg

#define NRC_MAX_NODES           (64)
#define NRC_MAX_NODE_WIRES      (4)

#ifdef __cplusplus
}
#endif

#endif