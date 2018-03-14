#include "nrc_types.h"
#include "nrc_misc_cbuf.h"
#include <string.h>

bool_t test_cbuf_read_write(void)
{
    u8_t            buf[3];
    nrc_misc_cbuf_t cbuf;
    bool_t          ok = TRUE;
    bool_t          full;
    bool_t          empty;
    s32_t           result;
    u8_t            tbuf[] = { 1, 2, 3 };
    u32_t           bytes;

    result = nrc_misc_cbuf_init(buf, 3, &cbuf);
    if (result != NRC_R_OK) {
        ok = FALSE;
    }

    if (ok) {
        empty = nrc_misc_cbuf_is_empty(cbuf);
        full = nrc_misc_cbuf_is_full(cbuf);

        if ((!empty) || (full)) {
            ok = FALSE;
        }
    }

    if (ok) {
        bytes = nrc_misc_cbuf_write(cbuf, tbuf, 2);
        if (bytes != 2) {
            ok = FALSE;
        }
        if ((buf[0] != 1) || (buf[1] != 2)) {
            ok = FALSE;
        }
    }

    if (ok) {
        empty = nrc_misc_cbuf_is_empty(cbuf);
        full = nrc_misc_cbuf_is_full(cbuf);

        if ((empty) || (full)) {
            ok = FALSE;
        }
    }

    if (ok) {
        bytes = nrc_misc_cbuf_read(cbuf, tbuf, 1);
        if ((bytes != 1) || (tbuf[0] != 1)) {
            ok = FALSE;
        }
    }

    if (ok) {
        tbuf[0] = 3;
        tbuf[1] = 4;
        bytes = nrc_misc_cbuf_write(cbuf, tbuf, 3);
        if ((bytes != 2) || (buf[0] != 4) || (buf[1] != 2) || (buf[2] != 3)) {
            ok = FALSE;
        }
    }

    if (ok) {
        empty = nrc_misc_cbuf_is_empty(cbuf);
        full = nrc_misc_cbuf_is_full(cbuf);

        if ((empty) || (!full)) {
            ok = FALSE;
        }
    }

    if (ok) {
        memset(tbuf, 255, 3);
        bytes = nrc_misc_cbuf_read(cbuf, tbuf, 3);
        if ((bytes != 3) || (tbuf[0] != 2) || (tbuf[1] != 3) || (tbuf[2] != 4)) {
            ok = FALSE;
        }
    }

    if (ok) {
        result = nrc_misc_cbuf_deinit(cbuf);
        if (result != NRC_R_OK) {
            ok = FALSE;
        }
    }

    return ok;
}

bool_t test_cbuf_read_write_buf(void)
{
    u8_t            buf[3];
    nrc_misc_cbuf_t cbuf;
    bool_t          ok = TRUE;
    bool_t          full;
    bool_t          empty;
    s32_t           result;
    u8_t            *pbuf;
    u32_t           bytes;

    result = nrc_misc_cbuf_init(buf, 3, &cbuf);
    if (result != NRC_R_OK) {
        ok = FALSE;
    }

    if (ok) {

       pbuf = nrc_misc_cbuf_get_write_buf(cbuf, &bytes);
        if ((pbuf == NULL) || (bytes != 3)) {
            ok = FALSE;
        }
        pbuf[0] = 1;
        pbuf[1] = 2;
        nrc_misc_cbuf_write_buf_consumed(cbuf, 2);
        if ((buf[0] != 1) || (buf[1] != 2)) {
            ok = FALSE;
        }
    }

    if (ok) {
        empty = nrc_misc_cbuf_is_empty(cbuf);
        full = nrc_misc_cbuf_is_full(cbuf);

        if ((empty) || (full)) {
            ok = FALSE;
        }
    }

    if (ok) {
        pbuf = nrc_misc_cbuf_get_read_buf(cbuf, &bytes);
        if ((bytes != 2) || (pbuf[0] != 1) || (pbuf[1] != 2)) {
            ok = FALSE;
        }
        nrc_misc_cbuf_read_buf_consumed(cbuf, 1);
    }

    if (ok) {
        pbuf = nrc_misc_cbuf_get_write_buf(cbuf, &bytes);
        pbuf[0] = 3;
        nrc_misc_cbuf_write_buf_consumed(cbuf, 1);
        pbuf = nrc_misc_cbuf_get_write_buf(cbuf, &bytes);
        pbuf[0] = 4;
        nrc_misc_cbuf_write_buf_consumed(cbuf, 1);
        if ((bytes != 1) || (buf[0] != 4) || (buf[1] != 2) || (buf[2] != 3)) {
            ok = FALSE;
        }
    }

    if (ok) {
        empty = nrc_misc_cbuf_is_empty(cbuf);
        full = nrc_misc_cbuf_is_full(cbuf);

        if ((empty) || (!full)) {
            ok = FALSE;
        }
    }

    if (ok) {
        pbuf = nrc_misc_cbuf_get_read_buf(cbuf, &bytes);
        if ((bytes != 2) || (pbuf[0] != 2) || (pbuf[1] != 3)) {
            ok = FALSE;
        }
        nrc_misc_cbuf_read_buf_consumed(cbuf, bytes);
        pbuf = nrc_misc_cbuf_get_read_buf(cbuf, &bytes);
        if ((bytes != 1) || (pbuf[0] != 4)) {
            ok = FALSE;
        }
        nrc_misc_cbuf_read_buf_consumed(cbuf, bytes);
    }

    if (ok) {
        result = nrc_misc_cbuf_deinit(cbuf);
        if (result != NRC_R_OK) {
            ok = FALSE;
        }
    }

    return ok;
}