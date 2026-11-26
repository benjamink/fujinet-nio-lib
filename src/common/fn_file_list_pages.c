#include <string.h>

#include "fn_internal.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

#define LIST_PAGES_VERSION 1
#define LIST_PAGES_HEADER  12   /* version .. pageCount, before the pages */

static uint8_t map_status(uint8_t status)
{
    switch (status) {
        case 0: return FN_OK;
        case 1: return FN_ERR_NOT_FOUND;
        case 2: return FN_ERR_INVALID;
        case 3: return FN_ERR_BUSY;
        case 4: return FN_ERR_NOT_READY;
        case 5: return FN_ERR_IO;
        case 6: return FN_ERR_TIMEOUT;
        case 7: return FN_ERR_INTERNAL;
        case 8: return FN_ERR_UNSUPPORTED;
        default: return FN_ERR_UNKNOWN;
    }
}

uint8_t fn_file_list_pages(fn_file_io_t *io,
                           const char *uri,
                           const fn_list_pages_req_t *req,
                           fn_list_pages_t *out)
{
    fn_raw_response_t raw;
    uint8_t *buf;
    uint16_t uri_len;
    uint16_t off;
    uint8_t filter_len;
    uint8_t result;

    if (!io || !io->buffer || !uri || !req || !out) {
        return FN_ERR_INVALID;
    }
    uri_len = (uint16_t)strlen(uri);
    filter_len = req->filter ? (uint8_t) strlen(req->filter) : 0;
    if (req->filter && strlen(req->filter) > 255) {
        return FN_ERR_INVALID;
    }
    off = (uint16_t)(12 + (filter_len ? filter_len + 1 : 0));
    if (uri_len == 0 || req->page_size == 0 || io->capacity < off ||
        uri_len > (uint16_t)(io->capacity - off)) {
        return FN_ERR_INVALID;
    }
    buf = io->buffer;
    buf[0] = LIST_PAGES_VERSION;
    buf[1] = (uint8_t)uri_len;
    buf[2] = (uint8_t)(uri_len >> 8);
    memcpy(&buf[3], uri, uri_len);
    off = (uint16_t)(3 + uri_len);
    /* startPage, pageSize, maxPayload, flags, maxName: the same bytes as the struct,
     * apart from the layout padding a compiler may add */
    buf[off++] = (uint8_t)req->start_page;
    buf[off++] = (uint8_t)(req->start_page >> 8);
    buf[off++] = req->page_size;
    buf[off++] = (uint8_t)req->max_payload;
    buf[off++] = (uint8_t)(req->max_payload >> 8);
    buf[off++] = req->flags;
    buf[off++] = req->max_name;
    /* Optional trailing field: older firmware accepts the fixed v1 prefix. */
    if (filter_len) {
        buf[off++] = filter_len;
        memcpy(buf + off, req->filter, filter_len);
        off = (uint16_t)(off + filter_len);
    }

    result = fn_raw_call(FN_DEVICE_FILE, FN_CMD_FILE_LIST_PAGES, buf, off,
                         buf, io->capacity, &raw);
    if (result != FN_OK) {
        return result;
    }
    result = map_status(raw.status);
    if (result != FN_OK) {
        return result;
    }
    if (raw.payload_length < LIST_PAGES_HEADER || buf[0] != LIST_PAGES_VERSION) {
        return FN_ERR_IO;
    }
    out->flags = buf[1];
    out->total = FN_GET_LE32(&buf[4]);
    out->start_page = FN_GET_LE16(&buf[8]);
    out->page_size = buf[10];
    out->page_count = buf[11];
    out->pages = buf + LIST_PAGES_HEADER;
    out->pages_len = (uint16_t)(raw.payload_length - LIST_PAGES_HEADER);
    return FN_OK;
}
