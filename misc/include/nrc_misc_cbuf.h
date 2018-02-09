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

/**
* @brief Initialises a new circular buffer
*
* @param buf The buffer for storage of data
* @param buf_size Number of bytes of the provided buffer
* @param cbuf Output parameter where the circular buffer identifier is stored
*
* @return NRC_R_OK is successful
*/
s32_t nrc_misc_cbuf_init(u8_t *buf, u32_t buf_size, nrc_misc_cbuf_t *cbuf);

/**
* @brief Frees a previously initiated circular buffer
*
* @param cbuf Identifies the circular buffer
*
* @return NRC_R_OK is successful
*/
s32_t nrc_misc_cbuf_deinit(nrc_misc_cbuf_t cbuf);

/**
* @brief Checks if the buffer is empty
*
* @param cbuf Identifies the circular buffer
*
* @return TRUE if empty and FALSE if not
*/
bool_t nrc_misc_cbuf_is_empty(nrc_misc_cbuf_t cbuf);

/**
* @brief Checks if the buffer is full
*
* @param cbuf Identifies the circular buffer
*
* @return TRUE if full and FALSE if not
*/
bool_t nrc_misc_cbuf_is_full(nrc_misc_cbuf_t cbuf);

/**
* @brief Gets the number of bytes stored in circular buffer
*
* @param cbuf Identifies the circular buffer
*
* @return Number of bytes
*/
u32_t nrc_misc_cbuf_get_bytes(nrc_misc_cbuf_t cbuf);

/**
* @brief Clears the circular buffer
*
* @param cbuf Identifies the circular buffer
*
* @return NRC_R_OK if successful
*/
s32_t nrc_misc_cbuf_clear(nrc_misc_cbuf_t cbuf);

/**
* @brief Reads data from the circular buffer
*
* @param cbuf Identifies the circular buffer
* @param buf Buffer to read data into
* @param buf_size Maximum number of bytes to read
*
* @return Number of read bytes
*/
u32_t nrc_misc_cbuf_read(nrc_misc_cbuf_t cbuf, u8_t *buf, u32_t buf_size);

/**
* @brief Gets a buffer with data to read from
*
* The call must be followed by a call to nrc_misc_cbuf_read_buf_consumed.
*
* @param cbuf Identifies the circular buffer
* @param buf_size Output parameter where the number of bytes that can be read is stored
*
* @return Pointer to the read buffer
*/
u8_t* nrc_misc_cbuf_get_read_buf(nrc_misc_cbuf_t cbuf, u32_t *buf_size);

/**
* @brief Call to inform number of read bytes for the previous nrc_misc_cbuf_get_read_buf call
*
* The call must followed a call to nrc_misc_cbuf_get_read_buf
*
* @param cbuf Identifies the circular buffer
* @param bytes Number of bytes that was actually read from the buffer
*
* @return void
*/
void nrc_misc_cbuf_read_buf_consumed(nrc_misc_cbuf_t cbuf, u32_t bytes);

/**
* @brief Writes data into the circular buffer
*
* @param cbuf Identifies the circular buffer
* @param buf Buffer with data to write
* @param buf_size Number of bytes to write
*
* @return Number of bytes written
*/
u32_t nrc_misc_cbuf_write(nrc_misc_cbuf_t cbuf, u8_t *buf, u32_t buf_size);

/**
* @brief Gets a buffer to write data into the circular buffer
*
* The call must be followed by a call to nrc_misc_cbuf_write_buf_consumed.
*
* @param cbuf Identifies the circular buffer
* @param buf_size Output parameter where the number of bytes that can be written is stored
*
* @return Pointer to the write buffer
*/
u8_t* nrc_misc_cbuf_get_write_buf(nrc_misc_cbuf_t cbuf, u32_t *buf_size);

/**
* @brief Call to inform number of written bytes for the previous nrc_misc_cbuf_get_write_buf call
*
* The call must follow a call to nrc_misc_cbuf_get_write_buf
*
* @param cbuf Identifies the circular buffer
* @param bytes Number of bytes that was actually written into the write buffer
*
* @return void
*/
void nrc_misc_cbuf_write_buf_consumed(nrc_misc_cbuf_t cbuf, u32_t bytes);

#endif
