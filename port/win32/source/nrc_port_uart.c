#include "nrc_port_uart.h"
#include <assert.h>
#include <Windows.h>
#include <stdio.h>

#define NRC_PORT_UART_TYPE (0x1362B64F)

enum nrc_port_uart_state {
    NRC_PORT_UART_S_IDLE,
    NRC_PORT_UART_S_BUSY
};

struct nrc_port_uart {
    u32_t                               type;

    enum nrc_port_uart_state            tx_state;
    enum nrc_port_uart_state            rx_state;

    struct nrc_port_uart_callback_fcn   callback;

    HANDLE                              hPort;
    OVERLAPPED                          tx_overlapped;
    OVERLAPPED                          rx_overlapped;
};

static VOID CALLBACK write_complete(
    _In_    DWORD        dwErrorCode,
    _In_    DWORD        dwNumberOfBytesTransfered,
    _Inout_ LPOVERLAPPED lpOverlapped);

static VOID CALLBACK read_complete(
    _In_    DWORD        dwErrorCode,
    _In_    DWORD        dwNumberOfBytesTransfered,
    _Inout_ LPOVERLAPPED lpOverlapped);

static bool_t _initialized = FALSE;

s32_t nrc_port_uart_init(void)
{
    s32_t result = NRC_PORT_RES_OK;

    if (_initialized == FALSE) {
        _initialized = TRUE;
    }

    return result;
}

s32_t nrc_port_uart_open(
    u8_t                                port,
    struct nrc_port_uart_pars           pars,
    struct nrc_port_uart_callback_fcn   callback,
    nrc_port_uart_t                     *uart)
{
    s32_t   result = NRC_PORT_RES_OK;
    s8_t    file_name[32];
    s32_t   len;

    HANDLE                  hPort;
    BOOL                    ok;
    DCB                     dcb;
    COMMTIMEOUTS            timeouts;
    struct nrc_port_uart    *self = NULL;

    assert(uart != NULL);
    *uart = NULL;

    len = snprintf(file_name, 32, "\\\\.\\COM%d", port);
    assert(len < 32);

    hPort = CreateFile((LPCTSTR)file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hPort != INVALID_HANDLE_VALUE) {
        result = NRC_PORT_RES_RESOURCE_UNAVAILABLE;
    }

    if (result == NRC_PORT_RES_OK) {
        memset(&dcb, 0, sizeof(dcb));

        ok = GetCommState(hPort, &dcb);
        if (ok == FALSE) {
            result = NRC_PORT_RES_RESOURCE_UNAVAILABLE;
        }
    }

    if (result == NRC_PORT_RES_OK) {
        dcb.BaudRate = pars.baud_rate;
        dcb.ByteSize = pars.data_bits;
        
        switch(pars.stop_bits) {
        case 1:
            dcb.StopBits = ONESTOPBIT;
            break;
        case 2:
            dcb.StopBits = TWOSTOPBITS;
            break;
        default:
            result = NRC_PORT_RES_INVALID_IN_PARAM;
            break;
        }

        dcb.fParity = TRUE;
        switch (pars.parity) {
        case NRC_PORT_UART_PARITY_EVEN:
            dcb.Parity = EVENPARITY;
            break;
        case NRC_PORT_UART_PARITY_ODD:
            dcb.Parity = ODDPARITY;
            break;
        default:
            dcb.Parity = NOPARITY;
            break;
        }
        
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDsrSensitivity = FALSE;
        if (pars.flow_ctrl == NRC_PORT_UART_FLOW_HW) {
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutxCtsFlow = TRUE;
        }
        else {
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            dcb.fOutxCtsFlow = FALSE;
        }

        ok = SetCommState(hPort, &dcb);
        if (ok == FALSE) {
            result = NRC_PORT_RES_INVALID_IN_PARAM;
        }
    }

    if (result == NRC_PORT_RES_OK) {
        memset(&timeouts, 0, sizeof(COMMTIMEOUTS));

        ok = GetCommTimeouts(hPort, &timeouts);
        if (ok == FALSE) {
            result = NRC_PORT_RES_RESOURCE_UNAVAILABLE;
        }
    }

    if (result == NRC_PORT_RES_OK) {
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;

        ok = SetCommTimeouts(hPort, &timeouts);
        if (ok == FALSE) {
            result = NRC_PORT_RES_INVALID_IN_PARAM;
        }
    }

    if (result == NRC_PORT_RES_OK) {
        self = (struct nrc_port_uart*)nrc_port_heap_alloc(sizeof(struct nrc_port_uart));
        if (self != NULL) {
            self->rx_state = NRC_PORT_UART_S_IDLE;
            self->tx_state = NRC_PORT_UART_S_IDLE;
            self->callback = callback;
            self->hPort = hPort;
            self->type = NRC_PORT_UART_TYPE;
        }
        else {
            result = NRC_PORT_RES_OUT_OF_MEM;
        }
    }

    if (result == NRC_PORT_RES_OK) {
        //TODO: Start reading to circular buffer
    }

    if (result == NRC_PORT_RES_OK) {
        *uart = self;
    }
    else {
        if (hPort != INVALID_HANDLE_VALUE) {
            ok = CloseHandle(hPort);
        }
        if (self != NULL) {
            nrc_port_heap_free(self);
        }
    }

    return result;
}

s32_t nrc_port_uart_close(nrc_port_uart_t uart)
{
    struct nrc_port_uart    *self = (struct nrc_port_uart*)uart;
    s32_t                   result = NRC_PORT_RES_ERROR;
    BOOL                    ok;

    assert(self != NULL);
    assert(self->type == NRC_PORT_UART_TYPE);

    ok = CloseHandle(self->hPort);

    nrc_port_heap_free(self);

    return result;
}

s32_t nrc_port_uart_write(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size)
{
    struct nrc_port_uart    *self = (struct nrc_port_uart*)uart;
    s32_t                   result = NRC_PORT_RES_OK;
    BOOL                    ok;
    
    assert(self != NULL);
    assert(self->type == NRC_PORT_UART_TYPE);

    switch (self->tx_state) {
    case NRC_PORT_UART_S_IDLE:
        self->tx_overlapped.Offset = 0xFFFFFFFF;
        self->tx_overlapped.OffsetHigh = 0xFFFFFFFF;
        self->tx_overlapped.hEvent = self;

        ok = WriteFileEx(self->hPort, buf, buf_size, &self->tx_overlapped, write_complete);
        if (ok == TRUE) {
            self->tx_state = NRC_PORT_UART_S_BUSY;
        }
        else {
            result = NRC_PORT_RES_ERROR;
        }
        break;
    default:
        result = NRC_PORT_RES_INVALID_STATE;
        break;
    }

    return result;
}

u32_t nrc_port_uart_read(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size)
{
    s32_t result = NRC_PORT_RES_ERROR;

    return result;
}

static VOID CALLBACK write_complete(
    _In_    DWORD        dwErrorCode,
    _In_    DWORD        dwNumberOfBytesTransfered,
    _Inout_ LPOVERLAPPED lpOverlapped)
{
    struct nrc_port_uart *self;

    if (lpOverlapped != NULL) {
        self = (struct nrc_port_uart*)lpOverlapped->hEvent;

        assert(self != NULL);
        assert(self->type == NRC_PORT_UART_TYPE);

        if (self->tx_state == NRC_PORT_UART_S_BUSY) {
            assert(self->callback.write_complete != NULL);

            self->tx_state = NRC_PORT_UART_S_IDLE;

            if (dwErrorCode == 0) {
                self->callback.write_complete(self, NRC_PORT_RES_OK, dwNumberOfBytesTransfered);
            }
            else {
                self->callback.write_complete(self, NRC_PORT_RES_ERROR, 0);
            }
        }
    }
}

static VOID CALLBACK read_complete(
    _In_    DWORD        dwErrorCode,
    _In_    DWORD        dwNumberOfBytesTransfered,
    _Inout_ LPOVERLAPPED lpOverlapped)
{
    struct nrc_port_uart *self;

    if (lpOverlapped != NULL) {
        self = (struct nrc_port_uart*)lpOverlapped->hEvent;

        assert(self != NULL);
        assert(self->type == NRC_PORT_UART_TYPE);

        if (self->rx_state == NRC_PORT_UART_S_BUSY) {
            self->rx_state = NRC_PORT_UART_S_IDLE;

            //TODO: If circular buffer is not yet full, start reading
        }
    }
}