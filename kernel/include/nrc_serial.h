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

 /**
 * @brief Serial handle
 */
typedef void* nrc_serial_t;

/**
* @brief Serial reader data for callback event
*
* @param data_available_evt Event bit mask to receive when data is available
* @param error_evt Event bit mask to receive when there is a read error
* @param node Reader node that serial shall send the events to
* @param prio Priority of the events
*/
struct nrc_serial_reader {
    u32_t                   data_available_evt;
    u32_t                   error_evt;
    nrc_node_t              node;
    s8_t                    prio;
};

/**
* @brief Serial writer data for callback event
*
* @param write_complete_evt Event bit mask to receive when a write is completed
* @param error_evt Event bit mask to receive when there is a write error
* @param node Writer node that serial shall send the events to
* @param prio Priority of the events
*/
struct nrc_serial_writer {
    u32_t       write_complete_evt;
    u32_t       error_evt;
    nrc_node_t  node;
    s8_t        prio;
};

/**
* @brief Initialises the serial component
*
* @return NRC_R_OK if successful
*/
s32_t nrc_serial_init(void);

/**
* @brief Opens the serial port for reading
*
* @param cfg_id_settings Identifies the configuration node for the serial settings
* @param reader_notification Callback data for read events
* @param serial Output where the serial identifier is stored
*
* @return NRC_R_OK if successful
*/
s32_t nrc_serial_open_reader(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_reader    reader_notification,
    nrc_serial_t                *serial);

/**
* @brief Closes the serial port for reading
*
* @param serial Identifies the serial port
*
* @return NRC_R_OK if successful
*/
s32_t nrc_serial_close_reader(nrc_serial_t serial);

/**
* @brief Reads data from the serial port
*
* @param serial Identifies the serial port
* @param buf Buffer to read data into
* @param buf_size Maximum number of bytes to read into buffer
*
* @return Number of read bytes. If 0, there will be a new data_available event when there is new data.
*/
u32_t nrc_serial_read(nrc_serial_t serial, u8_t *buf, u32_t buf_size);

/**
* @brief Gets the number of available bytes to read
*
* @param serial Identifies the serial port
*
* @return Number of available bytes to read. If 0, there will be a new data_available event when there is new data.
*/
u32_t nrc_serial_get_bytes(nrc_serial_t serial);

/**
* @brief Clears the serial for reading
*
* There will be a data_available event when there is new data to read.
*
* @param serial Identifies the serial port
*
* @return NRC_R_OK if successful
*/
s32_t nrc_serial_clear(nrc_serial_t serial);

/**
* @brief Gets the last read error
*
* Clears the error once read
*
* @param serial Identifies the serial port
*
* @return Last read error. If no error, NRC_R_OK is returned
*/
s32_t nrc_serial_get_read_error(nrc_serial_t serial);

/**
* @brief Opens the serial port for writing
*
* @param cfg_id_settings Identifies the configuration node for the serial settings
* @param writer_notification Callback data for write events
* @param serial Output where the serial identifier is stored
*
* @return NRC_R_OK if successful
*/
s32_t nrc_serial_open_writer(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_writer    writer_notification,
    nrc_serial_t                *serial);

/**
* @brief Closes the serial port for writing
*
* @param serial Identifies the serial port
*
* @return NRC_R_OK if successful
*/
s32_t nrc_serial_close_writer(nrc_serial_t serial);

/**
* @brief Writes data to the serial port
*
* When the write is completed, a write_complete event is sent
*
* @param serial Identifies the serial port
* @param buf Buffer to write
* @param buf_size Size of buffer to write
*
* @return NRC_R_OK if successful
*/
s32_t nrc_serial_write(nrc_serial_t serial, u8_t *buf, u32_t buf_size);

/**
* @brief Gets the last write error
*
* Clears the error once read
*
* @param serial Identifies the serial port
*
* @return Last write error. If no error, NRC_R_OK is returned
*/
s32_t nrc_serial_get_write_error(nrc_serial_t serial);




#endif
