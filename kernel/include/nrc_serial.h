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

#ifndef _NRC_SERIAL_H_
#define _NRC_SERIAL_H_

#include "nrc_types.h"
#include "nrc_port.h"
#include "nrc_node.h"

typedef void* nrc_serial_t;

struct nrc_serial_reader {
    u32_t                   data_available_evt;
    u32_t                   error_evt;
    nrc_node_t              node;
};

struct nrc_serial_writer {
    u32_t       write_complete_evt;
    u32_t       error_evt;
    nrc_node_t  node;
};

s32_t nrc_serial_init(void);

s32_t nrc_serial_open_reader(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_reader    reader_notification,
    nrc_serial_t                *serial);

s32_t nrc_serial_close_reader(nrc_serial_t serial);

u32_t nrc_serial_read(nrc_serial_t serial, u8_t *buf, u32_t buf_size);

s32_t nrc_serial_get_read_error(nrc_serial_t serial);


//u32_t nrc_serial_write(nrc_serial_t serial, u8_t *buf, u32_t buf_size);



#endif
