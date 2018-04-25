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

/**
 * @brief Initialize configuration class
 *
 * @return NRC_R_OK is return if operation succeeds
 */
 s32_t nrc_cfg_init(void);


/**
 * @brief Create a configuration object from a json formated flow configuration.  
 *        The configuration data is used by the configuration object.
 *        and must be available as long as the configuration is active.
 *
 * @param flow_config json formatted configuration, 
 *                    pass NULL to create an empty confguration 
 * @param config_size Length of configuration data
 *
 * @return A configuration object is returned if parsing of the json flow succeeds, NULL if parsing fails.
 */
nrc_cfg_t * nrc_cfg_create(const u8_t *flow_config, u32_t config_size);

/**
 * @brief Free a configuration  
 *
 * @param config configuration too be freed 
 *
 * @return None
 */
 void nrc_cfg_destroy(nrc_cfg_t* config);

/**
 * @brief Set actvie configuration. 
 *
 * @param config Configuration object
 *
 * @return NRC_R_OK is return if operation succeeds
 */
 s32_t nrc_cfg_set_active(nrc_cfg_t* config);

 /**
 * @brief Create and add a node to configuration
 *
 * @param config configuration to which the node shall be added
 * @param node_config json formatted node configuration
 *                    This configuration data is stored by the 
 *                    created node object and can is not used after this 
 *                    function returns.
 * @param config_size Length of configuration data
 * @param node_id (out) id of added node
 *
 * @return NRC_R_OK is return if operation succeeds
 */
 s32_t nrc_cfg_add_node(struct nrc_cfg_t *config, const u8_t *node_config, u32_t node_config_size, const s8_t **node_id);

 /**
 * @brief Remove a node from configuration.
 *
 * @param config configuration from which the node shall be removed
 * @param cfg_id id of node that shall be removed
 *
 * @return NRC_R_OK is return if operation succeeds
 */
 s32_t nrc_cfg_remove_node(struct nrc_cfg_t *config, const s8_t *cfg_id);

/**
 * @brief Iterator for reading node basic node configuration.
 *  
 * Nodes are numbered from 0 and up. To read all node configurations, 
 * call this function repeatedly until NRC_R_ERROR is returned. 
 * Data is read from the active configuration.
 *
 * @param index Node index 
 * @param cfg_type (out) Pointer to node type string 
 * @param cfg_id (out) Pointer to node id string
 * @param cfg_name (out) Pointer to node name string
 *
 * @return NRC_R_OK is return if id exists, NRC_R_ERROR if not 
 */
 s32_t nrc_cfg_get_node(u32_t index, const s8_t **cfg_type, const s8_t **cfg_id, const s8_t **cfg_name);

/**
 * @brief Get configuration string value  
 *
 * Value is read from the active configuration.
 *
 * @param cfg_id The id of the node
 * @param cfg_param_name The name of the configuration 
 * @param str (out) Configuration string value
 *
 * @return NRC_R_OK is return if operation succeeds
 */
 s32_t nrc_cfg_get_str(const s8_t *cfg_id, const s8_t *cfg_param_name, const  s8_t **str);

 /**
 * @brief Get configuration integer value
 *
 * Value is read from the active configuration.
 *
 * @param cfg_id The id of the node
 * @param cfg_param_name The name of the configuration
 * @param value (out) Configuration integer value
 *
 * @return NRC_R_OK is return if operation succeeds
 */
s32_t nrc_cfg_get_int(const s8_t *cfg_id, const s8_t *cfg_param_name, s32_t *value);

/**
 * @brief Get string value from string array configuratoin 
 *
 * Value is read from the active configuration.
 *
 * @param cfg_id The id of the node
 * @param cfg_array_name The name of the array configuration
 * @param index index in the array
 * @param str Configuration string value 
 *
 * @return NRC_R_OK is return if operation succeeds
 */
 s32_t nrc_cfg_get_str_from_array(const s8_t *cfg_id, const s8_t *cfg_arr_name, u8_t index, const s8_t **str);


 /**
 * @brief Get integer value from integer array configuratoin
 *
 * Value is read from the active configuration.
 *
 * @param cfg_id The id of the node
 * @param cfg_array_name The name of the array configuration
 * @param index index in the array
 * @param str (out) Configuration integer value
 *
 * @return NRC_R_OK is return if operation succeeds
 */
 s32_t nrc_cfg_get_int_from_array(const s8_t *cfg_type, const s8_t *cfg_id, s8_t const *cfg_arr_name, u8_t index, s32_t *value);

#ifdef __cplusplus
}
#endif

#endif
