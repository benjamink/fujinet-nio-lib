/* NetworkDevice (0xFD): tcp:// streams and http:// requests over nio_sock.
 *
 * Wire formats follow fujinet-nio's docs/network_device_protocol.md. What is
 * left out: TLS (https://, the tls flag), redirects, content translation and
 * the Ready command; each answers Unsupported. HTTP is spoken as HTTP/1.0
 * with "Connection: close", so a body is never chunked and ends at the
 * Content-Length or when the server closes. A request body of unknown length
 * is gathered in the session's buffer until its commit, so it is limited to
 * NIO_RX_BYTES.
 */
#include <string.h>
#include "nio.h"
#include "nio_sock.h"

/* Text constants the library audit (scripts/palmos_shared_image.py) checks byte for byte: objdump decodes them
 * as instructions, some through A5. */
static const char k_http_host[] = " HTTP/1.0\r\nHost: ";
static const char k_http_agent[] = "\r\nUser-Agent: fujinet-nio-palmOS\r\nConnection: close\r\n";
static const char k_crlf[] = "\r\n";
static const char k_crlf2[] = "\r\n\r\n";
static const char k_get[] = "GET", k_post[] = "POST", k_put[] = "PUT", k_delete[] = "DELETE", k_head[] = "HEAD";
static const char k_type_json[] = "application/json";
static const char k_type_form[] = "application/x-www-form-urlencoded";
static const char k_type_text[] = "text/plain";

#define CMD_OPEN  0x01
#define CMD_READ  0x02
#define CMD_WRITE 0x03
#define CMD_CLOSE 0x04
#define CMD_INFO  0x05

#define OPEN_FLAG_TLS 0x01
#define OPEN_FLAG_BODY_UNKNOWN 0x04

#define READ_EOF       0x01
#define READ_TRUNCATED 0x02

#define INFO_HEADERS 0x01
#define INFO_LENGTH  0x02
#define INFO_STATUS  0x04

/* How long one Read or Info waits for the server before NotReady. A TCP read
 * does not wait: its caller polls from an event loop. */
#define HTTP_WAIT_MS 500

#define HOST_MAX 96

/* A switch, not a table of pointers: such a table would be global data, which
 * the core can't have in a Palm shared library. */
static const char *method_name(uint8_t method)
{
    switch (method) {
    case 2: return k_post;
    case 3: return k_put;
    case 4: return k_delete;
    case 5: return k_head;
    }
    return k_get;
}

uint8_t nio_net_up(NioState *nio)
{
    if (nio->net_up) return NIO_ST_OK;
    if (nio_sock_start(&nio->net) != 0) return NIO_ST_NOT_READY;
    nio->net_up = 1;
    return NIO_ST_OK;
}

static void session_close(NioSession *s)
{
    if (s->used) nio_sock_close(s->net, s->sock);
    s->used = 0;
}

void nio_shutdown(NioState *nio)
{
    uint8_t i;

    for (i = 0; i < NIO_SESSIONS; i++) session_close(&nio->sessions[i]);
    if (nio->net_up) nio_sock_stop(&nio->net);
    nio->net_up = 0;
}

void nio_net_down(NioState *nio)
{
    nio->net.pad = 1;                   /* the stack's link goes now, not after its idle time */
    nio_shutdown(nio);
    nio->net.pad = 0;
}

uint8_t nio_sessions_open(const NioState *nio)
{
    uint8_t i, n = 0;

    for (i = 0; i < NIO_SESSIONS; i++) n = (uint8_t)(n + nio->sessions[i].used);
    return n;
}

static NioSession *find(NioState *nio, uint16_t handle)
{
    uint8_t i;

    for (i = 0; i < NIO_SESSIONS; i++)
        if (nio->sessions[i].used && nio->sessions[i].handle == handle)
            return &nio->sessions[i];
    return 0;
}

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Nonzero when text holds a byte that would break the request line or a header line apart: kind 0 is a URL (space and
 * controls), 1 a header name (those and ':'), 2 a header value (controls other than a tab). 0x7F is always refused. */
static uint8_t bad_text(const uint8_t *p, uint16_t n, uint8_t kind)
{
    uint8_t c;

    while (n--) {
        c = *p++;
        if (c == 0x7F || (kind == 2 ? (c < 0x20 && c != 9) : (c <= 0x20 || (kind == 1 && c == ':')))) return 1;
    }
    return 0;
}

static uint8_t send_all(NioSession *s, const uint8_t *p, uint16_t len)
{
    int n;

    while (len) {
        n = nio_sock_send(s->net, s->sock, p, len);
        if (n <= 0) {
            s->failed = 1;
            return 0;
        }
        s->net->bytesOut += (uint32_t)n;
        p += n;
        len = (uint16_t)(len - n);
    }
    return 1;
}

/* The HTTP request is put together in the session's receive buffer, which
 * is idle until the reply comes, and sent whenever it fills. */
static uint8_t put(NioSession *s, const char *text, uint16_t len)
{
    uint16_t room;

    while (len) {
        room = (uint16_t)(NIO_RX_BYTES - s->rx_len);
        if (room > len) room = len;
        memcpy(s->rx + s->rx_len, text, room);
        s->rx_len = (uint16_t)(s->rx_len + room);
        text += room;
        len = (uint16_t)(len - room);
        if (s->rx_len == NIO_RX_BYTES) {
            if (!send_all(s, s->rx, s->rx_len)) return 0;
            s->rx_len = 0;
        }
    }
    return 1;
}

static uint8_t puts_(NioSession *s, const char *text)
{
    return put(s, text, (uint16_t)strlen(text));
}

static uint8_t put_number(NioSession *s, uint32_t v)
{
    char digits[10];
    uint8_t n = 0;

    do {
        digits[9 - n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    return put(s, digits + 10 - n, n);
}

/* Splits "scheme://host[:port][/path]". host gets a copy; *path points into
 * url at the '/' (or '?'), or at its end. Zero if it does not parse. */
static uint8_t split_url(const char *url, uint16_t len, uint8_t *http, char *host,
                         uint16_t *port, uint16_t *path)
{
    uint16_t at, n = 0;
    uint32_t p = 0;

    if (len > 6 && memcmp(url, "tcp://", 6) == 0) {
        *http = 0;
        at = 6;
    } else if (len > 7 && memcmp(url, "http://", 7) == 0) {
        *http = 1;
        at = 7;
        p = 80;
    } else {
        return 0;
    }
    while (at < len && url[at] != ':' && url[at] != '/' && url[at] != '?') {
        if (n >= HOST_MAX - 1) return 0;
        host[n++] = url[at++];
    }
    host[n] = 0;
    if (!n) return 0;
    if (at < len && url[at] == ':') {
        p = 0;
        at++;
        while (at < len && url[at] >= '0' && url[at] <= '9') {
            p = p * 10 + (uint32_t)(url[at++] - '0');
            if (p > 65535UL) return 0;
        }
    }
    if (!p) return 0;
    *port = (uint16_t)p;
    *path = at;
    return 1;
}

/* A status line or header line, without its line end. */
static void http_line(NioSession *s, const char *line, uint16_t len)
{
    uint16_t i, n;
    const char *w;
    uint32_t v;

    if (!s->http_status) {
        /* "HTTP/1.x 200 OK" */
        for (i = 0; i < len && line[i] != ' '; i++) {}
        v = 0;
        for (i++; i < len && line[i] >= '0' && line[i] <= '9'; i++)
            v = v * 10 + (uint32_t)(line[i] - '0');
        s->http_status = v ? (uint16_t)v : 1;
        return;
    }
    if (!len) {
        s->headers_done = 1;
        return;
    }
    if (len > 15) {
        for (i = 0; i < 15 && lower(line[i]) == "content-length:"[i]; i++) {}
        if (i == 15) {
            v = 0;
            while (i < len && line[i] == ' ') i++;
            for (; i < len && line[i] >= '0' && line[i] <= '9'; i++)
                v = v * 10 + (uint32_t)(line[i] - '0');
            s->content_length = v;
            s->has_length = 1;
        }
    }
    /* Keep the line if its name is one the client asked for. */
    for (w = s->want; *w; w += n) {
        n = (uint16_t)strlen(w);
        for (i = 0; i < n && i < len && lower(line[i]) == w[i]; i++) {}
        if (i == n) {
            if ((uint16_t)(s->hdr_len + len + 2) <= NIO_HDR_BYTES) {
                memcpy(s->hdr + s->hdr_len, line, len);
                s->hdr[s->hdr_len + len] = '\r';
                s->hdr[s->hdr_len + len + 1] = '\n';
                s->hdr_len = (uint16_t)(s->hdr_len + len + 2);
            }
            break;
        }
        n++;   /* past the name's terminator */
    }
}

/* Reads and parses the reply's status line and headers; body bytes that came
 * with them stay in rx. NotReady until the blank line has arrived. */
static uint8_t http_headers(NioSession *s, uint16_t wait_ms)
{
    int n;
    uint16_t i, start, len;

    while (!s->headers_done) {
        if (s->failed) return NIO_ST_IO_ERROR;
        if (s->peer_closed) return NIO_ST_IO_ERROR;   /* closed before the headers ended */
        n = nio_sock_recv(s->net, s->sock, s->rx + s->rx_len, (uint16_t)(NIO_RX_BYTES - s->rx_len),
                          wait_ms);
        if (n == NIO_SOCK_WOULDBLOCK) return NIO_ST_NOT_READY;
        if (n < 0) {
            s->failed = 1;
            return NIO_ST_IO_ERROR;
        }
        s->net->bytesIn += (uint32_t)n;
        if (n == 0) s->peer_closed = 1;
        s->rx_len = (uint16_t)(s->rx_len + n);
        start = 0;
        for (i = 0; i < s->rx_len && !s->headers_done; i++) {
            if (s->rx[i] != '\n') continue;
            len = (uint16_t)(i - start);
            if (len && s->rx[i - 1] == '\r') len--;
            if (s->skip_line) s->skip_line = 0;   /* the tail of an over-long line */
            else http_line(s, (const char *)s->rx + start, len);
            start = (uint16_t)(i + 1);
        }
        if (start == 0 && s->rx_len == NIO_RX_BYTES) {
            /* A line longer than the buffer: drop it. */
            s->skip_line = 1;
            start = s->rx_len;
        }
        memmove(s->rx, s->rx + start, (uint16_t)(s->rx_len - start));
        s->rx_len = (uint16_t)(s->rx_len - start);
    }
    return NIO_ST_OK;
}

static uint8_t open_cmd(NioState *nio, const uint8_t *in, uint16_t len, uint8_t *out,
                        uint16_t cap, uint16_t *out_len)
{
    char host[HOST_MAX];
    uint8_t method, flags, http, i, status, has_type = 0, profile = 0;
    uint16_t url_len, port, path, at, count, klen, vlen, wlen = 0, c;
    uint32_t body = 0, ext;
    const char *url;
    const uint8_t *headers;
    NioSession *s = 0;

    if (len < 5 || cap < 7) return NIO_ST_INVALID;
    method = in[1];
    flags = in[2];
    url_len = NIO_GET16(in + 3);
    if ((uint32_t)5 + url_len + 2 > len) return NIO_ST_INVALID;
    url = (const char *)in + 5;
    if (flags & OPEN_FLAG_TLS) return NIO_ST_UNSUPPORTED;
    if (url_len > 8 && memcmp(url, "https://", 8) == 0) return NIO_ST_UNSUPPORTED;
    if (!split_url(url, url_len, &http, host, &port, &path)) return NIO_ST_INVALID;
    if (http && (method < 1 || method > 5)) return NIO_ST_INVALID;
    if (bad_text((const uint8_t *)url, url_len, 0)) return NIO_ST_INVALID;

    for (i = 0; i < NIO_SESSIONS; i++)
        if (!nio->sessions[i].used) {
            s = &nio->sessions[i];
            break;
        }
    if (!s) return NIO_ST_BUSY;

    /* Request headers, then the body length, then the response header names. */
    at = (uint16_t)(5 + url_len);
    count = NIO_GET16(in + at);
    at = (uint16_t)(at + 2);
    headers = in + at;
    for (c = 0; c < count; c++) {
        if ((uint32_t)at + 2 > len) return NIO_ST_INVALID;
        klen = NIO_GET16(in + at);
        if ((uint32_t)at + 2 + klen + 2 > len) return NIO_ST_INVALID;
        if (!klen || bad_text(in + at + 2, klen, 1)) return NIO_ST_INVALID;
        if (klen == 12) {
            for (i = 0; i < 12 && lower((char)in[at + 2 + i]) == "content-type"[i]; i++) {}
            if (i == 12) has_type = 1;
        }
        vlen = NIO_GET16(in + at + 2 + klen);
        if ((uint32_t)at + 4 + klen + vlen > len || bad_text(in + at + 4 + klen, vlen, 2)) return NIO_ST_INVALID;
        at = (uint16_t)(at + 4 + klen + vlen);
    }
    memset(s, 0, sizeof(*s));
    if ((uint32_t)at + 4 <= len) {
        body = NIO_GET32(in + at);
        at = (uint16_t)(at + 4);
    }
    if ((uint32_t)at + 2 <= len) {
        c = NIO_GET16(in + at);
        at = (uint16_t)(at + 2);
        while (c--) {
            if ((uint32_t)at + 2 > len) return NIO_ST_INVALID;
            klen = NIO_GET16(in + at);
            if ((uint32_t)at + 2 + klen > len) return NIO_ST_INVALID;
            /* Names that do not fit are not captured. */
            if (klen && (uint16_t)(wlen + klen + 3) <= NIO_WANT_BYTES) {
                for (i = 0; i < klen; i++) s->want[wlen++] = lower((char)in[at + 2 + i]);
                s->want[wlen++] = ':';
                s->want[wlen++] = 0;
            }
            at = (uint16_t)(at + 2 + klen);
        }
    }
    if ((uint32_t)at + 4 <= len) {
        ext = NIO_GET32(in + at);
        at = (uint16_t)(at + 4);
        if (ext & 1) {
            if ((uint32_t)at + 4 > len) return NIO_ST_INVALID;
            if (in[at] != 0) return NIO_ST_UNSUPPORTED;   /* content translation */
            at = (uint16_t)(at + 4 + NIO_GET16(in + at + 2));
        }
        if ((ext & 2) && at < len) profile = in[at];
    }

    status = nio_net_up(nio);
    if (status != NIO_ST_OK) return status;
    if (nio_sock_tcp_connect(&nio->net, host, port, &s->sock) != 0) {
        nio->net.failures++;
        return NIO_ST_IO_ERROR;
    }
    nio->net.connects++;
    s->net = &nio->net;
    s->used = 1;
    s->http = http;
    s->handle = nio->next_handle++;
    if (!nio->next_handle) nio->next_handle = 1;

    if (http) {
        s->head_only = (uint8_t)(method == 5);
        puts_(s, method_name(method));
        puts_(s, " ");
        if (path >= url_len || url[path] != '/') puts_(s, "/");
        put(s, url + path, (uint16_t)(url_len - path));
        puts_(s, k_http_host);
        puts_(s, host);
        if (port != 80) {
            puts_(s, ":");
            put_number(s, port);
        }
        puts_(s, k_http_agent);
        at = 0;
        for (c = 0; c < count; c++) {
            klen = NIO_GET16(headers + at);
            vlen = NIO_GET16(headers + at + 2 + klen);
            put(s, (const char *)headers + at + 2, klen);
            puts_(s, ": ");
            put(s, (const char *)headers + at + 4 + klen, vlen);
            puts_(s, k_crlf);
            at = (uint16_t)(at + 4 + klen + vlen);
        }
        if (!has_type && profile >= 1 && profile <= 3) {
            puts_(s, "Content-Type: ");
            puts_(s, profile == 1 ? k_type_json : profile == 2 ? k_type_form : k_type_text);
            puts_(s, k_crlf);
        }
        if ((method == 2 || method == 3) && !body && (flags & OPEN_FLAG_BODY_UNKNOWN)) {
            /* The length and the blank line follow at the commit. */
            s->body_open = 1;
        } else {
            if (method == 2 || method == 3) {
                puts_(s, "Content-Length: ");
                put_number(s, body);
                puts_(s, k_crlf);
                s->body_expected = body;
            }
            puts_(s, k_crlf);
        }
        if (s->rx_len) send_all(s, s->rx, s->rx_len);
        s->rx_len = 0;
        if (s->failed) {
            session_close(s);
            return NIO_ST_IO_ERROR;
        }
    }

    out[0] = 1;
    out[1] = (uint8_t)(0x01 | (s->body_expected || s->body_open ? 0x02 : 0));
    out[2] = out[3] = 0;
    nio_put16(out + 4, s->handle);
    out[6] = (uint8_t)(http ? 0x00 : 0x07);   /* tcp: sequential, streaming */
    *out_len = 7;
    return NIO_ST_OK;
}

static uint8_t read_cmd(NioState *nio, const uint8_t *in, uint16_t len, uint8_t *out,
                        uint16_t cap, uint16_t *out_len)
{
    NioSession *s;
    uint32_t offset, left;
    uint16_t max, room, got = 0;
    uint8_t flags = 0, status;
    int n;

    if (len < 9 || cap < 12) return NIO_ST_INVALID;
    s = find(nio, NIO_GET16(in + 1));
    if (!s) return NIO_ST_INVALID;
    offset = NIO_GET32(in + 3);
    max = NIO_GET16(in + 7);
    if (max > (uint16_t)(cap - 12)) max = (uint16_t)(cap - 12);
    room = max;                         /* what the caller can take: Truncated means it was filled */
    if (offset != s->read_cursor) return NIO_ST_INVALID;
    if (s->http) {
        if (s->body_expected || s->body_open) return NIO_ST_NOT_READY;   /* body not written yet */
        status = http_headers(s, HTTP_WAIT_MS);
        if (status != NIO_ST_OK) return status;
        if (s->has_length) {
            left = s->content_length - s->read_cursor;
            if (left < max) max = (uint16_t)left;
        }
        if (s->head_only) max = 0;
    }
    if (s->rx_len && max) {
        got = s->rx_len < max ? s->rx_len : max;
        memcpy(out + 12, s->rx, got);
        memmove(s->rx, s->rx + got, (uint16_t)(s->rx_len - got));
        s->rx_len = (uint16_t)(s->rx_len - got);
    } else if (max && !s->peer_closed) {
        if (s->failed) return NIO_ST_IO_ERROR;
        n = nio_sock_recv(s->net, s->sock, out + 12, max, (uint16_t)(s->http ? HTTP_WAIT_MS : 0));
        if (n == NIO_SOCK_WOULDBLOCK) return NIO_ST_NOT_READY;
        if (n < 0) {
            s->failed = 1;
            return NIO_ST_IO_ERROR;
        }
        s->net->bytesIn += (uint32_t)n;
        if (n == 0) s->peer_closed = 1;
        got = (uint16_t)n;
    }
    s->read_cursor += got;
    /* A body shorter than its Content-Length is an error, not an end: what did arrive is delivered first, then the
     * Read that finds nothing left fails. */
    left = (s->http && s->has_length && !s->head_only && s->read_cursor < s->content_length);
    if (left && !got && s->peer_closed && !s->rx_len) return NIO_ST_IO_ERROR;
    if (s->head_only || (s->http && s->has_length ? !left : (s->peer_closed && !s->rx_len)))
        flags |= READ_EOF;
    if (got && got == room) flags |= READ_TRUNCATED;
    out[0] = 1;
    out[1] = flags;
    out[2] = out[3] = 0;
    nio_put16(out + 4, s->handle);
    nio_put32(out + 6, offset);
    nio_put16(out + 10, got);
    *out_len = (uint16_t)(12 + got);
    return NIO_ST_OK;
}

static uint8_t write_cmd(NioState *nio, const uint8_t *in, uint16_t len, uint8_t *out,
                         uint16_t cap, uint16_t *out_len)
{
    NioSession *s;
    uint32_t offset;
    uint16_t n;

    if (len < 9 || cap < 12) return NIO_ST_INVALID;
    s = find(nio, NIO_GET16(in + 1));
    if (!s) return NIO_ST_INVALID;
    offset = NIO_GET32(in + 3);
    n = NIO_GET16(in + 7);
    if ((uint32_t)9 + n > len || offset != s->write_cursor) return NIO_ST_INVALID;
    if (s->failed || s->peer_closed) return NIO_ST_IO_ERROR;
    if (s->body_open) {
        if (n) {
            if (n > (uint16_t)(NIO_RX_BYTES - s->rx_len)) return NIO_ST_INVALID;
            memcpy(s->rx + s->rx_len, in + 9, n);
            s->rx_len = (uint16_t)(s->rx_len + n);
        } else {
            /* The commit: the length, the blank line and the gathered body. */
            static const char kLength[] = "Content-Length: ";
            char digits[5];
            uint8_t d = 0;
            uint16_t v = s->rx_len;

            do {
                digits[4 - d++] = (char)('0' + v % 10);
                v /= 10;
            } while (v);
            if (!send_all(s, (const uint8_t *)kLength, 16) ||
                !send_all(s, (const uint8_t *)digits + 5 - d, d) ||
                !send_all(s, (const uint8_t *)k_crlf2, 4) ||
                (s->rx_len && !send_all(s, s->rx, s->rx_len)))
                return NIO_ST_IO_ERROR;
            s->rx_len = 0;
            s->body_open = 0;
        }
    } else {
        if (s->http && n > s->body_expected) return NIO_ST_INVALID;
        if (n && !send_all(s, in + 9, n)) return NIO_ST_IO_ERROR;
        if (s->http) s->body_expected -= n;
    }
    s->write_cursor += n;
    out[0] = 1;
    out[1] = out[2] = out[3] = 0;
    nio_put16(out + 4, s->handle);
    nio_put32(out + 6, offset);
    nio_put16(out + 10, n);
    *out_len = 12;
    return NIO_ST_OK;
}

static uint8_t info_cmd(NioState *nio, const uint8_t *in, uint16_t len, uint8_t *out,
                        uint16_t cap, uint16_t *out_len)
{
    NioSession *s;
    uint8_t flags = 0, status;
    uint16_t hdr = 0;

    if (len < 3 || cap < 18) return NIO_ST_INVALID;
    s = find(nio, NIO_GET16(in + 1));
    if (!s) return NIO_ST_INVALID;
    memset(out, 0, 18);
    if (s->http) {
        if (s->body_expected || s->body_open) return NIO_ST_NOT_READY;
        status = http_headers(s, HTTP_WAIT_MS);
        if (status != NIO_ST_OK) return status;
        flags = INFO_STATUS;
        nio_put16(out + 6, s->http_status);
        if (s->has_length) {
            flags |= INFO_LENGTH;
            nio_put32(out + 8, s->content_length);
        }
        hdr = s->hdr_len;
        if (hdr > (uint16_t)(cap - 18)) hdr = (uint16_t)(cap - 18);
        if (hdr) flags |= INFO_HEADERS;
        memcpy(out + 18, s->hdr, hdr);
    }
    out[0] = 1;
    out[1] = flags;
    nio_put16(out + 4, s->handle);
    nio_put16(out + 16, hdr);
    *out_len = (uint16_t)(18 + hdr);
    return NIO_ST_OK;
}

uint8_t nio_network(NioState *nio, uint8_t cmd, const uint8_t *in, uint16_t in_len,
                    uint8_t *out, uint16_t cap, uint16_t *out_len)
{
    NioSession *s;

    if (in_len < 1 || in[0] != 1) return NIO_ST_INVALID;
    switch (cmd) {
    case CMD_OPEN:  return open_cmd(nio, in, in_len, out, cap, out_len);
    case CMD_READ:  return read_cmd(nio, in, in_len, out, cap, out_len);
    case CMD_WRITE: return write_cmd(nio, in, in_len, out, cap, out_len);
    case CMD_INFO:  return info_cmd(nio, in, in_len, out, cap, out_len);
    case CMD_CLOSE:
        if (in_len < 3 || cap < 6) return NIO_ST_INVALID;
        s = find(nio, NIO_GET16(in + 1));
        if (!s) return NIO_ST_INVALID;
        session_close(s);
        out[0] = 1;
        out[1] = out[2] = out[3] = 0;
        out[4] = in[1];
        out[5] = in[2];
        *out_len = 6;
        return NIO_ST_OK;
    default:
        return NIO_ST_UNSUPPORTED;
    }
}
