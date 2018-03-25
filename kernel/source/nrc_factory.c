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

typedef struct nrc_node_factory_t
{
    struct nrc_node_factory_t  *next;
    s8_t                       *cfg_type;
    nrc_node_create_t          node_create;
} nrc_node_factory_t;

static const s8_t *_tag = "nrc_factory";
static nrc_node_factory_t *_nrc_node_factory = NULL;

static nrc_node_create_t find_create_function(const s8_t *cfg_type)
{
    nrc_node_factory_t *factory = _nrc_node_factory;
    while (factory != NULL) {
        if (strcmp(factory->cfg_type, cfg_type) == 0) {
            return factory->node_create;
        }
        factory = factory->next;
    }
    return NULL;
}

s32_t nrc_factory_register_node_type(s8_t* cfg_type, nrc_node_create_t node_create)
{
    s32_t result = NRC_R_ERROR;
    nrc_node_factory_t *new_factory = NULL;
   
    if (find_create_function(cfg_type) == NULL) {        
        result = NRC_R_OK;
    }
    else {
        NRC_LOGE(_tag, "%s already registered to node factory", cfg_type);
    }
    
    if (OK(result)) {
        new_factory = (nrc_node_factory_t*)nrc_port_heap_alloc(sizeof(nrc_node_factory_t));
        if (new_factory != NULL) {
            new_factory->cfg_type = cfg_type;
            new_factory->node_create = node_create;

            new_factory->next = _nrc_node_factory;
            _nrc_node_factory = new_factory;

            result = NRC_R_OK;
        }
        else {
            result = NRC_R_OUT_OF_MEM;
        }
    }

    return result;
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