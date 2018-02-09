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

#ifndef _NRC_NODE_H_
#define _NRC_NODE_H_

#include "nrc_types.h"
#include "nrc_defs.h"
#include "nrc_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* nrc_node_t;

/**
* @brief Function declarations for node api
*/
typedef s32_t (*nrc_node_init_t)(nrc_node_t self);
typedef s32_t (*nrc_node_deinit_t)(nrc_node_t self);
typedef s32_t (*nrc_node_start_t)(nrc_node_t self);
typedef s32_t (*nrc_node_stop_t)(nrc_node_t self);
typedef s32_t (*nrc_node_recv_msg_t)(nrc_node_t self, nrc_msg_t msg);
typedef s32_t (*nrc_node_recv_evt_t)(nrc_node_t self, u32_t event_mask);

/**
* @brief Function api that every node must implement
*/
struct nrc_node_api {
    nrc_node_init_t     init;       // Allocate memory, read configuration, etc..
    nrc_node_deinit_t   deinit;     // Free allocated memory
    nrc_node_start_t    start;      // Open resources and get ready to receive messages/events
    nrc_node_stop_t     stop;       // Close resources, and free memory allocated when started
    nrc_node_recv_msg_t recv_msg;   // Receive message or chain of messages sent to node
    nrc_node_recv_evt_t recv_evt;   // Receive events from nrc layer (typically from opened resources)
};

/**
* @brief A node structure must start with the following header
*/
struct nrc_node_hdr {
    const s8_t          *cfg_type;  // Node type as in configuration file
    const s8_t          *cfg_id;    // Node identifier as in configuration file
    const s8_t          *cfg_name;  // Node name as in configuration file
};

#ifdef __cplusplus
}
#endif

#endif