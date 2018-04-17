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

#ifndef _NRC_MSG_H_
#define _NRC_MSG_H_

#include "nrc_types.h"
#include "nrc_defs.h"
//#include "nrc_node.h"

#define NRC_MSG_TYPE_INVALID        (0) //Shall not be used
#define NRC_MSG_TYPE_EMPTY          (1)
#define NRC_MSG_TYPE_INT            (2)
#define NRC_MSG_TYPE_STRING         (3)
#define NRC_MSG_TYPE_BUF            (4)
#define NRC_MSG_TYPE_DATA_AVAILABLE (5)
#define NRC_MSG_TYPE_STATUS         (6)

#ifdef __cplusplus
extern "C" {
#endif

typedef void* nrc_msg_t;

/**
* @brief Message header that every message type must start with
*/
struct nrc_msg_hdr {
    struct nrc_msg_hdr  *next;  // Messages can be linked
    const s8_t          *topic; // Topic of the message
    u32_t               type;   // Type to describe what specific message it is
};

/**
* @brief Message representing an integer value
*/
struct nrc_msg_int {
    struct nrc_msg_hdr  hdr;
    s32_t               value;
};

/**
* @brief Message representing a null terminated string
*/
struct nrc_msg_str {
    struct nrc_msg_hdr  hdr;
    s8_t                str[NRC_EMTPY_ARRAY];
};

/**
* @brief Message representing a byte array
*/
struct nrc_msg_buf {
    struct nrc_msg_hdr  hdr;
    u32_t               buf_size;
    u8_t                buf[NRC_EMTPY_ARRAY];
};

typedef u32_t(*nrc_msg_read_t)(void *node, u8_t *buf, u32_t buf_size);

/**
* @brief Message representing a data_available callback
*/
struct nrc_msg_data_available {
    struct nrc_msg_hdr  hdr;
    void                *node;  // Node to read data from
    nrc_msg_read_t      read;   // Read function. If 0 bytes is returned, a new
};                              // data_available message shall be sent when new data is available
 
/**
* @brief Message representing a node status update
*/
struct nrc_msg_status {
    struct nrc_msg_hdr  hdr;
    void                *node;                  // Node with status update
    s32_t               status;                 // Status code (see nrc_status.h)
    s8_t                text[NRC_EMTPY_ARRAY];  // 0 terminated string
};

#ifdef __cplusplus
}
#endif

#endif