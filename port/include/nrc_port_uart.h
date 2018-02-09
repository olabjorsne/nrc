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

#ifndef _NRC_PORT_UART_H_
#define _NRC_PORT_UART_H_

#include "nrc_types.h"
#include "nrc_port.h"

typedef void* nrc_port_uart_t;

enum nrc_port_serial_parity {
    NRC_PORT_UART_PARITY_NONE,
    NRC_PORT_UART_PARITY_ODD,
    NRC_PORT_UART_PARITY_EVEN
};

enum nrc_port_uart_flow_ctrl {
    NRC_PORT_UART_FLOW_NONE,
    NRC_PORT_UART_FLOW_HW
};

struct nrc_port_uart_pars {
    u32_t                           baud_rate;
    u8_t                            data_bits;
    enum nrc_port_serial_parity     parity;
    u8_t                            stop_bits;
    enum nrc_port_uart_flow_ctrl    flow_ctrl;
};

typedef void(*nrc_port_uart_data_available_t)(nrc_port_uart_t uart, s32_t result);
typedef void(*nrc_port_uart_write_complete_t)(nrc_port_uart_t uart, s32_t result, u32_t bytes);

struct nrc_port_uart_callback_fcn {
    nrc_port_uart_data_available_t    data_available;
    nrc_port_uart_write_complete_t    write_complete;
};

/**
* @brief Initialises the UART module.
*
* Must be called before any other function is called.
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_uart_init(void);

/**
* @brief Opens a UART for reading and writing
*
* When there is data to read the data_available callback function is called.
* Note, the data_available callback is only called when going from no data to read
* to data is available. Hence, caller must read data until no more data to read.
* When a write is completed, the write_complete callback is called. It is then possible
* to do another write.
*
* @param port UART port to open
* @param pars Parameters to open UART with
* @param callback Callback functions for data_available and write_complete
* @param uart Output parameter where the UART identifier is stored.
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_uart_open(
    u8_t                                port,
    struct nrc_port_uart_pars           pars,
    struct nrc_port_uart_callback_fcn   callback,
    nrc_port_uart_t                     *uart);

/**
* @brief Closes a previously openend UART
*
* @param uart UART identifier from the open call
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_uart_close(nrc_port_uart_t uart);

/**
* @brief Initiate writing
*
* Write is started and once completed the write_complete callback is called.
* Another write cannot be done until the complete callback is called.
*
* @param uart UART identifier from open call
* @param buf Buffer to write
* @param buf_size Number of bytes to write
*
* @return NRC_R_OK if call is successful.
*/
s32_t nrc_port_uart_write(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size);

/**
* @brief Reads data
*
* Synchronous call to read available data. If no data is available, the function
* returns 0. If 0 is returned, the data_available callback is called when
* there is new data available.
*
* @param uart UART identifier from open call
* @param buf Buffer to read data into
* @param buf_size Number of bytes to read
*
* @return Number of read bytes. 0 if no data is available.
*/
u32_t nrc_port_uart_read(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size);

/**
* @brief Gets number of bytes available for reading
*
* If 0 is returned, the data_available callback is called when
* there is new data available.
*
* @param uart UART identifier from open call
*
* @return Number of bytes. 0 if no data is available.
*/
u32_t nrc_port_uart_get_bytes(nrc_port_uart_t uart);

/**
* @brief Clears all available data
*
* The data_available callback is called when there is new data available.
*
* @param uart UART identifier from open call
*
* @return NRC_R_OK if successful
*/
s32_t nrc_port_uart_clear(nrc_port_uart_t uart);

#endif