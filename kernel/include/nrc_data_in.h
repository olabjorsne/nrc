/**
* Copyright 2018 Tomas Frisberg & Ola Bjorsne
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

#ifndef _NRC_DIN_H_
#define _NRC_DIN_H_

#include "nrc_types.h"
#include "nrc_port.h"
#include "nrc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nrc_din_node_pars {
    nrc_node_t      node;
    const s8_t      *topic;
    const s8_t      *cfg_msg_type;
    s8_t            prio;
    u32_t           max_size;
};

typedef void* nrc_din_stream_t;

typedef u32_t (*nrc_din_stream_read_t)(nrc_din_stream_t stream, u8_t *buf, u32_t buf_size);
typedef u32_t (*nrc_din_stream_get_bytes_t)(nrc_din_stream_t stream);
typedef s32_t (*nrc_din_stream_clear_t)(nrc_din_stream_t stream);

/**
* @brief Generic serial stream api
*/
struct nrc_din_stream_api {
    nrc_din_stream_t            id;          // Stream id (e.g. serial_in or tcp_in)
    nrc_din_stream_read_t       read;        // Read function
    nrc_din_stream_get_bytes_t  get_bytes;   // Get number of bytes function
    nrc_din_stream_clear_t      clear;       // Clears any available data function
};

/**
* @brief Data in class structure
*/
struct nrc_din {
    struct nrc_din_node_pars    node_pars;      // Node parameters
    u32_t                       msg_type;       // Type of message to send (see nrc_msg.h)
    struct nrc_din_stream_api   stream_api;     // Generic stream api for reading data
    u32_t                       type;           // Unique id of class type
};

/**
* @brief Starts the data-in sub-node
*
* @param din Data in sub-node
* @param node_pars Node parameters
* @param stream_api Generic serial stream to read data from
*
* @return NRC_R_OK if successful
*/
s32_t nrc_din_start(
    struct nrc_din              *din,
    struct nrc_din_node_pars    node_pars,
    struct nrc_din_stream_api   stream_api);

/**
* @brief Stops the data-in sub-stream
*
* @param din Data in sub-node
*
* @return NRC_R_OK if successful
*/
s32_t nrc_din_stop(struct nrc_din *din);

/**
* @brief Data available function
*
* Called when the stream interface has more data to read
*
* @param din Data in sub-node
* @param more_to_read Return TRUE if there is more data to read.
*
* @return NRC_R_OK if successful
*/
s32_t nrc_din_data_available(
    struct nrc_din              *din,
    bool_t                      *more_to_read);

#ifdef __cplusplus
}
#endif

#endif

