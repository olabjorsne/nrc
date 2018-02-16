#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")

#include "nrc_port_socket.h"
#include "nrc_misc_cbuf.h"
#include "nrc_assert.h"


#define PORT_NBR_MAX_STR_LEN      (10)  
#define NRC_PORT_SOCKET_BUF_SIZE  (1024)
#define NRC_PORT_SOCKET_READ_SIZE (1024)

#define LOCK(self)      NRC_ASSERT(OK(nrc_port_mutex_lock(self->mutex, 0)));
#define UNLOCK(self)    NRC_ASSERT(OK(nrc_port_mutex_unlock(self->mutex)));

enum nrc_port_socket_state {
    STATE_CLOSED = 0,
    STATE_LISTENING,
    STATE_CONNECTING,
    STATE_CONNECTED,    
    STATE_DISCONNECTING,
    STATE_ERROR,

    STATE_IDLE,
    STATE_WRITING,
    STATE_READING
};

struct nrc_port_socket {
    enum nrc_socket_protocol                protocol;
    u16_t                                   port;
    enum nrc_port_socket_state              state;
    void*                                   context;

    nrc_port_socket_remote_connect_evt_t    remote_connect_callback;
    struct nrc_port_socket_callback_fcn     *event_callback;
    const s8_t                              *address;
    nrc_port_thread_t                       thread;
    nrc_port_mutex_t                        mutex;
    
    SOCKET                                  win_socket;
    struct addrinfo*                        addr_result;
    struct addrinfo                         addr_hints;

    struct rx {
        enum nrc_port_socket_state          state;
        u8_t                                buf[NRC_PORT_SOCKET_BUF_SIZE];
        nrc_misc_cbuf_t                     cbuf;
        bool_t                              notify_data_available;
        WSAOVERLAPPED                       overlapped;
        WSABUF                              wsa_buf;
    } rx;

    struct tx {
        enum nrc_port_socket_state          state;
        WSAOVERLAPPED                       overlapped;
        WSABUF                              wsa_buf;
    } tx;
};

static struct nrc_port_socket *nrc_port_socket_alloc(enum nrc_socket_protocol protocol, SOCKET win_socket, void *context);
static s32_t start_receive(struct nrc_port_socket *socket);

static void socket_thread(void* context);
// Wrapper functions for win3 api
static SOCKET create_socket_win32(enum nrc_socket_protocol protocol);
static SOCKET listen_socket_win32(SOCKET listener);
static s32_t bind_socket_win32(struct nrc_port_socket* socket, u16_t port);
static s32_t receive_win32(SOCKET socket, WSAOVERLAPPED *RecvOverlapped, WSABUF *data_buf);
static s32_t send_win32(SOCKET socket, WSAOVERLAPPED *overlapped, WSABUF *data_buf);
static void CALLBACK tx_complete(IN DWORD dwError, IN DWORD cbTransferred, IN LPWSAOVERLAPPED lpOverlapped, IN DWORD dwFlags);
static void CALLBACK rx_complete(IN DWORD dwError, IN DWORD cbTransferred, IN LPWSAOVERLAPPED lpOverlapped, IN DWORD dwFlags);

static bool_t _initialized = FALSE;
static const s8_t *_tag = "nrc_port_socket";

s32_t nrc_port_socket_init(void)
{
    s32_t result = NRC_R_OK;
    if (_initialized == FALSE) {
        WSADATA wsaData;
        int wsa_startup_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (wsa_startup_result == 0) {
            _initialized = TRUE;
        }
        else {
            NRC_LOGE(_tag, "WSAStartup failed with error: %d\n", wsa_startup_result)
        }
    }
    return result;
}

s32_t nrc_port_socket_create(enum nrc_socket_protocol protocol, void* context, nrc_port_socket_t *socket)
{
    s32_t result = NRC_R_OK;
    struct nrc_port_socket *self = NULL;
    SOCKET win_socket = INVALID_SOCKET;

    NRC_ASSERT(socket != NULL);
    NRC_ASSERT(_initialized == TRUE);
    *socket = NULL;

    win_socket = create_socket_win32(protocol);
    if (win_socket == INVALID_SOCKET) {
        result = NRC_R_ERROR;
    }
    
    if (OK(result)) {
        self = nrc_port_socket_alloc(protocol, win_socket, context);
        if (self != NULL) {
            *socket = self;
        }
        else {
            result = NRC_R_OUT_OF_MEM;
            closesocket(win_socket);
        }       
    }

    return result;
}

s32_t nrc_port_socket_register(nrc_port_socket_t socket, struct nrc_port_socket_callback_fcn *callback)
{
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket; 
    NRC_ASSERT(self);
    self->event_callback = callback;
    return NRC_R_OK;
}

s32_t nrc_port_socket_close(nrc_port_socket_t socket)
{
    s32_t result = NRC_R_OK;
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket;
    NRC_ASSERT(self);

    LOCK(self);
    {
        switch (self->state) {
        case STATE_CONNECTED:
        {
            s32_t res = shutdown(self->win_socket, SD_SEND);
            if (OK(res)) {
                result = NRC_R_OK;
                self->state = STATE_CLOSED;
            }
            else {
                NRC_LOGE(_tag, "Failed to shutdown socket");
                self->state = STATE_ERROR;
            }
        }
        break;
        default:
            result = NRC_R_ERROR;
            break;
        }
    }
    UNLOCK(self);

    return result;
}

s32_t nrc_port_socket_bind(nrc_port_socket_t socket, u16_t port)
{
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket;
    s32_t result = NRC_R_ERROR;
    NRC_ASSERT(self);

    LOCK(self);
    {
        switch (self->state) {
        case STATE_CLOSED:
            result = bind_socket_win32(self, port);
            break;
        default:
            result = NRC_R_ERROR;
            break;
        }
    }
    UNLOCK(self);

    return result;
}

static void listening(struct nrc_port_socket* self)
{
    s32_t result;

    NRC_ASSERT(self->remote_connect_callback);
    
    SOCKET incoming = listen_socket_win32(self->win_socket);
    if (incoming != INVALID_SOCKET) {
        struct nrc_port_socket *incoming_nrc_socket = (struct nrc_port_socket *)nrc_port_socket_alloc(self->protocol, incoming, self->context);
        NRC_ASSERT(incoming_nrc_socket);
        
        incoming_nrc_socket->state = STATE_CONNECTED;
        incoming_nrc_socket->event_callback = self->event_callback;
        self->remote_connect_callback(incoming_nrc_socket, self->context);
        
        LOCK(self);
        {
            result = start_receive(incoming_nrc_socket);
        }
        UNLOCK(self);

        if (!OK(result)) {
            NRC_LOGE(_tag, "Failed to initialize receive");
            if (incoming_nrc_socket->event_callback) {
                self->event_callback->error_event(self, NRC_R_ERROR);
            }
            incoming_nrc_socket->state = STATE_ERROR;
        }
    }
}

static connecting(struct nrc_port_socket* self)
{
    s32_t result;
    s8_t port_str[PORT_NBR_MAX_STR_LEN];
    memset(port_str, 0, PORT_NBR_MAX_STR_LEN);
    nrc_port_socket_event connect_event = NULL;
    nrc_port_socket_event disconnect_event = NULL;
    nrc_port_socket_event error_event = NULL;

    LOCK(self);
    {
        _itoa_s(self->port, port_str, PORT_NBR_MAX_STR_LEN, 10);
        s32_t res = getaddrinfo(self->address, port_str, &self->addr_hints, &self->addr_result);
        if (OK(res)) {
            res = connect(self->win_socket, self->addr_result->ai_addr, (int)self->addr_result->ai_addrlen);
            if (OK(res)) {
                self->state = STATE_CONNECTED;
                connect_event = self->event_callback->connect_event;
            }
        }

        if (!OK(res)) {
            self->state = STATE_CLOSED;
            if (self->event_callback) {
                disconnect_event = self->event_callback->disconnect_event;
            }
        }

        if (self->state == STATE_CONNECTED) {
            result = start_receive(self);
            if (!OK(result)) {
                if (self->event_callback) {
                    error_event = self->event_callback->error_event;
                }
                self->state = STATE_ERROR;
            }
        }
    }
    UNLOCK(self);

    if (connect_event) {
        connect_event(self, NRC_R_OK);
    }

    if (disconnect_event) {
        disconnect_event(self, NRC_R_ERROR);
    }
    if (error_event) {
        error_event(self, NRC_R_ERROR);
    }
}

void socket_thread(void* context)
{
    s32_t result = NRC_R_OK;
    bool_t run = TRUE;
    struct nrc_port_socket *self = (struct nrc_port_socket*)context;
    NRC_ASSERT(self);

    while (run) {
        switch (self->state) {
        case STATE_LISTENING:
            listening(self);
            break;
        case STATE_CONNECTING:
            connecting(self);
            break;
        case STATE_CONNECTED:
        case STATE_CLOSED:
        case STATE_ERROR:
        default:
            // Socket thread is only active when listening, connecting or disconnecting
            run = FALSE;
            break;
        }
    }
    NRC_LOGD(_tag, "Socket thread stopped");
}

s32_t nrc_port_socket_listen(nrc_port_socket_t socket, nrc_port_socket_remote_connect_evt_t callback)
{
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket;
    s32_t result = NRC_R_ERROR;
    NRC_ASSERT(self);
    NRC_ASSERT(callback);

    switch (self->state) {
    case STATE_CLOSED:
        result = nrc_port_thread_init(NRC_PORT_THREAD_PRIO_NORMAL, 4096, socket_thread, self, &self->thread);
        if (OK(result)) {
            self->state = STATE_LISTENING;
            self->remote_connect_callback = callback;
            result = nrc_port_thread_start(self->thread);
            NRC_ASSERT(OK(result));
        }
        break;
    default:
        result = NRC_R_ERROR;
        break;
    }
    return result;
}

s32_t nrc_port_socket_connect(nrc_port_socket_t socket, const s8_t *address, u16_t port)
{
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket;
    s32_t result = NRC_R_ERROR;
    NRC_ASSERT(self);
    self->port = port;
    LOCK(self);
    {
        switch (self->state) {
        case STATE_CLOSED:
            self->address = address;
            result = nrc_port_thread_init(NRC_PORT_THREAD_PRIO_NORMAL, 4096, socket_thread, self, &self->thread);
            if (OK(result)) {
                self->state = STATE_CONNECTING;
                result = nrc_port_thread_start(self->thread);
                NRC_ASSERT(OK(result));
            }
            break;
        default:
            result = NRC_R_ERROR;
            break;
        }
    }
    UNLOCK(self);
    return result;
}

u32_t nrc_port_socket_write(nrc_port_socket_t socket, u8_t *buf, u32_t buf_size)
{
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket;
    s32_t result = NRC_R_ERROR;
    NRC_ASSERT(self);

    LOCK(self);
    {
        switch (self->state) {
        case STATE_CONNECTED:
            if (self->tx.state == STATE_IDLE) {
                self->tx.wsa_buf.buf = buf;
                self->tx.wsa_buf.len = buf_size;
                self->tx.state = STATE_WRITING;
                result = send_win32(self->win_socket, &self->tx.overlapped, &self->tx.wsa_buf);
                if (!OK(result)) {
                    self->tx.state = STATE_IDLE;
                }
            }
            break;
        default:
            result = NRC_R_ERROR;
            break;
        }
    }
    UNLOCK(self);

    return result;
}

u32_t nrc_port_socket_get_bytes(nrc_port_socket_t socket)
{
    u32_t size;
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket;
    NRC_ASSERT(self);

    LOCK(self);
    {
        size = nrc_misc_cbuf_get_bytes(self->rx.cbuf);
        if (size == 0) {
            self->rx.notify_data_available = TRUE;
        }
    }
    UNLOCK(self);

    return size;
}

u32_t nrc_port_socket_read(nrc_port_socket_t socket, u8_t *buf, u32_t buf_size)
{
    s32_t result = NRC_R_ERROR;
    u32_t read_size;
    struct nrc_port_socket *self = (struct nrc_port_socket*)socket;
    NRC_ASSERT(self);

    LOCK(self);
    {
        read_size = nrc_misc_cbuf_read(self->rx.cbuf, buf, buf_size);
        if (nrc_misc_cbuf_get_bytes(self->rx.cbuf) == 0) {
            self->rx.notify_data_available = TRUE;
        }
        start_receive(self);
    } 
    UNLOCK(self);

    return read_size;
}

struct nrc_port_socket *nrc_port_socket_alloc(enum nrc_socket_protocol protocol, SOCKET win_socket, void *context)
{
    struct nrc_port_socket *self;
    s32_t result = NRC_R_ERROR;
    self = (struct nrc_port_socket*)nrc_port_heap_alloc(sizeof(struct nrc_port_socket));
    if (self != NULL) {
        memset(self, 0, sizeof(struct nrc_port_socket));
        self->win_socket = win_socket;
        self->context = context;
        self->protocol = protocol;
        self->state = STATE_CLOSED;
        self->rx.state = STATE_IDLE;
        self->tx.state = STATE_IDLE;

        result = nrc_port_mutex_init(&self->mutex);
        NRC_ASSERT(OK(result));

        result = nrc_misc_cbuf_init(self->rx.buf, NRC_PORT_SOCKET_BUF_SIZE, &self->rx.cbuf);
        NRC_ASSERT(OK(result));

        SecureZeroMemory((PVOID)&self->rx.overlapped, sizeof(WSAOVERLAPPED));
        self->rx.overlapped.hEvent = WSACreateEvent();
        NRC_ASSERT(self->rx.overlapped.hEvent);
        self->rx.overlapped.Pointer = (PVOID)self;        
        self->rx.notify_data_available = TRUE;

        SecureZeroMemory((PVOID)&self->tx.overlapped, sizeof(WSAOVERLAPPED));
        self->tx.overlapped.hEvent = WSACreateEvent();
        NRC_ASSERT(self->tx.overlapped.hEvent);
        self->tx.overlapped.Pointer = (PVOID)self;
 
    }
    return self;
}

s32_t start_receive(struct nrc_port_socket *socket)
{
    s32_t result = NRC_R_OK;
 
    if (socket->state == STATE_CONNECTED && socket->rx.state == STATE_IDLE) {
        socket->rx.wsa_buf.buf = nrc_misc_cbuf_get_write_buf(socket->rx.cbuf, &socket->rx.wsa_buf.len);
        if (socket->rx.wsa_buf.len > 0) {
            socket->rx.state = STATE_READING;
            result = receive_win32(socket->win_socket, &socket->rx.overlapped, &socket->rx.wsa_buf);
            if (!OK(result)) {
                NRC_LOGE(_tag, "Failed to initiate receive\n");
                socket->rx.state = STATE_IDLE;
            }
        }
    }
 
    return result;
}

static SOCKET create_socket_win32(enum nrc_socket_protocol protocol)
{
    SOCKET s = INVALID_SOCKET;

    switch (protocol) {
    case NRC_PORT_SOCKET_TCP:
        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        break;
    case NRC_PORT_SOCKET_UDP:
        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        break;
    default:
        s = INVALID_SOCKET;
        break;
    }
    return s;
}

static s32_t bind_socket_win32(struct nrc_port_socket* socket, u16_t port)
{
    s32_t result = NRC_R_OK;
    s8_t port_str[PORT_NBR_MAX_STR_LEN];
    memset(port_str, 0, PORT_NBR_MAX_STR_LEN);

    ZeroMemory(&socket->addr_hints, sizeof(socket->addr_hints));
    socket->addr_hints.ai_family = AF_INET;
    if (socket->protocol == NRC_PORT_SOCKET_TCP) {
        socket->addr_hints.ai_socktype = SOCK_STREAM;
        socket->addr_hints.ai_protocol = IPPROTO_TCP;
    }
    else {
        socket->addr_hints.ai_socktype = SOCK_DGRAM;
        socket->addr_hints.ai_protocol = IPPROTO_UDP;
    }

    socket->addr_hints.ai_flags = AI_PASSIVE;

    _itoa_s(port, port_str, PORT_NBR_MAX_STR_LEN, 10);
    result = getaddrinfo(NULL, port_str, &socket->addr_hints, &socket->addr_result);

    if (OK(result)) {
        result = bind(socket->win_socket, socket->addr_result->ai_addr, (int)socket->addr_result->ai_addrlen);
    }

    return result;
}

static SOCKET listen_socket_win32(SOCKET listener)
{
    SOCKET incoming = INVALID_SOCKET;
    s32_t result = listen(listener, SOMAXCONN);
    if (result != SOCKET_ERROR) {
        incoming = accept(listener, NULL, NULL);
    }
    return incoming;
}

static s32_t receive_win32(SOCKET socket, WSAOVERLAPPED *overlapped, WSABUF *data_buf)
{
    s32_t result;
    DWORD flags = 0;
    DWORD receive_bytes = 0;

    result = WSARecv(socket, data_buf, 1, &receive_bytes, &flags, overlapped, rx_complete);
    if ((result == SOCKET_ERROR)) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            NRC_LOGE(_tag, "WSARecv returned error code %d", err);
        }
        else {
            result = NRC_R_OK;
        }
    }
    else {
        result = NRC_R_OK;
    }
    return result;
}

static void CALLBACK tx_complete(
    IN DWORD dwError,
    IN DWORD transferred,
    IN LPWSAOVERLAPPED overlapped,
    IN DWORD dwFlags)
{
    struct nrc_port_socket *self = (struct nrc_port_socket *)overlapped->Pointer;
    nrc_port_socket_event write_complete = NULL;
    nrc_port_socket_event disconnect_event = NULL;

    NRC_ASSERT(self);
    NRC_ASSERT(self->tx.state == STATE_WRITING);

    LOCK(self)
    {
        if (transferred > 0) {
            if (transferred == self->tx.wsa_buf.len) {
                self->tx.state = STATE_IDLE;
                if (self->event_callback) {
                    write_complete = self->event_callback->write_complete;
                }
            }
            else {
                // Only part of buffer has been transmitted
                // Continue sending rest of buffer
                s32_t result = NRC_R_ERROR;
                self->tx.wsa_buf.buf += transferred;
                self->tx.wsa_buf.len -= transferred;
                result = send_win32(self->win_socket, &self->tx.overlapped, &self->tx.wsa_buf);
                NRC_ASSERT(OK(result));
            }
        }
        else {
            if (self->state != STATE_CLOSED) {
                self->state = STATE_CLOSED;
                self->tx.state = STATE_IDLE;
                if (self->event_callback == NULL) {
                    disconnect_event = self->event_callback->disconnect_event;
                }
            }
        }
    }
    UNLOCK(self);
    
    if (write_complete) {
        write_complete(self, NRC_R_OK);
    }
    
    if (disconnect_event) {
        disconnect_event(self, NRC_R_ERROR);
    }
}

static s32_t send_win32(SOCKET socket, WSAOVERLAPPED *overlapped, WSABUF *data_buf)
{
    s32_t result;
    DWORD flags = 0;
    DWORD send_bytes = 0;

    result = WSASend(socket, data_buf, 1, &send_bytes, flags, overlapped, tx_complete);
    if ((result == SOCKET_ERROR)) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            NRC_LOGE(_tag, "WSARecv returned error code %d", err);
        }
        else {
            result = NRC_R_OK;
        }
    }
    else {
        result = NRC_R_OK;
    }
    return result;
}

static void CALLBACK rx_complete(
    IN DWORD dwError,
    IN DWORD cbTransferred,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN DWORD dwFlags)
{
    struct nrc_port_socket *self = (struct nrc_port_socket *)lpOverlapped->Pointer;
    nrc_port_socket_event data_available = NULL;
    nrc_port_socket_event disconnect_event = NULL;
    
    NRC_ASSERT(self);

    LOCK(self);
    {
        if (cbTransferred > 0) {
            self->rx.state = STATE_IDLE;
            nrc_misc_cbuf_write_buf_consumed(self->rx.cbuf, cbTransferred);
            if (self->rx.notify_data_available) {
                self->rx.notify_data_available = FALSE;
                if (self->event_callback) {
                    data_available = self->event_callback->data_available;
                }
            }
            start_receive(self);
        }
        else {
            if (self->state != STATE_CLOSED) {
                self->state = STATE_CLOSED;
                self->rx.state = STATE_IDLE;
                if (self->event_callback) {
                    disconnect_event = self->event_callback->disconnect_event;
                }
            }
        }
    }
    UNLOCK(self);

    if (data_available) {
        data_available(self, NRC_R_OK);
    }

    if (disconnect_event) {
        disconnect_event(self, NRC_R_OK);
    }
}

