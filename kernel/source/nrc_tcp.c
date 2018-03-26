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

#include "nrc_tcp.h"
#include "nrc_port.h"
#include "nrc_port_socket.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_assert.h"
#include <string.h>

#define NRC_TCP_TYPE (0x1438B4BB)

enum nrc_tcp_state {
    CLOSED = 0,
    LISTENING,
    CONNECTING,
    OPEN,
    IDLE,
    BUSY
};

enum nrc_tcp_role {
    CLIENT = 0,
    SERVER,
};

struct nrc_tcp {
    struct nrc_tcp          *next;
    const s8_t              *cfg_id_settings;
    struct {        
        enum nrc_tcp_role   role;
        u16_t               port;
        s8_t                *host;
    } settings;

    nrc_port_mutex_t            mutex;
    bool_t                      open;
    enum nrc_tcp_state          state;
    nrc_port_socket_t           listening_socket;   
    nrc_port_socket_t           socket;

    struct nrc_tcp_reader       reader;
    struct nrc_tcp_writer       writer;
    enum nrc_tcp_state          tx_state;
    u32_t                       type;
};

static s32_t init_settings(struct nrc_tcp* tcp, const s8_t *cfg_id);
static bool_t equal_node_config(struct nrc_tcp *tcp, const s8_t *cfg_id);

static struct nrc_tcp* get_tcp_from_cfg(const s8_t *cfg_id_settings);
static struct nrc_tcp* get_tcp_from_socket(nrc_port_socket_t socket);

static s32_t tcp_open_reader_or_writer(
    const s8_t               *cfg_id_settings,
    struct nrc_tcp_reader    *reader,
    struct nrc_tcp_writer    *writer,
    nrc_tcp_t                *tcp_id);

static void remote_connect_evt(nrc_port_socket_t socket, void *context);
static void socket_data_available(nrc_port_socket_t socket, s32_t result);
static void socket_write_complete(nrc_port_socket_t socket, s32_t result);
static void socket_connect_event(nrc_port_socket_t socket, s32_t result);
static void socket_disconnect_event(nrc_port_socket_t socket, s32_t result);
static void socket_error_event(nrc_port_socket_t socket, s32_t result);

static const s8_t                           *_tag = "tcp";
static bool_t                               _initialized = FALSE;
static struct nrc_tcp                       *_tcp = NULL;        // List of created tcp objects

static struct nrc_port_socket_callback_fcn  _callback;

s32_t nrc_tcp_init(void)
{
    s32_t result = NRC_R_OK;

    if (_initialized != TRUE) {
        _initialized = TRUE;
        _tcp = NULL;

        memset(&_callback, 0, sizeof(struct nrc_port_socket_callback_fcn));

        _callback.data_available = socket_data_available;
        _callback.write_complete = socket_write_complete;
        _callback.disconnect_event = socket_disconnect_event;
        _callback.connect_event = socket_connect_event;
        _callback.error_event = socket_error_event;

        result = nrc_port_socket_init();
    }

    return result;
}

static s32_t tcp_open_reader_or_writer(
    const s8_t               *cfg_id_settings,
    struct nrc_tcp_reader    *reader,
    struct nrc_tcp_writer    *writer,
    nrc_tcp_t                *tcp_id)
{
    s32_t          result = NRC_R_OK;
    struct nrc_tcp *tcp = NULL;

    if ((cfg_id_settings != NULL) && (tcp_id != NULL) &&
        (((reader != NULL) && (reader->node != NULL) && (reader->data_available_evt != 0) && (reader->error_evt != 0)) ||
        ((writer != NULL) && (writer->node != NULL) && (writer->write_complete_evt != 0) && (writer->error_evt != 0)))) {

        *tcp_id = NULL;

        // Get (if already created) tcp object identified by
        // the id for the "nrc-tcp" configuration node
        tcp = get_tcp_from_cfg(cfg_id_settings);

        if (tcp != NULL) {
            if ((reader != NULL) && (tcp->reader.node == NULL)) {
                // Serial available with no reader
                tcp->reader = *reader;
            }
            else if ((writer != NULL) && (tcp->writer.node == NULL)) {
                // Serial available with no writer
                tcp->writer = *writer;
            }
            else {
                // Serial already opened by reader
                result = NRC_R_UNAVAILABLE_RESOURCE;
            }
        }
        else {
            // Allocate and initialize tcp object
            tcp = (struct nrc_tcp*)nrc_port_heap_alloc(sizeof(struct nrc_tcp));

            if (tcp != NULL) {
                memset(tcp, 0, sizeof(struct nrc_tcp));

                tcp->cfg_id_settings = cfg_id_settings;
                if (reader != NULL) {
                    tcp->reader = *reader;
                }
                if (writer != NULL) {
                    tcp->writer = *writer;
                }
                tcp->type = NRC_TCP_TYPE;
                tcp->state = CLOSED;
                tcp->tx_state = IDLE;
                tcp->open = FALSE;

                // Insert first in list
                tcp->next = _tcp;
                _tcp = tcp;
            } 
            else {
                result = NRC_R_OUT_OF_MEM;
            }
        }

        if ((result == NRC_R_OK) && (tcp->state == CLOSED)) {
            // Read tcp port and settings from configuration
            result = init_settings(tcp, cfg_id_settings);
        }

        if ((result == NRC_R_OK) && (tcp->state == CLOSED)) {
            if (tcp->settings.role == SERVER) {

                result = nrc_port_socket_create(NRC_PORT_SOCKET_TCP, tcp, &tcp->listening_socket);
                NRC_ASSERT(OK(result));
                NRC_ASSERT(tcp->listening_socket);

                result = nrc_port_socket_register(tcp->listening_socket, &_callback);
                NRC_ASSERT(OK(result));

                result = nrc_port_socket_bind(tcp->listening_socket, tcp->settings.port);
                NRC_ASSERT(OK(result));

                result = nrc_port_socket_listen(tcp->listening_socket, remote_connect_evt);
                NRC_ASSERT(OK(result));

                tcp->state = LISTENING;
            }
            else if (tcp->settings.role == CLIENT) {

                result = nrc_port_socket_create(NRC_PORT_SOCKET_TCP, tcp, &tcp->socket);
                NRC_ASSERT(OK(result));
                NRC_ASSERT(tcp->socket);

                result = nrc_port_socket_register(tcp->listening_socket, &_callback);
                NRC_ASSERT(OK(result));

                //result = nrc_port_socket_bind(tcp->listening_socket, tcp->settings.port);
                //NRC_ASSERT(OK(result));

                result = nrc_port_socket_connect(tcp->listening_socket, tcp->settings.host, tcp->settings.port);
                NRC_ASSERT(OK(result));

                tcp->state = CONNECTING;
            }
        }
    }

    if (result == NRC_R_OK) {
        *tcp_id = tcp;
    }

    return result;
}

s32_t nrc_tcp_open_reader(
    const s8_t              *cfg_id_settings,
    struct nrc_tcp_reader    reader,
    nrc_tcp_t                *tcp)
{
    s32_t result = NRC_R_OK;

    if ((cfg_id_settings != NULL) && (tcp != NULL) &&
        (reader.node != NULL) && (reader.data_available_evt != 0) && (reader.error_evt != 0)) {
        result = tcp_open_reader_or_writer(cfg_id_settings, &reader, NULL, tcp);
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

s32_t nrc_tcp_close_reader(nrc_tcp_t tcp)
{
    s32_t               result = NRC_R_OK;
    struct nrc_tcp      *self = (struct nrc_tcp*)tcp;

    if ((self != NULL) && (self->type == NRC_TCP_TYPE)) {
        if ((self->open == TRUE) && (self->reader.node != NULL)) {
            memset(&self->reader, 0, sizeof(struct nrc_tcp_reader));

            if (self->writer.node == NULL) {
                // Close socket only if both reader and writer are closed
                self->open = FALSE;
                result = nrc_port_socket_close(self->socket);
                self->socket = NULL;
            }
        }
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

u32_t nrc_tcp_read(nrc_tcp_t tcp, u8_t *buf, u32_t buf_size)
{
    u32_t          read_bytes = 0;
    struct nrc_tcp *self = (struct nrc_tcp*)tcp;

    if ((self != NULL) && (self->type == NRC_TCP_TYPE) &&
        (buf != NULL) && (buf_size > 0)) {

        read_bytes = nrc_port_socket_read(self->socket, buf, buf_size);
    }
    else {
        NRC_LOGE(_tag, "read: invalid in params");
    }

    return read_bytes;
}

u32_t nrc_tcp_get_bytes(nrc_tcp_t tcp)
{
    u32_t          bytes = 0;
    struct nrc_tcp *self = (struct nrc_tcp*)tcp;

    if ((self != NULL) && (self->type == NRC_TCP_TYPE)) {
        if ((self->open) && (self->socket != NULL)) {
            bytes = nrc_port_socket_get_bytes(self->socket);
        }
        else {
            NRC_LOGW(_tag, "get_bytes(%s): socket not open", self->cfg_id_settings);
        }
    }
    else {
        NRC_LOGE(_tag, "get_bytes: invalid in params");
    }

    return bytes;
}

s32_t nrc_tcp_open_writer(
    const s8_t            *cfg_id_settings,
    struct nrc_tcp_writer writer,
    nrc_tcp_t             *tcp)
{
    s32_t result = NRC_R_OK;

    if ((cfg_id_settings != NULL) && (tcp != NULL) &&
        (writer.node != NULL) && (writer.write_complete_evt != 0) && (writer.error_evt != 0)) {
        result = tcp_open_reader_or_writer(cfg_id_settings, NULL, &writer, tcp);
    }
    else {
        result = NRC_R_INVALID_IN_PARAM;
    }

    return result;
}

s32_t nrc_tcp_close_writer(nrc_tcp_t tcp)
{
    s32_t               result = NRC_R_OK;
    struct nrc_tcp      *self = (struct nrc_tcp*)tcp;

    if ((self != NULL) && (self->type == NRC_TCP_TYPE) && (self->writer.node != NULL)) {
        if (self->open == TRUE) {
            memset(&self->writer, 0, sizeof(struct nrc_tcp_writer));

            if (self->reader.node == NULL) {
                // Close socket only if both reader and writer are closed
                self->open = FALSE;
                result = nrc_port_socket_close(self->socket);
                self->socket = NULL;
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

s32_t nrc_tcp_write(nrc_tcp_t tcp, u8_t *buf, u32_t buf_size)
{
    s32_t               result = NRC_R_INVALID_IN_PARAM;
    struct nrc_tcp     *self = (struct nrc_tcp*)tcp;

    if ((self != NULL) && (self->type == NRC_TCP_TYPE) && (buf != NULL) && (buf_size > 0)) {
        if ((self->open) && (self->tx_state == IDLE)) {
            result = nrc_port_socket_write(self->socket, buf, buf_size);
            if (result == NRC_R_OK) {
                self->tx_state = BUSY;
            }
        }
        else {
            result = NRC_R_INVALID_STATE;
        }
    }

    return result;
}

s32_t nrc_tcp_clear(nrc_tcp_t tcp)
{
    s32_t          result = NRC_R_INVALID_IN_PARAM;
    struct nrc_tcp *self = (struct nrc_tcp*)tcp;

    if ((self != NULL) && (self->type == NRC_TCP_TYPE)) {
        //TODO
    }

    return result;
}

static struct nrc_tcp* get_tcp_from_cfg(const s8_t *cfg_id_settings)
{
    struct nrc_tcp* tcp = _tcp;
    
    while ((tcp != NULL) && !equal_node_config(tcp, cfg_id_settings)) {
        tcp = tcp->next;
    }

    return tcp;
}

static struct nrc_tcp* get_tcp_from_socket(nrc_port_socket_t socket)
{
    struct nrc_tcp* tcp = _tcp;

    while ((tcp != NULL) && (tcp->socket != socket)) {
        tcp = tcp->next;
    }

    return tcp;
}

static s32_t init_settings(struct nrc_tcp* tcp, const s8_t *cfg_id)
{
    s32_t       result = NRC_R_OK;
    const s8_t  *cfg_type = NULL;

    if ((cfg_id == NULL) || (tcp == NULL)) {
        result = NRC_R_INVALID_IN_PARAM;
    }

    if (result == NRC_R_OK) {
        result = nrc_cfg_get_str(cfg_id, "type", &cfg_type);

        if (result == NRC_R_OK) {
            if ((cfg_type == NULL) || 
                !((strcmp("nrc-tcp-in", cfg_type) == 0) || (strcmp("nrc-tcp-out", cfg_type) == 0))) {
                result = NRC_R_INVALID_CFG;
            }
        }
    }

    if (result == NRC_R_OK) {
        s32_t p;
        result = nrc_cfg_get_int(cfg_id, "port", &p);

        if (result == NRC_R_OK) {
            if ((p >= 0) && (p <= 0xFFFF)) {
                tcp->settings.port = p;
            }
            else {
                result = NRC_R_INVALID_CFG;
            }
        }
    }

    if (OK(result)) {
        const s8_t *role;
        result = nrc_cfg_get_str(cfg_id, "role", &role);
        if (OK(result)) {
            if (strcmp(role, "server") == 0) {
                tcp->settings.role = SERVER;
            }
            else if (strcmp(role, "client") == 0) {
                tcp->settings.role = CLIENT;
            }
            else {
                result = NRC_R_ERROR;
            }
        }
    }

    if (result == NRC_R_OK) {
        result = nrc_cfg_get_str(cfg_id, "host", &tcp->settings.host);
    }

    return result;
}

static bool_t equal_node_config(struct nrc_tcp *tcp, const s8_t *cfg_id)
{
    bool_t ok = TRUE;
    s8_t result;

    if ((cfg_id == NULL) || (tcp == NULL)) {
        ok = FALSE;
    }

    if (ok) {
        s32_t port;
        result = nrc_cfg_get_int(cfg_id, "port", &port);
        if (!OK(result) || (port != tcp->settings.port)) {
            ok = FALSE;
        }
    }

    if (ok) {
        const s8_t *role;
        result = nrc_cfg_get_str(cfg_id, "role", &role);
        if (OK(result)) {
            if (strcmp(role, "server") == 0) {
                if (tcp->settings.role != SERVER) {
                    ok = FALSE;
                }
            }
            else if (strcmp(role, "client") == 0) {
                if (tcp->settings.role != CLIENT) {
                    ok = FALSE;
                }
            }
            else {
                ok = FALSE;
            }
        }
    }

    if (ok) {
        const s8_t *host;
        result = nrc_cfg_get_str(cfg_id, "host", &host);
        if (!OK(result) || 
            ((host && tcp->settings.host) && (strcmp(tcp->settings.host, host) != 0))) {
            ok = FALSE;
        }
    }

    return ok;
}

static void remote_connect_evt(nrc_port_socket_t socket, void *context)
{
    NRC_LOGD(_tag, "Socket remote connect event");
    struct nrc_tcp* tcp = (struct nrc_tcp*)context;
    tcp->socket = socket;
    tcp->open = TRUE; 
}

// Callback function from thread in port layer
static void socket_data_available(nrc_port_socket_t socket, s32_t result)
{
    struct nrc_tcp      *tcp;

    u32_t               evt = 0;
    nrc_node_t          node = NULL;
    s8_t                prio;

    result = nrc_os_lock();
    NRC_ASSERT(result == NRC_R_OK);

    tcp = get_tcp_from_socket(socket);

    if ((tcp != NULL) && (tcp->type == NRC_TCP_TYPE) &&
        (tcp->open == TRUE) && (tcp->reader.node != NULL)) {

        evt = tcp->reader.data_available_evt;

        if (result != NRC_R_OK) {
            evt |= tcp->reader.error_evt;
        }
        
        node = tcp->reader.node;
        prio = tcp->reader.prio;
    }
    else {
        NRC_LOGD(_tag, "data_available: invalid socket or no reader");
    }

    if (evt != 0) {
        nrc_os_send_evt(node, evt, prio);
    }

    result = nrc_os_unlock();
    NRC_ASSERT(result == NRC_R_OK);
}

// Callback from thread in port layer
static void socket_write_complete(nrc_port_socket_t socket, s32_t result)
{
    struct nrc_tcp      *self;
    u32_t               evt = 0;
    nrc_node_t          node = NULL;
    s8_t                prio;

    result = nrc_os_lock();
    NRC_ASSERT(result == NRC_R_OK);

    self = get_tcp_from_socket(socket);

    if ((self != NULL) && (self->type == NRC_TCP_TYPE) && (self->open == TRUE) &&
        (self->writer.node != NULL) && (self->tx_state == BUSY)) {

        evt = self->writer.write_complete_evt;

        if (result != NRC_R_OK) {
            evt |= self->writer.error_evt;
        }

        node = self->writer.node;
        prio = self->writer.prio;

        self->tx_state = IDLE;
    }
    else {
        NRC_LOGD(_tag, "write_complete: invalid socket, state or no writer");
    }

    if (evt != 0) {
        nrc_os_send_evt(node, evt, prio);
    }
	
	result = nrc_os_unlock();
    NRC_ASSERT(result == NRC_R_OK);
}

static void socket_connect_event(nrc_port_socket_t socket, s32_t result)
{
    //TODO
    NRC_LOGD(_tag, "socket_connect_event");
}

static void socket_disconnect_event(nrc_port_socket_t socket, s32_t result)
{
    //TODO
    NRC_LOGD(_tag, "socket_disconnect_event");
}
static void socket_error_event(nrc_port_socket_t socket, s32_t result)
{
    //TODO
    NRC_LOGD(_tag, "socket_error_event");
}
