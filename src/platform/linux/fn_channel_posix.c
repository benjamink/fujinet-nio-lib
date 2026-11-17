/*
 * fn_channel_posix.c - POSIX serial/PTY byte channel.
 *
 * This file owns termios/open/select/read/write handling only. The common stream
 * transport owns SLIP framing and FujiBus request/response exchange.
 */

#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "fujinet-nio.h"
#include "fn_channel.h"

#define DEFAULT_PORT "/dev/ttyUSB0"
#define DEFAULT_BAUD 115200

static int _fd = -1;
static struct termios _saved_termios;

static speed_t get_baud(int baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    default:
        return B115200;
    }
}

static void timeout_to_timeval(uint16_t timeout_ms, struct timeval *tv)
{
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (long)(timeout_ms % 1000) * 1000L;
}

uint8_t fn_channel_init(void)
{
    const char *port;
    const char *baud_str;
    int baud;
    struct termios tio;

    if (_fd >= 0) {
        return FN_OK;
    }

    port = getenv("FN_PORT");
    if (port == NULL || port[0] == '\0') {
        port = DEFAULT_PORT;
    }

    baud_str = getenv("FN_BAUD");
    if (baud_str != NULL && baud_str[0] != '\0') {
        baud = atoi(baud_str);
        if (baud <= 0) {
            baud = DEFAULT_BAUD;
        }
    } else {
        baud = DEFAULT_BAUD;
    }

    _fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (_fd < 0) {
        fprintf(stderr, "fn_channel_posix: cannot open %s: %s\n", port, strerror(errno));
        return FN_ERR_NOT_FOUND;
    }

    if (tcgetattr(_fd, &_saved_termios) < 0) {
        fprintf(stderr, "fn_channel_posix: tcgetattr failed: %s\n", strerror(errno));
        close(_fd);
        _fd = -1;
        return FN_ERR_IO;
    }

    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;

    cfsetispeed(&tio, get_baud(baud));
    cfsetospeed(&tio, get_baud(baud));

    if (tcsetattr(_fd, TCSANOW, &tio) < 0) {
        fprintf(stderr, "fn_channel_posix: tcsetattr failed: %s\n", strerror(errno));
        close(_fd);
        _fd = -1;
        return FN_ERR_IO;
    }

    tcflush(_fd, TCIOFLUSH);
    return FN_OK;
}

uint8_t fn_channel_ready(void)
{
    return _fd >= 0;
}

/* Submit a complete encoded frame instead of exhausting CDC ACM's write
 * buffers with one URB per byte. Partial writes advance, never replay bytes.
 * Use one monotonic deadline, including EINTR and repeated readiness wakes. */
uint8_t fn_channel_write_bytes(const uint8_t *data, uint16_t length,
                               uint16_t timeout_ms)
{
    struct timespec now;
    uint64_t deadline, ms;
    uint16_t sent = 0;
    int first = 1;

    if (_fd < 0) return FN_ERR_NOT_READY;
    if (length && data == NULL) return FN_ERR_INVALID;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return FN_ERR_IO;
    deadline = (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000 + timeout_ms;
    while (sent < length) {
        ssize_t n;
        fd_set write_fds;
        struct timeval tv;
        int ret;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return FN_ERR_IO;
        ms = (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
        if (!first && ms >= deadline) return FN_ERR_TIMEOUT;
        first = 0;
        n = write(_fd, data + sent, length - sent);
        if (n > 0) {
            sent += (uint16_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return FN_ERR_IO;
        if (ms >= deadline) return FN_ERR_TIMEOUT;

        /* Recheck the nonblocking write even if a driver readiness wake was
         * missed; do not spend the whole frame deadline waiting for one wake. */
        ms = deadline - ms;
        if (ms > 10) ms = 10;
        FD_ZERO(&write_fds);
        FD_SET(_fd, &write_fds);
        timeout_to_timeval((uint16_t)ms, &tv);
        ret = select(_fd + 1, NULL, &write_fds, NULL, &tv);
        if (ret < 0 && errno != EINTR) return FN_ERR_IO;
    }
    return FN_OK;
}

uint8_t fn_channel_write_byte(uint8_t value, uint16_t timeout_ms)
{
    return fn_channel_write_bytes(&value, 1, timeout_ms);
}

uint8_t fn_channel_read_byte(uint8_t *value, uint16_t timeout_ms)
{
    fd_set read_fds;
    struct timeval tv;
    ssize_t n;
    int ret;

    if (_fd < 0 || value == NULL) {
        return 0;
    }

    FD_ZERO(&read_fds);
    FD_SET(_fd, &read_fds);
    timeout_to_timeval(timeout_ms, &tv);
    ret = select(_fd + 1, &read_fds, NULL, NULL, &tv);
    if (ret <= 0) {
        return 0;
    }

    n = read(_fd, value, 1);
    if (n == 1) {
        return 1;
    }

    return 0;
}

void fn_channel_drain_rx(void)
{
    if (_fd >= 0) {
        tcflush(_fd, TCIFLUSH);
    }
}

void fn_channel_close(void)
{
    if (_fd >= 0) {
        tcsetattr(_fd, TCSANOW, &_saved_termios);
        close(_fd);
        _fd = -1;
    }
}

const char *fn_platform_name(void)
{
    return "linux";
}
