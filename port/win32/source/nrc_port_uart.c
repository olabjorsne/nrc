#include "nrc_port_uart.h"
#include "nrc_misc_cbuf.h"
#include <assert.h>
#include <Windows.h>
#include <stdio.h>

#define NRC_PORT_UART_TYPE      (0x1362B64F)

#define NRC_PORT_UART_BUF_SIZE  (1024)
#define NRC_PORT_UART_READ_SIZE (256)

enum nrc_port_uart_state {
    NRC_PORT_UART_S_IDLE,
    NRC_PORT_UART_S_WRITING,
    NRC_PORT_UART_S_READING
};

struct nrc_port_uart {
    u32_t                               type;

    enum nrc_port_uart_state_tx         tx_state;
    enum nrc_port_uart_state_rx         rx_state;

    nrc_port_mutex_t                    mutex;

    struct nrc_port_uart_callback_fcn   callback;
    bool_t                              notify_data_available;

    HANDLE                              hPort;
    OVERLAPPED                          tx_overlapped;
    OVERLAPPED                          rx_overlapped;

    u8_t                                rx_buf[NRC_PORT_UART_BUF_SIZE];
    nrc_misc_cbuf_t                     cbuf;
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
    u8_t    file_name[32];
    s32_t   len;

    HANDLE                  hPort;
    BOOL                    ok;
    DCB                     dcb;
    COMMTIMEOUTS            timeouts;
    struct nrc_port_uart    *self = NULL;

    assert(uart != NULL);
    *uart = NULL;

    len = snprintf(file_name, 32, "\\\\.\\COM%d", port);
    //len = snprintf(file_name, 32, "\\\\.\\COM3");
    assert(len < 32);

    hPort = CreateFileA(file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hPort == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
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
            self->notify_data_available = TRUE;
            self->hPort = hPort;
            self->type = NRC_PORT_UART_TYPE;

            result = nrc_port_mutex_init(&self->mutex);

            if (result == NRC_PORT_RES_OK) {
                result = nrc_misc_cbuf_init(self->rx_buf, NRC_PORT_UART_BUF_SIZE, &self->cbuf);
            }
        }
        else {
            result = NRC_PORT_RES_OUT_OF_MEM;
        }
    }

    if (result == NRC_PORT_RES_OK) {
        u32_t   buf_size = 0;
        u8_t    *buf = nrc_misc_cbuf_get_write_buf(self->cbuf, &buf_size);

        assert((buf != NULL) && (buf_size > 0));

        self->rx_overlapped.Offset = 0;
        self->rx_overlapped.OffsetHigh = 0;
        self->rx_overlapped.hEvent = self;

        ok = ReadFileEx(hPort, buf, buf_size, &self->rx_overlapped, read_complete);
        if (ok == TRUE) {
            self->rx_state = NRC_PORT_UART_S_READING;
        }
        else {
            DWORD err = GetLastError();
            result = NRC_PORT_RES_ERROR;
        }
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
    s32_t                   result = NRC_PORT_RES_OK;
    BOOL                    ok;

    assert(self != NULL);
    assert(self->type == NRC_PORT_UART_TYPE);

    result = nrc_port_mutex_lock(self->mutex, 0);
    assert(result == NRC_PORT_RES_OK);

    ok = CloseHandle(self->hPort);

    result = nrc_port_mutex_unlock(self->mutex);
    assert(result == NRC_PORT_RES_OK);

    nrc_port_heap_free(self);

    return result;
}

s32_t nrc_port_uart_write(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size)
{
    struct nrc_port_uart    *self = (struct nrc_port_uart*)uart;
    s32_t                   result = NRC_PORT_RES_OK;
    s32_t                   result2;
    BOOL                    ok;
    
    assert(self != NULL);
    assert(self->type == NRC_PORT_UART_TYPE);

    result2 = nrc_port_mutex_lock(self->mutex, 0);
    assert(result2 == NRC_PORT_RES_OK);

    switch (self->tx_state) {
    case NRC_PORT_UART_S_IDLE:
        self->tx_overlapped.Offset = 0xFFFFFFFF;
        self->tx_overlapped.OffsetHigh = 0xFFFFFFFF;
        self->tx_overlapped.hEvent = self;

        ok = WriteFileEx(self->hPort, buf, buf_size, &self->tx_overlapped, write_complete);
        if (ok == TRUE) {
            self->tx_state = NRC_PORT_UART_S_WRITING;
        }
        else {
            result = NRC_PORT_RES_ERROR;
        }
        break;
    default:
        result = NRC_PORT_RES_INVALID_STATE;
        break;
    }

    result2 = nrc_port_mutex_unlock(self->mutex);
    assert(result2 == NRC_PORT_RES_OK);

    return result;
}

u32_t nrc_port_uart_read(nrc_port_uart_t uart, u8_t *buf, u32_t buf_size)
{
    struct nrc_port_uart    *self = (struct nrc_port_uart*)uart;
    u32_t                   read_bytes = 0;
    s32_t                   result;

    assert(self != NULL);
    assert(self->type == NRC_PORT_UART_TYPE);

    result = nrc_port_mutex_lock(self->mutex, 0);
    assert(result == NRC_PORT_RES_OK);

    read_bytes = nrc_misc_cbuf_read(self->cbuf, buf, buf_size);

    if (read_bytes == 0) {
        self->notify_data_available = TRUE;
    }

    result = nrc_port_mutex_unlock(self->mutex);
    assert(result == NRC_PORT_RES_OK);

    return read_bytes;
}

static VOID CALLBACK write_complete(
    _In_    DWORD        dwErrorCode,
    _In_    DWORD        dwNumberOfBytesTransfered,
    _Inout_ LPOVERLAPPED lpOverlapped)
{
    struct nrc_port_uart            *self;
    s32_t                           result;
    nrc_port_uart_write_complete_t  notify_fcn;
    s32_t                           notify_result;
    u32_t                           notify_bytes;

    if (lpOverlapped != NULL) {
        self = (struct nrc_port_uart*)lpOverlapped->hEvent;

        result = nrc_port_mutex_lock(self->mutex, 0);
        assert(result == NRC_PORT_RES_OK);

        assert(self != NULL);
        assert(self->type == NRC_PORT_UART_TYPE);
        assert(self->tx_state == NRC_PORT_UART_S_WRITING);
        assert(self->callback.write_complete != NULL);

        self->tx_state = NRC_PORT_UART_S_IDLE;

        if (dwErrorCode == 0) {
            notify_result = NRC_PORT_RES_OK;
            notify_bytes = dwNumberOfBytesTransfered;
        }
        else {
            notify_result = NRC_PORT_RES_ERROR;
            notify_bytes = 0;
        }
        notify_fcn = self->callback.write_complete;

        result = nrc_port_mutex_unlock(self->mutex);
        assert(result == NRC_PORT_RES_OK);

        notify_fcn(self, notify_result, notify_bytes);
    }
}

static VOID CALLBACK read_complete(
    _In_    DWORD        dwErrorCode,
    _In_    DWORD        dwNumberOfBytesTransfered,
    _Inout_ LPOVERLAPPED lpOverlapped)
{
    struct nrc_port_uart            *self;
    s32_t                           result;
    u32_t                           buf_size;
    u8_t                            *buf;
    BOOL                            ok;
    nrc_port_uart_data_available_t  data_available = NULL;
    nrc_port_uart_error_t           error = NULL;
    s32_t                           error_code;

    if (lpOverlapped != NULL) {
        self = (struct nrc_port_uart*)lpOverlapped->hEvent;

        assert(self != NULL);
        assert(self->type == NRC_PORT_UART_TYPE);
        assert(self->rx_state == NRC_PORT_UART_S_READING);

        result = nrc_port_mutex_lock(self->mutex, 0);
        assert(result == NRC_PORT_RES_OK);

        self->rx_state = NRC_PORT_UART_S_IDLE;

        nrc_misc_cbuf_write_buf_consumed(self->cbuf, dwNumberOfBytesTransfered);

        if (dwErrorCode == 0) {
            if (self->notify_data_available == TRUE) {
                self->notify_data_available = FALSE;
                data_available = self->callback.data_available;
            }

            buf = nrc_misc_cbuf_get_write_buf(self->cbuf, &buf_size);

            if ((buf != NULL) && (buf_size > 0)) {

                self->rx_overlapped.Offset = 0;
                self->rx_overlapped.OffsetHigh = 0;
                self->rx_overlapped.hEvent = self;

                ok = ReadFileEx(self->hPort, buf, buf_size, &self->rx_overlapped, read_complete);
                if (ok == TRUE) {
                    self->rx_state = NRC_PORT_UART_S_READING;
                }
                else {
                    error = self->callback.error;
                    error_code = NRC_PORT_RES_READ_FAILURE;
                }
            }
        }
        else {
            error = self->callback.error;
            error_code = NRC_PORT_RES_READ_FAILURE;
        }

        result = nrc_port_mutex_unlock(self->mutex);
        assert(result == NRC_PORT_RES_OK);

        if (data_available != NULL) {
            data_available(self);
        }
        if (error != NULL) {
            error(self, error_code);
        }
    }
}