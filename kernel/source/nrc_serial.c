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
#include "nrc_os.h"
#include "nrc_log.h"
#include <string.h>

#define NRC_SERIAL_TYPE (0x1438B4AA)
#define NRC_SERIAL_PRIO (8) //TODO: Priority should be configurable

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
    s8_t                        priority;

    bool_t                      open;

    struct nrc_serial_reader    reader;

    enum nrc_serial_state       tx_state;
    struct nrc_serial_writer    writer;
    u32_t                       bytes_written;

    u32_t                       type;
};

static s32_t get_settings(const s8_t *cfg_id, u8_t *port, struct nrc_port_uart_pars *pars);
static struct nrc_serial* get_serial_from_cfg(const s8_t *cfg_id_settings);
static struct nrc_serial* get_serial_from_uart(nrc_port_uart_t uart);

static void data_available(nrc_port_uart_t uart, s32_t result);
static void write_complete(nrc_port_uart_t uart, s32_t result, u32_t bytes);

static bool_t                               _initialized = FALSE;
static struct nrc_serial                    *_serial = NULL;
static struct nrc_port_uart_callback_fcn    _callback;
static const s8_t                           *_tag = "nrc_serial";

s32_t nrc_serial_init(void)
{
    s32_t result = NRC_R_OK;

    if (_initialized != TRUE) {
        _initialized = TRUE;
        _serial = NULL;

        memset(&_callback, 0, sizeof(struct nrc_port_uart_callback_fcn));

        _callback.data_available = data_available;
        _callback.write_complete = write_complete;

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

        serial = get_serial_from_cfg(cfg_id_settings);

        if (serial != NULL) {
            if (serial->reader.node == NULL) {
                serial->reader = reader;
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
                serial->open = FALSE;

                serial->next = _serial;
                _serial = serial;
            }
            else {
                result = NRC_R_OUT_OF_MEM;
            }
        }

        if ((result == NRC_R_OK) && (serial->open == FALSE)) {
            result = get_settings(cfg_id_settings, &serial->port, &serial->pars);
        }

        if ((result == NRC_R_OK) && (serial->open == FALSE)) {
            result = nrc_port_uart_open(serial->port, serial->pars, _callback, &serial->uart);

            if (result == NRC_R_OK) {
                serial->open = TRUE;
                serial->priority = NRC_SERIAL_PRIO; //TODO: Configurable
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

u32_t nrc_serial_get_bytes(nrc_serial_t serial)
{
    u32_t               bytes = 0;
    struct nrc_serial   *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE)) {
        bytes = nrc_port_uart_get_bytes(self->uart);
    }

    return bytes;
}

s32_t nrc_serial_clear(nrc_serial_t serial)
{
    s32_t               result = NRC_R_INVALID_IN_PARAM;
    struct nrc_serial   *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE)) {
        result = nrc_port_uart_clear(self->uart);
    }

    return result;
}

s32_t nrc_serial_get_read_error(nrc_serial_t serial)
{
    s32_t error = NRC_R_OK;

    //TODO:

    return error;
}

static struct nrc_serial* get_serial_from_cfg(const s8_t *cfg_id_settings)
{
    struct nrc_serial* serial = _serial;

    while ((serial != NULL) && (serial->cfg_id_settings != cfg_id_settings)) {
        serial = serial->next;
    }

    return serial;
}

static struct nrc_serial* get_serial_from_uart(nrc_port_uart_t uart)
{
    struct nrc_serial* serial = _serial;

    while ((serial != NULL) && (serial->uart != uart)) {
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
            if ((cfg_type == NULL) || (strcmp("serial", cfg_type) != 0)) {
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
        result = nrc_cfg_get_str(curr_config, cfg_id, "flowctrl", &cfg_flow_ctrl);

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

static void data_available(nrc_port_uart_t uart, s32_t result)
{
    struct nrc_serial *serial = get_serial_from_uart(uart);

    if ((serial != NULL) && (serial->type == NRC_SERIAL_TYPE) &&
        (serial->open == TRUE) && (serial->reader.node != NULL)) {

        u32_t evt = serial->reader.data_available_evt;

        if (result != NRC_R_OK) {
            evt |= serial->reader.error_evt;
        }
        
        nrc_os_send_evt(serial->reader.node, evt, serial->priority);
    }
    else {
        NRC_LOGD(_tag, "data_available: invalid uart or no reader");
    }
}
static void write_complete(nrc_port_uart_t uart, s32_t result, u32_t bytes)
{
    struct nrc_serial *self = get_serial_from_uart(uart);

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE) && (self->open == TRUE) &&
        (self->writer.node != NULL) && (self->tx_state == NRC_SERIAL_S_BUSY)) {

        u32_t evt = self->writer.write_complete_evt;

        if (result != NRC_R_OK) {
            evt |= self->writer.error_evt;
        }

        if (self->bytes_written != bytes) {
            NRC_LOGD(_tag, "write_complete: Not correct number of bytes written (%d != %d)", self->bytes_written, bytes);
        }

        self->bytes_written = 0;
        self->tx_state = NRC_SERIAL_S_IDLE;

        nrc_os_send_evt(self->writer.node, evt, self->priority);
    }
    else {
        NRC_LOGD(_tag, "write_complete: invalid uart, state or no writer");
    }
}