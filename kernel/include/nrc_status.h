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

#ifndef _NRC_STATUS_H_
#define _NRC_STATUS_H_

#include "nrc_types.h"
#include "nrc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NRC_STATUS_STARTED      (1)
#define NRC_STATUS_STOPPED      (2)
#define NRC_STATUS_COMPLETED    (3)
#define NRC_STATUS_ERROR        (4)
#define NRC_STATUS_CONNECTED    (5)
#define NRC_STATUS_DISCONNECTED (6)
#define NRC_STATUS_CONNECTING   (7)

struct nrc_status {
    const s8_t              *topic;
    s8_t                    prio;

    s32_t                   status;
    const s8_t              *text;
};

/**
* @brief Initialises the status component
*
* @return NRC_R_OK if successful
*/
s32_t nrc_status_init(void);

/**
* @brief Set status
*
* @param group Z parameter in configuration of node (tab in node-red GUI)
* @param node Node setting the status
* @param status Status info to set
*
* @return NRC_R_OK if successful
*/
s32_t nrc_status_set(const s8_t *group, nrc_node_t node, struct nrc_status status);

/**
* @brief Register self as listener to status updates
*
* When a status is updated, a status message is sent to registered listeners
*
* @param group Z parameter in configuration of node (flow tab in node-red GUI). NULL for all
* @param node Node to register as a listener
*
* @return NRC_R_OK if successful
*/
s32_t nrc_status_start_listen(const s8_t *group, nrc_node_t listener_node);

/**
* @brief Stop listen to status updates
*
* @param node Node to stop listening
*
* @return NRC_R_OK if successful
*/
s32_t nrc_status_stop_listen(nrc_node_t listener_node);


#ifdef __cplusplus
}
#endif

#endif
