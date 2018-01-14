#include "test_port_uart_echo.h"
#include <assert.h>

static struct nrc_port_uart_pars    pars;
static nrc_port_uart_t              uart;
static bool_t                       uart_tx = FALSE;
static u8_t                         buf[1024];

static bool_t transfer(void)
{
    u32_t bytes;
    s32_t result;
    bool_t ok = FALSE;

    bytes = nrc_port_uart_read(uart, buf, 1024);

    if (bytes > 0) {
        result = nrc_port_uart_write(uart, buf, bytes);
        assert(result == NRC_PORT_RES_OK);

        ok = TRUE;
    }

    return ok;
}


static void data_available(nrc_port_uart_t uart)
{
    if(uart_tx == FALSE) {
        if (transfer() == TRUE) {
            uart_tx = TRUE;
        }
    }
}
static void write_complete(nrc_port_uart_t uart, s32_t result, u32_t bytes)
{
    assert(uart_tx == TRUE);

    if (transfer() == FALSE) {
        uart_tx = FALSE;
    }
}
static void error(nrc_port_uart_t uart, s32_t error)
{
    assert(FALSE);
}

bool_t test_port_uart_echo(u8_t port)
{
    s32_t result;
    struct nrc_port_uart_callback_fcn fcn = {data_available, write_complete, error};

    result = nrc_port_uart_init();
    assert(result == NRC_PORT_RES_OK);

    pars.baud_rate = 115200;
    pars.data_bits = 8;
    pars.flow_ctrl = NRC_PORT_UART_FLOW_NONE;
    pars.parity = NRC_PORT_UART_PARITY_NONE;
    pars.stop_bits = 1;

    result = nrc_port_uart_open(port, pars, fcn, &uart);
    assert(result == NRC_PORT_RES_OK);

    return result;
}