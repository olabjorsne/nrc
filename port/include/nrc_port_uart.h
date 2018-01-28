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

s32_t nrc_port_uart_init(void);

s32_t nrc_port_uart_open(
    u8_t                                port,
    struct nrc_port_uart_pars           pars,
    struct nrc_port_uart_callback_fcn   callback,
    nrc_port_uart_t                     *uart);

s32_t nrc_port_uart_close(nrc_port_uart_t uart);

s32_t nrc_port_uart_write(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size);

u32_t nrc_port_uart_read(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size);

u32_t nrc_port_uart_get_bytes(nrc_port_uart_t uart);

#endif