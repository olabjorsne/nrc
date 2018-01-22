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

#include "nrc_serial.h"
#include "nrc_port_uart.h"
#include "nrc_cfg.h"
#include <string.h>

#define NRC_SERIAL_TYPE (0x1438B4AA)

enum nrc_serial_state {
    NRC_SERIAL_S_CLOSED = 0,
    NRC_SERIAL_S_IDLE,
    NRC_SERIAL_S_BUSY
};

struct nrc_serial {
    struct nrc_serial           *next;

    s8_t                        port;
    const s8_t                  *cfg_id_settings;

    nrc_port_uart_t             uart;

    struct nrc_port_uart_pars   pars;

    bool_t                      open;

    enum nrc_serial_state       rx_state;
    struct nrc_serial_reader    reader;

    enum nrc_serial_state       tx_state;
    struct nrc_serial_writer    writer;

    u32_t                       type;
};

static s32_t get_settings(const s8_t *cfg_id, u8_t *port, struct nrc_port_uart_pars *pars);
static struct nrc_serial* get_serial(const s8_t *cfg_id_settings);

static void data_available(nrc_port_uart_t uart);
static void write_complete(nrc_port_uart_t uart, s32_t result, u32_t bytes);
static void error(nrc_port_uart_t uart, s32_t error);

static bool_t                               _initialized = FALSE;
static struct nrc_serial                    *_serial = NULL;
static struct nrc_port_uart_callback_fcn    _callback;

s32_t nrc_serial_init(void)
{
    s32_t result = NRC_R_OK;

    if (_initialized != TRUE) {
        _initialized = TRUE;
        _serial = NULL;

        memset(&_callback, 0, sizeof(struct nrc_port_uart_callback_fcn));

        _callback.data_available = data_available;
        _callback.write_complete = write_complete;
        _callback.error = error;

        result = nrc_port_uart_init();
    }

    return result;
}

s32_t nrc_serial_open_reader(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_reader    reader,
    nrc_serial_t                *serial_id)
{
    s32_t                       result = NRC_R_OK;
    struct nrc_serial           *serial = NULL;

    if ((cfg_id_settings != NULL) && (reader.node != NULL) &&
        (reader.data_available_evt != 0) && (reader.error_evt != 0)) {

        *serial_id = NULL;

        serial = get_serial(cfg_id_settings);

        if (serial != NULL) {
            if (serial->reader.node == NULL) {
                serial->reader = reader;
                serial->rx_state = NRC_SERIAL_S_IDLE;
            }
            else {
                result = NRC_R_UNAVAILABLE_RESOURCE;
            }
        }
        else {
            serial = (struct nrc_serial*)nrc_port_heap_alloc(sizeof(struct nrc_serial));

            if (serial != NULL) {
                memset(serial, 0, sizeof(struct nrc_serial));

                serial->cfg_id_settings = cfg_id_settings;
                serial->reader = reader;
                serial->rx_state = NRC_SERIAL_S_IDLE;
                serial->open = FALSE;

                serial->next = _serial;
                _serial = serial;
            }
            else {
                result = NRC_R_OUT_OF_MEM;
            }
        }

        if (result == NRC_R_OK) {
            result = get_settings(cfg_id_settings, &serial->port, &serial->pars);
        }

        if ((result == NRC_R_OK) && (serial->open == FALSE)) {
            result = nrc_port_uart_open(serial->port, serial->pars, _callback, &serial->uart);

            if (result == NRC_R_OK) {
                serial->open = TRUE;
                serial->rx_state = NRC_SERIAL_S_IDLE;
                serial->tx_state = NRC_SERIAL_S_IDLE;
            }
        }

        if (result == NRC_R_OK) {
            *serial_id = serial; 
        }
        else {
            memset(&serial->reader, 0, sizeof(struct nrc_serial_reader));
        }
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

s32_t nrc_serial_close_reader(nrc_serial_t serial)
{
    s32_t result = NRC_R_OK;
    struct nrc_serial *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE)) {
        if ((self->open == TRUE) && (self->reader.node != NULL)) {
            memset(&self->reader, 0, sizeof(struct nrc_serial_reader));
            self->rx_state = NRC_SERIAL_S_IDLE;

            if (self->writer.node == NULL) {
                self->open = FALSE;

                result = nrc_port_uart_close(self->uart);
                self->uart = NULL;
            }
        }
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

u32_t nrc_serial_read(nrc_serial_t serial, u8_t *buf, u32_t buf_size)
{
    u32_t               read_bytes = 0;
    struct nrc_serial   *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE) &&
        (buf != NULL) && (buf_size > 0)) {

        read_bytes = nrc_port_uart_read(self->uart, buf, buf_size);
    }

    return read_bytes;
}

s32_t nrc_serial_get_read_error(nrc_serial_t serial)
{
    s32_t error = NRC_R_OK;

    return error;
}

static struct nrc_serial* get_serial(const s8_t *cfg_id_settings)
{
    struct nrc_serial* serial = _serial;

    while ((serial != NULL) && (serial->cfg_id_settings != cfg_id_settings)) {
        serial = serial->next;
    }

    return serial;
}

static s32_t get_settings(const s8_t *cfg_id, u8_t *port, struct nrc_port_uart_pars *pars)
{
    s32_t       result = NRC_R_OK;
    const s8_t  *cfg_type = NULL;

    if ((cfg_id == NULL) || (port == NULL) || (pars == NULL)) {
        result = NRC_R_INVALID_IN_PARAM;
    }
    if (result == NRC_R_OK) {
        result = nrc_cfg_get_str(curr_config, cfg_id, "type", &cfg_type);

        if (result == NRC_R_OK) {
            if ((cfg_type == NULL) || (strcmp("serial-port", cfg_type) != 0)) {
                result = NRC_R_INVALID_CFG;
            }
        }
    }
    if (result == NRC_R_OK) {
        s32_t p;
        result = nrc_cfg_get_int(curr_config, cfg_id, "port", &p);

        if (result == NRC_R_OK) {
            if ((p >= 0) && (p <= U8_MAX_VALUE)) {
                *port = (u8_t)p;
            }
            else {
                result = NRC_R_INVALID_CFG;
            }
        }
    }
    if (result == NRC_R_OK) {
        result = nrc_cfg_get_int(curr_config, cfg_id, "baudrate", &pars->baud_rate);
    }
    if (result == NRC_R_OK) {
        s32_t db;
        result = nrc_cfg_get_int(curr_config, cfg_id, "databits", &db);

        if (result == NRC_R_OK) {
            if ((db >= 7) && (db <= 8)) {
                pars->data_bits = (u8_t)db;
            }
            else {
                result = NRC_R_INVALID_CFG;
            }
        }  
    }
    if (result == NRC_R_OK) {
        const s8_t *cfg_parity = NULL;
        result = nrc_cfg_get_str(curr_config, cfg_id, "parity", &cfg_parity);

        if (result == NRC_R_OK) {
            if (strcmp("none", cfg_parity) == 0) {
                pars->parity = NRC_PORT_UART_PARITY_NONE;
            }
            else if (strcmp("odd", cfg_parity) == 0) {
                pars->parity = NRC_PORT_UART_PARITY_ODD;
            }
            else if (strcmp("even", cfg_parity) == 0) {
                pars->parity = NRC_PORT_UART_PARITY_EVEN;
            }
            else {
                result = NRC_R_INVALID_CFG;
            }
        }
    }
    if (result == NRC_R_OK) {
        s32_t sb;
        result = nrc_cfg_get_int(curr_config, cfg_id, "stopbits", &sb);

        if (result == NRC_R_OK) {
            if ((sb >= 1) && (sb <= 2)) {
                pars->stop_bits = (u8_t)sb;
            }
            else {
                result = NRC_R_INVALID_CFG;
            }
        }
    }
    if (result == NRC_R_OK) {
        const s8_t *cfg_flow_ctrl = NULL;
        result = nrc_cfg_get_str(curr_config, cfg_id, "flowcontrol", &cfg_flow_ctrl);

        if (result == NRC_R_OK) {
            if (strcmp("none", cfg_flow_ctrl) == 0) {
                pars->flow_ctrl = NRC_PORT_UART_FLOW_NONE;
            }
            else if (strcmp("hw", cfg_flow_ctrl) == 0) {
                pars->flow_ctrl = NRC_PORT_UART_FLOW_HW;
            }
            else {
                result = NRC_R_INVALID_CFG;
            }
        }
    }

    return result;
}

static void data_available(nrc_port_uart_t uart)
{

}
static void write_complete(nrc_port_uart_t uart, s32_t result, u32_t bytes)
{

}
static void error(nrc_port_uart_t uart, s32_t error)
{

}