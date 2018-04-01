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

#ifndef _NRC_DOUT_H_
#define _NRC_DOUT_H_

#include "nrc_types.h"
#include "nrc_port.h"
#include "nrc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

struct nrc_dout_node_pars {
    u32_t   buf_size;
};

typedef void* nrc_dout_stream_t;

typedef s32_t (*nrc_dout_stream_write_t)(nrc_dout_stream_t stream, u8_t *buf, u32_t buf_size);
typedef s32_t (*nrc_dout_stream_get_error_t)(nrc_dout_stream_t stream);

/**
* @brief Generic serial-out stream api
*/
struct nrc_dout_stream_api {
    nrc_dout_stream_t            id;          // Stream id (e.g. serial_in or tcp_in)
    nrc_dout_stream_write_t      write;       // Write function
};

enum nrc_dout_state {
    NRC_DOUT_S_INVALID,
    NRC_DOUT_S_IDLE,
    NRC_DOUT_S_TX_BUF,
    NRC_DOUT_S_TX_DATA_AVAIL,
    NRC_DOUT_S_TX_STRING
};

/**
* @brief Data in class structure
*/
struct nrc_dout {
    enum nrc_dout_state         state;

    struct nrc_dout_node_pars   node_pars;      // Node parameters
    struct nrc_dout_stream_api  stream_api;     // Generic stream api for reading data

    struct nrc_msg_buf          *msg_buf;       // Message for outstanding buf write
    struct nrc_msg_str          *msg_str;       // Message for outstanding string write

    nrc_node_t                  read_node;      // Node to read from
    nrc_msg_read_t              read_fcn;       // Read function of data available msg
    u8_t                        *data_avail_buf;// Buffer for data available writes

    u32_t                       type;           // Unique id of class type
};

/**
* @brief Starts the data-out sub-node
*
* @param dout Data-out sub-node
* @param node_pars Node parameters needed by sub-node
* @param stream_api Generic serial stream to write data to
*
* @return NRC_R_OK if successful
*/
s32_t nrc_dout_start(
    struct nrc_dout              *dout,
    struct nrc_dout_node_pars    node_pars,
    struct nrc_dout_stream_api   stream_api);

/**
* @brief Stops the data-out sub-stream
*
* @param dout Data-out sub-node
*
* @return NRC_R_OK if successful
*/
s32_t nrc_dout_stop(struct nrc_dout *dout);

/**
* @brief Data available function
*
* Called when the stream interface has more data to read
*
* @param dout Data-out sub-node
* @param msg Message for the data-out sub-node to receive
*
* @return NRC_R_OK if successful
*/
s32_t nrc_dout_recv_msg(struct nrc_dout  *dout, nrc_msg_t msg);

/**
* @brief Previous write is complete
*
* @param dout Data-out sub-node
*
* @return NRC_R_OK if successful
*/
s32_t nrc_dout_write_complete(struct nrc_dout *dout);

#ifdef __cplusplus
}
#endif

#endif

