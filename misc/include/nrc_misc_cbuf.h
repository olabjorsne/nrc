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

#ifndef _NRC_MISC_CBUF_H_
#define _NRC_MISC_CBUF_H_

#include "nrc_types.h"

typedef void *nrc_misc_cbuf_t;

s32_t nrc_misc_cbuf_init(u8_t *buf, u32_t buf_size, nrc_misc_cbuf_t *cbuf);

s32_t nrc_misc_cbuf_deinit(nrc_misc_cbuf_t cbuf);

bool_t nrc_misc_cbuf_is_empty(nrc_misc_cbuf_t cbuf);

bool_t nrc_misc_cbuf_is_full(nrc_misc_cbuf_t cbuf);

u32_t nrc_misc_cbuf_get_bytes(nrc_misc_cbuf_t cbuf);

u32_t nrc_misc_cbuf_read(nrc_misc_cbuf_t cbuf, u8_t *buf, u32_t buf_size);

u8_t* nrc_misc_cbuf_get_read_buf(nrc_misc_cbuf_t cbuf, u32_t *buf_size);

void nrc_misc_cbuf_read_buf_consumed(nrc_misc_cbuf_t cbuf, u32_t bytes);

u32_t nrc_misc_cbuf_write(nrc_misc_cbuf_t cbuf, u8_t *buf, u32_t buf_size);

u8_t* nrc_misc_cbuf_get_write_buf(nrc_misc_cbuf_t cbuf, u32_t *buf_size);

void nrc_misc_cbuf_write_buf_consumed(nrc_misc_cbuf_t cbuf, u32_t bytes);

#endif
