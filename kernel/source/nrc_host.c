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

#include "nrc_types.h"
#include "nrc_assert.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_timer.h"
#include "nrc_log.h"
#include "nrc_node_factory.h"
#include <string.h>

static const s8_t *_tag = "config";

//TBD - host should be a node as well
//      For a start a it is only parsing the 
//      configuration during initialization
//      the config / factory api could be simplified
//      by using only id/node as parameter/output from factory 
s32_t nrc_host_init(void)
{
    return NRC_R_OK;
}


s32_t nrc_host_start(void)
{
    s32_t status = NRC_R_OK;
    struct nrc_node_factory_pars f_pars;
    struct nrc_os_register_node_pars n_pars;
    nrc_node_t node;   
    u8_t *json_config;
    u32_t json_config_size;
    nrc_cfg_t* config = NULL;

    status = nrc_port_get_config(&json_config, &json_config_size);
    if (OK(status)) {
        config =  nrc_cfg_create(json_config, json_config_size);
        NRC_ASSERT(config);
        status = nrc_cfg_set_active(config);
        NRC_ASSERT(status == NRC_R_OK);
    }

    for (u32_t i = 0; OK(status); i++) {
        status = nrc_cfg_get_node(i, &f_pars.cfg_type, &f_pars.cfg_id, &f_pars.cfg_name);
        if (OK(status) && f_pars.cfg_type && f_pars.cfg_id && f_pars.cfg_name) { 
            node = nrc_factory_create(&f_pars);
            if (node) {
                n_pars.api = f_pars.api;
                n_pars.cfg_id = f_pars.cfg_id;
                s32_t res = nrc_os_node_register(FALSE, node, n_pars);
                if (OK(res)) {
                    NRC_LOGI(_tag, "Node added: type=\"%s\", id=\"%s\", name=\"%s\"", f_pars.cfg_type, f_pars.cfg_id, f_pars.cfg_name);
                }
                else {
                    NRC_LOGE(_tag, "Failed to add node (error=%d): type=\"%s\", id=\"%s\", name=\"%s\"", res, f_pars.cfg_type, f_pars.cfg_id, f_pars.cfg_name);
                }
            }
            else {
                NRC_LOGE(_tag, "Node not supported: type=\"%s\", id=\"%s\", name=\"%s\"", f_pars.cfg_type, f_pars.cfg_id, f_pars.cfg_name);
            }
        }
    }            
    return NRC_R_OK;
}