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
#include "nrc_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Initialises the NRC OS component
*
* Must be first call to component
*
* @return NRC_R_OK is successful
*/
s32_t nrc_os_init(void);

/**
* @brief Starts the component
*
* Creates, initialises and starts all registered nodes according to the flow configuration
* A consecutive call stops, deinit and frees started nodes before starting again with the new flow.
*
* @param flow_cfg Identifies the flow to start (from nrc_cfg component)
* 
* @return NRC_R_OK is successful
*/
s32_t nrc_os_start(struct nrc_cfg_t *flow_cfg);

/**
* @brief Allocates memory for node structures
*
* Called from the create function of the node
*
* @return Node identifier (which is a pointer to the node structure)
*/
nrc_node_t nrc_os_node_alloc(u32_t size);

/**
* @brief Gets the node identifier from the configuration identifier
*
* @param cfg_id Configuration identifier of the node
*
* @return Node identifier
*/
nrc_node_t nrc_os_node_get(const s8_t *cfg_id);

/**
* @brief Allocates memory for a message
*
* @param size Number of bytes to allocate
*
* @return Message identifier, which is a pointer to the message
*/
nrc_msg_t nrc_os_msg_alloc(u32_t size);

/**
* @brief Clones a previously allocated message or chain of messages
*
* @param msg Message or chain of messages to clone
*
* @return Pointer to the new cloned message(s)
*/
nrc_msg_t nrc_os_msg_clone(nrc_msg_t msg);

/**
* @brief Frees a previously allocated message or chain of messages
*
* @param msg Message or chain of messages to free
*
* @return NRC_R_OK is successful
*/
void nrc_os_msg_free(nrc_msg_t msg);

/**
* @brief Send message to a specific node
*
* @param to Node to send message to
* @param msg Message or chain of messages to send
* @param prio Priority of the message
*
* @return NRC_R_OK is successful
*/
s32_t nrc_os_send_msg_to(nrc_node_t to, nrc_msg_t msg, s8_t prio);

/**
* @brief Send message to all registered wires (nodes)
*
* OS component will find the registered wires
*
* @param to Node to send message from
* @param msg Message or chain of messages to send
* @param prio Priority of the message
*
* @return NRC_R_OK is successful
*/
s32_t nrc_os_send_msg_from(nrc_node_t from, nrc_msg_t msg, s8_t prio);

/**
* @brief Send event to a specific node
*
* Typically used by nrc layer for callbacks to nodes
*
* @param to Node to send event to
* @param event_mask Event(s) to send
* @param prio Priority of the event
*
* @return NRC_R_OK is successful
*/
s32_t nrc_os_send_evt(nrc_node_t to, u32_t event_mask, s8_t prio);

/**
* @brief Lock nrc_os until nrc_unlock is called
*
* @return NRC_R_OK is successful
*/
s32_t nrc_os_lock(void);

/**
* @brief Unlocks nrc_os previously locked by nrc_lock.
*
* @return NRC_R_OK is successful
*/
s32_t nrc_os_unlock(void);

#ifdef __cplusplus
}
#endif

#endif