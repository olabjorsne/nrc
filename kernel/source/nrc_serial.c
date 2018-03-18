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
#include "nrc_port.h"
#include "nrc_port_uart.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_assert.h"
#include <string.h>

#define NRC_SERIAL_TYPE (0x1438B4AA)

enum nrc_serial_state {
    NRC_SERIAL_S_CLOSED = 0,
    NRC_SERIAL_S_IDLE,
    NRC_SERIAL_S_BUSY
};

struct nrc_serial {
    u32_t                       type;

    struct nrc_serial           *next;

    s8_t                        port;
    const s8_t                  *cfg_id_settings;

    nrc_port_uart_t             uart;
    struct nrc_port_uart_pars   pars;

    bool_t                      open;
    struct nrc_serial_reader    reader;
    struct nrc_serial_writer    writer;
    enum nrc_serial_state       tx_state;
    u32_t                       bytes_written;
};

static s32_t get_settings(const s8_t *cfg_id, u8_t *port, struct nrc_port_uart_pars *pars);
static struct nrc_serial* get_serial_from_cfg(const s8_t *cfg_id_settings);
static struct nrc_serial* get_serial_from_uart(nrc_port_uart_t uart);

static s32_t serial_open_reader_or_writer(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_reader    *reader,
    struct nrc_serial_writer    *writer,
    nrc_serial_t                *serial_id);

static void data_available(nrc_port_uart_t uart, s32_t result);
static void write_complete(nrc_port_uart_t uart, s32_t result, u32_t bytes);

static const s8_t                           *_tag = "serial";
static bool_t                               _initialized = FALSE;
static struct nrc_serial                    *_serial = NULL;        // List of created serial objects
static struct nrc_port_uart_callback_fcn    _callback;              // NRC UART callback functions

s32_t nrc_serial_init(void)
{
    s32_t result = NRC_R_OK;

    if (!_initialized) {
        _initialized = TRUE;
        _serial = NULL;

        memset(&_callback, 0, sizeof(struct nrc_port_uart_callback_fcn));

        _callback.data_available = data_available;
        _callback.write_complete = write_complete;

        result = nrc_port_uart_init();
    }

    return result;
}

static s32_t serial_open_reader_or_writer(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_reader    *reader,
    struct nrc_serial_writer    *writer,
    nrc_serial_t                *serial_id)
{
    s32_t                       result = NRC_R_OK;
    struct nrc_serial           *serial = NULL;

    if ((cfg_id_settings != NULL) && (serial_id != NULL) && 
        (((reader != NULL) && (reader->node != NULL) && (reader->data_available_evt != 0) && (reader->error_evt != 0)) ||
         ((writer != NULL) && (writer->node != NULL) && (writer->write_complete_evt != 0) && (writer->error_evt != 0)))) {

        *serial_id = NULL;

        // Get (if already created) serial object identified by
        // the id for the "nrc-serial" configuration node
        serial = get_serial_from_cfg(cfg_id_settings);

        if (serial != NULL) {
            // Already created

            if ((reader != NULL) && (serial->reader.node == NULL)) {
                // Serial available with no reader
                serial->reader = *reader;
            }
            else if ((writer != NULL) && (serial->writer.node == NULL)) {
                // Serial available with no writer
                serial->writer = *writer;
            }
            else {
                // Serial already opened by reader
                result = NRC_R_UNAVAILABLE_RESOURCE;
            }
        }
        else {
            // Allocate and initialize serial object
            serial = (struct nrc_serial*)nrc_port_heap_alloc(sizeof(struct nrc_serial));

            if (serial != NULL) {
                memset(serial, 0, sizeof(struct nrc_serial));

                serial->cfg_id_settings = cfg_id_settings;
                if (reader != NULL) {
                    serial->reader = *reader;
                }
                if (writer != NULL) {
                    serial->writer = *writer;
                }
                serial->type = NRC_SERIAL_TYPE;
                serial->open = FALSE; // Not yet opened

                // Insert first in list
                serial->next = _serial;
                _serial = serial;
            }
            else {
                result = NRC_R_OUT_OF_MEM;
            }
        }

        if ((result == NRC_R_OK) && (!serial->open)) {
            // Read serial port and settings from configuration
            result = get_settings(cfg_id_settings, &serial->port, &serial->pars);
        }

        if ((result == NRC_R_OK) && (!serial->open)) {
            // Open the UART if not already done
            result = nrc_port_uart_open(serial->port, serial->pars, _callback, &serial->uart);

            if (result == NRC_R_OK) {
                serial->open = TRUE;
                serial->tx_state = NRC_SERIAL_S_IDLE;
            }
            else {
                memset(&serial->reader, 0, sizeof(struct nrc_serial_reader));
                memset(&serial->writer, 0, sizeof(struct nrc_serial_writer));
            }
        }

        if (result == NRC_R_OK) {
            *serial_id = serial; 
        }
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

s32_t nrc_serial_open_reader(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_reader    reader,
    nrc_serial_t                *serial)
{
    s32_t result = NRC_R_OK;

    if ((cfg_id_settings != NULL) && (serial != NULL) &&
        (reader.node != NULL) && (reader.data_available_evt != 0) && (reader.error_evt != 0)) {

        result = serial_open_reader_or_writer(cfg_id_settings, &reader, NULL, serial);
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

s32_t nrc_serial_close_reader(nrc_serial_t serial)
{
    s32_t               result = NRC_R_OK;
    struct nrc_serial   *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE)) {
        if (self->open && (self->reader.node != NULL)) {
            memset(&self->reader, 0, sizeof(struct nrc_serial_reader));

            if (self->writer.node == NULL) {
                // Close UART only if both reader and writer are closed
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
    else {
        NRC_LOGE(_tag, "read: invalid in params");
    }

    return read_bytes;
}

u32_t nrc_serial_get_bytes(nrc_serial_t serial)
{
    u32_t               bytes = 0;
    struct nrc_serial   *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE)) {
        if (self->open && (self->uart != NULL)) {
            bytes = nrc_port_uart_get_bytes(self->uart);
        }
        else {
            NRC_LOGE(_tag, "get_bytes(%s): uart not open", self->cfg_id_settings);
        }
    }
    else {
        NRC_LOGE(_tag, "get_bytes: invalid in params");
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

s32_t nrc_serial_open_writer(
    const s8_t                  *cfg_id_settings,
    struct nrc_serial_writer    writer,
    nrc_serial_t                *serial)
{
    s32_t result = NRC_R_OK;

    if ((cfg_id_settings != NULL) && (serial != NULL) &&
        (writer.node != NULL) && (writer.write_complete_evt != 0) && (writer.error_evt != 0)) {

        result = serial_open_reader_or_writer(cfg_id_settings, NULL, &writer, serial);
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

s32_t nrc_serial_close_writer(nrc_serial_t serial)
{
    s32_t               result = NRC_R_OK;
    struct nrc_serial   *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE) && (self->writer.node != NULL)) {
        if (self->open) {
            memset(&self->writer, 0, sizeof(struct nrc_serial_writer));

            if (self->reader.node == NULL) {
                // Close UART only if both reader and writer are closed
                self->open = FALSE;
                result = nrc_port_uart_close(self->uart);
                self->uart = NULL;
            }
        }
        else {
            result = NRC_R_INVALID_STATE;
        }
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

s32_t nrc_serial_write(nrc_serial_t serial, u8_t *buf, u32_t buf_size)
{
    s32_t               result = NRC_R_INVALID_IN_PARAM;
    struct nrc_serial   *self = (struct nrc_serial*)serial;

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE) && (buf != NULL) && (buf_size > 0)) {
        if (self->open && (self->tx_state == NRC_SERIAL_S_IDLE)) {
            result = nrc_port_uart_write(self->uart, buf, buf_size);
            if (result == NRC_R_OK) {
                self->bytes_written = buf_size;
                self->tx_state = NRC_SERIAL_S_BUSY;
            }
        }
        else {
            result = NRC_R_INVALID_STATE;
        }
    }

    return result;
}

s32_t nrc_serial_get_write_error(nrc_serial_t serial)
{
    //TODO:
    return NRC_R_OK;
}

static struct nrc_serial* get_serial_from_cfg(const s8_t *cfg_id_settings)
{
    struct nrc_serial* serial = _serial;

    while ((serial != NULL) && (strcmp(serial->cfg_id_settings, cfg_id_settings) != 0)) { //(serial->cfg_id_settings != cfg_id_settings)) {
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
        result = nrc_cfg_get_str(cfg_id, "type", &cfg_type);

        if (result == NRC_R_OK) {
            if ((cfg_type == NULL) || (strcmp("nrc-serial", cfg_type) != 0)) {
                result = NRC_R_INVALID_CFG;
            }
        }
    }
    if (result == NRC_R_OK) {
        s32_t p;
        result = nrc_cfg_get_int(cfg_id, "port", &p);

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
        result = nrc_cfg_get_int(cfg_id, "baudrate", &pars->baud_rate);
    }
    if (result == NRC_R_OK) {
        s32_t db;
        result = nrc_cfg_get_int(cfg_id, "databits", &db);

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
        result = nrc_cfg_get_str(cfg_id, "parity", &cfg_parity);

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
        result = nrc_cfg_get_int(cfg_id, "stopbits", &sb);

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
        result = nrc_cfg_get_str(cfg_id, "flowctrl", &cfg_flow_ctrl);

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

// Callback function from thread in port layer
static void data_available(nrc_port_uart_t uart, s32_t result)
{
    struct nrc_serial   *serial;
    u32_t               evt = 0;
    nrc_node_t          node = NULL;
    s8_t                prio;

    result = nrc_os_lock();
    NRC_ASSERT(result == NRC_R_OK);

    serial = get_serial_from_uart(uart);

    if ((serial != NULL) && (serial->type == NRC_SERIAL_TYPE) &&
        serial->open && (serial->reader.node != NULL)) {

        evt = serial->reader.data_available_evt;

        if (result != NRC_R_OK) {
            evt |= serial->reader.error_evt;
        }
        
        node = serial->reader.node;
        prio = serial->reader.prio;
    }
    else {
        NRC_LOGD(_tag, "data_available: invalid uart or no reader");
    }

    if (evt != 0) {
        nrc_os_send_evt(node, evt, prio);
    }

    result = nrc_os_unlock();
    NRC_ASSERT(result == NRC_R_OK);
}

// Callback from thread in port layer
static void write_complete(nrc_port_uart_t uart, s32_t result, u32_t bytes)
{
    struct nrc_serial   *self;
    u32_t               evt = 0;
    nrc_node_t          node = NULL;
    s8_t                prio;

    result = nrc_os_lock();
    NRC_ASSERT(result == NRC_R_OK);

    self = get_serial_from_uart(uart);

    if ((self != NULL) && (self->type == NRC_SERIAL_TYPE) && self->open &&
        (self->writer.node != NULL) && (self->tx_state == NRC_SERIAL_S_BUSY)) {

        evt = self->writer.write_complete_evt;

        if (result != NRC_R_OK) {
            evt |= self->writer.error_evt;
        }

        node = self->writer.node;
        prio = self->writer.prio;

        if (self->bytes_written != bytes) {
            NRC_LOGD(_tag, "write_complete: Not correct number of bytes written (%d != %d)", self->bytes_written, bytes);
        }

        self->bytes_written = 0;
        self->tx_state = NRC_SERIAL_S_IDLE;
    }
    else {
        NRC_LOGD(_tag, "write_complete: invalid uart, state or no writer");
    }

    if (evt != 0) {
        nrc_os_send_evt(node, evt, prio);
    }

    result = nrc_os_unlock();
    NRC_ASSERT(result == NRC_R_OK);
}