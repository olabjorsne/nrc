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

#include <string.h>
#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_log.h"
#include "nrc_factory.h"

#ifndef NRC_NODE_FACTORY_MAX
    #define NRC_NODE_FACTORY_MAX_NBR_OF_NODES 32
#endif

typedef struct 
{
    s8_t* cfg_type;
    nrc_node_create_t node_create;
} nrc_node_factory_t;

static const s8_t *_tag = "nrc_factory";
static nrc_node_factory_t factory[NRC_NODE_FACTORY_MAX_NBR_OF_NODES] = { 0 };


static u32_t find_free_index(void)
{
    u32_t i;
    for (i = 0; i < NRC_NODE_FACTORY_MAX_NBR_OF_NODES; i++) {
        if (factory[i].cfg_type == NULL) {
            break;
        }
    }
    return i;
}

static nrc_node_create_t find_create_function(const s8_t *cfg_type)
{
    nrc_node_create_t factory_function = NULL;
    for (u32_t i = 0; (factory[i].cfg_type != NULL) && (i < NRC_NODE_FACTORY_MAX_NBR_OF_NODES); i++) {
        if ((factory[i].cfg_type != NULL) &&
            (strcmp(factory[i].cfg_type, cfg_type) == 0)) {
            factory_function = factory[i].node_create;
            break;
        }
    }
    return factory_function;
}

s32_t nrc_factory_register_node_type(s8_t* cfg_type, nrc_node_create_t node_create)
{
    s32_t status = NRC_R_ERROR;

    if (find_create_function(cfg_type) != NULL) {
        NRC_LOGE(_tag, "Type \"%s\" already registered to factory", cfg_type);
        return status;
    }

    u32_t i = find_free_index();
    if (i < NRC_NODE_FACTORY_MAX_NBR_OF_NODES) {
        factory[i].cfg_type = cfg_type;
        factory[i].node_create = node_create;
        status = NRC_R_OK;
    }
    return status;
}

nrc_node_t nrc_factory_create_node(struct nrc_node_factory_pars *pars)
{
    nrc_node_t node = NULL;
    nrc_node_create_t create = NULL;

    if (pars->cfg_type == NULL || pars->cfg_id == NULL || pars->cfg_name == NULL) {
        return NULL;
    }

    create = find_create_function(pars->cfg_type);   
    if (create != NULL) {
        node = create(pars);
    }

    return node;
}