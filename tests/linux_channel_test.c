#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include "../src/platform/linux/fn_channel_posix.c"
#include "../src/common/fn_transport_stream.c"
#include "../src/common/fn_session.c"

fn_transport_ctx_t _fn_transport_ctx;
static unsigned ticks, calls, waits, mode;
static uint8_t captured[128];
static size_t captured_size;

int __wrap_clock_gettime(clockid_t id, struct timespec *t)
{
    assert(id == CLOCK_MONOTONIC);
    t->tv_sec = ticks / 1000;
    t->tv_nsec = (ticks % 1000) * 1000000;
    return 0;
}
ssize_t __wrap_write(int fd, const void *data, size_t length)
{
    size_t count = length;
    assert(fd == 3);
    ++calls;
    if (mode == 1) {
        if (calls == 2) { errno = EAGAIN; return -1; }
        if (calls == 3) { ++ticks; errno = EINTR; return -1; }
        if (count > 3) count = 3;
    }
    if (mode == 2) { errno = EAGAIN; return -1; }
    if (mode == 3) { ++ticks; errno = EINTR; return -1; }
    if (mode == 4) { errno = EIO; return -1; }
    assert(captured_size + count <= sizeof captured);
    memcpy(captured + captured_size, data, count);
    captured_size += count;
    return count;
}
int __wrap_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv)
{
    (void)n; (void)r; (void)w; (void)e;
    ++waits;
    ticks += tv->tv_sec * 1000 + tv->tv_usec / 1000;
    return 0; /* No readiness wake: writer must recheck within its deadline. */
}
static void reset(unsigned m)
{
    mode=m; ticks=calls=waits=0; captured_size=0; _fd=3;
}
int main(void)
{
    const uint8_t data[] = {1,2,3,4,5,6,7,8};
    const uint8_t packet[] = {0xfd, 3, 0xc0, 0xdb, 6};
    const uint8_t encoded[] = {0xc0,0xfd,3,0xdb,0xdc,0xdb,0xdd,6,0xc0};
    reset(0);
    assert(fn_transport_init() == FN_OK);
    assert(_stream_session.ops->write_bytes != NULL);
    assert(write_frame(&_stream_session, packet, sizeof packet) == FN_OK);
    assert(calls == 1 && captured_size == sizeof encoded);
    assert(memcmp(captured, encoded, sizeof encoded) == 0);

    reset(1);
    assert(fn_channel_write_bytes(data, sizeof data, 100) == FN_OK);
    assert(captured_size == sizeof data && memcmp(data,captured,sizeof data)==0);
    assert(waits == 1);

    reset(2);
    assert(fn_channel_write_bytes(data, sizeof data, 25) == FN_ERR_TIMEOUT);
    assert(ticks == 25 && waits == 3 && captured_size == 0);
    reset(3);
    assert(fn_channel_write_bytes(data, sizeof data, 25) == FN_ERR_TIMEOUT);
    assert(ticks == 25);
    reset(4);
    assert(fn_channel_write_bytes(data, sizeof data, 25) == FN_ERR_IO);
    reset(0);
    assert(fn_channel_write_bytes(data, sizeof data, 0) == FN_OK);
    assert(fn_channel_write_bytes(NULL, 1, 1) == FN_ERR_INVALID);
    _fd=-1;
    assert(fn_channel_write_bytes(data, sizeof data, 1) == FN_ERR_NOT_READY);
    puts("Linux bulk channel: framing, partial writes, backpressure, EINTR, deadlines OK");
    return 0;
}
