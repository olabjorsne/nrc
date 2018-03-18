#include "nrc_misc_cbuf.h"
#include "nrc_port.h"
#include <string.h>

#define NRC_MISC_CBUF_TYPE   (0xA73481B6)

struct nrc_misc_cbuf {
    u32_t   type;

    u8_t    *buf;
    u32_t   buf_size;

    u32_t   bytes;
    u32_t   read_index;
    u32_t   write_index;
};

s32_t nrc_misc_cbuf_init(u8_t *buf, u32_t buf_size, nrc_misc_cbuf_t *cbuf)
{
    s32_t                   result = NRC_R_OK;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)nrc_port_heap_alloc(sizeof(struct nrc_misc_cbuf));

    if (self != NULL) {
        self->type = NRC_MISC_CBUF_TYPE;
        self->buf = buf;
        self->buf_size = buf_size;
        self->bytes = 0;
        self->read_index = 0;
        self->write_index = 0;

        *cbuf = self;
    }
    else {
        *cbuf = NULL;

        result = NRC_R_OUT_OF_MEM;
    }

    return result;
}

s32_t nrc_misc_cbuf_deinit(nrc_misc_cbuf_t cbuf)
{
    s32_t                   result = NRC_R_OK;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);

    self->type = 0;

    nrc_port_heap_free(self);

    return result;
}

bool_t nrc_misc_cbuf_is_empty(nrc_misc_cbuf_t cbuf)
{
    bool_t                  empty = FALSE;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);

    if (self->bytes == 0) {
        empty = TRUE;
    }

    return empty;
}

bool_t nrc_misc_cbuf_is_full(nrc_misc_cbuf_t cbuf)
{
    bool_t                  full = FALSE;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);

    if (self->bytes == self->buf_size) {
        full = TRUE;
    }

    return full;
}

u32_t nrc_misc_cbuf_get_bytes(nrc_misc_cbuf_t cbuf)
{
    struct nrc_misc_cbuf *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);

    return self->bytes;
}

s32_t nrc_misc_cbuf_clear(nrc_misc_cbuf_t cbuf)
{
    s32_t                   result = NRC_R_INVALID_IN_PARAM;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    if ((self != NULL) && (self->type == NRC_MISC_CBUF_TYPE)) {
        self->read_index = 0;
        self->write_index = 0;
        self->bytes = 0;

        result = NRC_R_OK;
    }

    return result;
}

u32_t nrc_misc_cbuf_read(nrc_misc_cbuf_t cbuf, u8_t *buf, u32_t buf_size)
{
    u32_t                   bytes = 0;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;
    u32_t                   read = 0;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);

    if ((buf != NULL) && (buf_size > 0) && (self->bytes > 0)) {
        do {
            if (self->write_index > self->read_index) {
                bytes = self->write_index - self->read_index;
            }
            else {
                bytes = self->buf_size - self->read_index;
            }
            NRC_PORT_ASSERT(bytes <= self->bytes);

            if (bytes > (buf_size - read)) {
                bytes = (buf_size - read);
            }

            memcpy(&buf[read], &self->buf[self->read_index], bytes);

            self->read_index = (self->read_index + bytes) % self->buf_size;
            self->bytes -= bytes;
            read += bytes;

        } while ((read < buf_size) && (self->bytes > 0));
    }
    NRC_PORT_ASSERT(read <= buf_size);

    return read;
}

u32_t nrc_misc_cbuf_write(nrc_misc_cbuf_t cbuf, u8_t *data, u32_t data_size)
{
    u32_t                   bytes = 0;
    u32_t                   written = 0;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);

    if ((data != NULL) && (data_size > 0) && (self->bytes < self->buf_size)) {
        do {
            if ((self->bytes == 0) || (self->write_index > self->read_index)) {
                bytes = self->buf_size - self->write_index;
            }
            else {
                bytes = self->read_index - self->write_index;
            }
            NRC_PORT_ASSERT((bytes + self->bytes) <= self->buf_size);

            if (bytes > (data_size - written)) {
                bytes = data_size - written;
            }

            memcpy(&self->buf[self->write_index], &data[written], bytes);

            self->write_index = (self->write_index + bytes) % self->buf_size;
            self->bytes += bytes;
            written += bytes;

        } while ((written < data_size) && (self->bytes < self->buf_size));
    }

    return written;
}

u8_t* nrc_misc_cbuf_get_read_buf(nrc_misc_cbuf_t cbuf, u32_t *buf_size)
{
    u8_t                    *buf = NULL;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);
    NRC_PORT_ASSERT(buf_size != NULL);

    if (self->bytes > 0) {
        if (self->write_index > self->read_index) {
            *buf_size = self->write_index - self->read_index;
        }
        else {
            *buf_size = self->buf_size - self->read_index;
        }
        buf = &self->buf[self->read_index];
    }
    else {
        buf = NULL;
        *buf_size = 0;
    }

    return buf;
}

void nrc_misc_cbuf_read_buf_consumed(nrc_misc_cbuf_t cbuf, u32_t bytes)
{
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);
    
    NRC_PORT_ASSERT(self->bytes >= bytes);
    self->bytes -= bytes;

    self->read_index = (self->read_index + bytes);
    NRC_PORT_ASSERT(self->read_index <= self->buf_size);
    self->read_index %= self->buf_size;
}

u8_t* nrc_misc_cbuf_get_write_buf(nrc_misc_cbuf_t cbuf, u32_t *buf_size)
{
    u8_t                    *buf = NULL;
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);
    NRC_PORT_ASSERT(buf_size != NULL);

    if (self->bytes < self->buf_size) {
        if ((self->bytes == 0) || (self->write_index > self->read_index)) {
            *buf_size = self->buf_size - self->write_index;
        }
        else {
            *buf_size = self->read_index - self->write_index;
        }
        buf = &self->buf[self->write_index];
    }
    else {
        buf = NULL;
        *buf_size = 0;
    }

    return buf;
}

void nrc_misc_cbuf_write_buf_consumed(nrc_misc_cbuf_t cbuf, u32_t bytes)
{
    struct nrc_misc_cbuf    *self = (struct nrc_misc_cbuf*)cbuf;

    NRC_PORT_ASSERT(self != NULL);
    NRC_PORT_ASSERT(self->type == NRC_MISC_CBUF_TYPE);

    self->bytes += bytes;
    NRC_PORT_ASSERT(self->bytes <= self->buf_size);

    self->write_index += bytes;
    NRC_PORT_ASSERT(self->write_index <= self->buf_size);
    self->write_index %= self->buf_size;
}


