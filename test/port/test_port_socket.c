
#include "nrc_port_socket.h"
#include "nrc_assert.h"
#include "nrc_log.h"



static const s8_t *_tag = "socket_test";
static bool_t                       tx = FALSE;
static u8_t                         buf[1024];



static bool_t transfer(nrc_port_socket_t socket);
static void data_available(nrc_port_socket_t socket);
static void write_complete(nrc_port_socket_t socket);
static void connect_event(nrc_port_socket_t socket);
static void disconnect_event(nrc_port_socket_t socket);
static void remote_connect_evt(nrc_port_socket_t socket, void *context);

struct nrc_port_socket_callback_fcn callback = {
    .data_available = data_available,
    .write_complete = write_complete,
    .connect_event = connect_event,
    .disconnect_event = disconnect_event
};


static bool_t transfer(nrc_port_socket_t socket)
{
    u32_t bytes;
    s32_t result;
    bool_t ok = FALSE;

    bytes = nrc_port_socket_read(socket, buf, 1024);

    if (bytes > 0) {
        result = nrc_port_socket_write(socket, buf, bytes);
        NRC_ASSERT(result == NRC_R_OK);
        ok = TRUE;
    }

    return ok;
}

static void data_available(nrc_port_socket_t socket)
{
    if (tx == FALSE) {
        if (transfer(socket) == TRUE) {
            tx = TRUE;
        }
    }
}

// TODO context part of callback
static void write_complete(nrc_port_socket_t socket)
{
    NRC_ASSERT(tx == TRUE);

    if (transfer(socket) == FALSE) {
        tx = FALSE;
    }
}

static void remote_connect_evt(nrc_port_socket_t socket, void *context)
{
    NRC_LOGD(_tag, "Socket remote connect event");
}

static void connect_event(nrc_port_socket_t socket)
{
    NRC_LOGD(_tag, "Socket connect event");
    nrc_port_socket_close(socket);
}
static void disconnect_event(nrc_port_socket_t socket)
{
    NRC_LOGD(_tag, "Socket disconnect event");
}
void test_port_socket_client_echo(void)
{
    s32_t result;
    nrc_port_socket_t s,c;
    result = nrc_port_socket_init();
    NRC_ASSERT(result == NRC_R_OK);

    result = nrc_port_socket_create(NRC_PORT_SOCKET_TCP, NULL, &s);
    NRC_ASSERT(OK(result));
    NRC_ASSERT(s);

    result = nrc_port_socket_register(s, &callback);
    NRC_ASSERT(OK(result));

    result = nrc_port_socket_bind(s, 2121);
    NRC_ASSERT(OK(result));

    result = nrc_port_socket_listen(s, remote_connect_evt);
    NRC_ASSERT(OK(result));
#if 0
    result = nrc_port_socket_create(NRC_PORT_SOCKET_TCP, NULL, &c);
    NRC_ASSERT(OK(result));
    NRC_ASSERT(c);

    result = nrc_port_socket_register(c, &callback);
    NRC_ASSERT(OK(result));

    result = nrc_port_socket_connect(c, "127.0.0.1", 2121);
    NRC_ASSERT(OK(result));
#endif
}