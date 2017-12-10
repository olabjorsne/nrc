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

#define NRC_MSG_TYPE_NULL           (0)
#define NRC_MSG_TYPE_INT            (1)
#define NRC_MSG_TYPE_STRING         (2)
#define NRC_MSG_TYPE_BUF            (3)
#define NRC_MSG_TYPE_DATA_AVAILABLE (4)

#ifdef __cplusplus
extern "C" {
#endif

typedef void* nrc_msg_t;

struct nrc_msg_hdr {
    struct nrc_msg_hdr  *next;
    const s8_t          *topic;
    u32_t               type;
};

struct nrc_msg_int {
    struct nrc_msg_hdr  hdr;
    s32_t               value;
};

struct nrc_msg_str {
    struct nrc_msg_hdr  hdr;
    s8_t                str[NRC_EMTPY_ARRAY];
};

struct nrc_msg_buf {
    struct nrc_msg_hdr  hdr;
    u32_t               buf_size;
    u8_t                buf[NRC_EMTPY_ARRAY];
};

typedef u32_t(*nrc_msg_read_t)(u8_t *buf, u32_t buf_size);

struct nrc_msg_data_available {
    struct nrc_msg_hdr  hdr;
    nrc_msg_read_t      read;
};

#ifdef __cplusplus
}
#endif

#endif