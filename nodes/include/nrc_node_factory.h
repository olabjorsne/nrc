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

#ifndef _NRC_NODE_FACTORY_H_
#define _NRC_NODE_FACTORY_H_

#include "nrc_types.h"
#include "nrc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nrc_node_factory_pars {
    // In parameters
    const s8_t          *cfg_type;
    const s8_t          *cfg_id;
    const s8_t          *cfg_name;

    // Out parameters
    struct nrc_node_api *api;
};

extern nrc_node_t nrc_factory_create_inject(struct nrc_node_factory_pars *pars);
extern nrc_node_t nrc_factory_create_debug(struct nrc_node_factory_pars *pars);

#ifdef __cplusplus
}
#endif

#endif