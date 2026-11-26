/* 64 KiB / 512-byte writes, followed by separately timed readback.
 * Offset zero overwrites the selected test file. No application retry.
 * Two app-owned stack buffers (1280 bytes); no extra library allocation. */
#include "write_benchmark.h"
NioTestWriteResult nio_write_benchmark;

void NioTestWriteBenchmark(const Char *uri)
{
    UInt8 scratch[768], data[512], expected;
    UInt16 i, count;
    fn_file_io_t io;
    fn_file_read_t read;
    fn_file_write_t written;
    UInt32 start, offset;
    NioLibCleanup cleanup;
    io.buffer = scratch; io.capacity = sizeof(scratch);
    MemSet(&nio_write_benchmark, sizeof(nio_write_benchmark), 0);
    start = TimGetTicks();
    nio_write_benchmark.started = start;
    nio_write_benchmark.result = fn_init();
    nio_write_benchmark.palm = NioLibClientLastError();
    nio_write_benchmark.initTicks = TimGetTicks() - start;
    if (nio_write_benchmark.result) nio_write_benchmark.stage = nioWriteInit;
    start = TimGetTicks();
    while (!nio_write_benchmark.result && nio_write_benchmark.bytes < 65536UL) {
        offset = nio_write_benchmark.bytes;
        nio_write_benchmark.failedOffset = offset;
        for (i = 0; i < sizeof(data); ++i)
            data[i] = (UInt8)((offset + i) ^ ((offset + i) >> 8));
        count = 0;
        ++nio_write_benchmark.calls;
        nio_write_benchmark.result = fn_file_write(&io, uri, offset, data,
                                                    sizeof(data), &written);
        nio_write_benchmark.palm = NioLibClientLastError();
        if (nio_write_benchmark.result) {
            nio_write_benchmark.stage = nioWriteError;
            break;
        }
        count = written.bytes_written;
        /* Publish positively acknowledged partial progress, then stop. */
        nio_write_benchmark.bytes += count;
        if (count != sizeof(data)) {
            nio_write_benchmark.result = FN_ERR_IO;
            nio_write_benchmark.stage = nioWriteShort;
            break;
        }
    }
    nio_write_benchmark.ticks = TimGetTicks() - start;
    if (nio_write_benchmark.ticks)
        nio_write_benchmark.rate = nio_write_benchmark.bytes * SysTicksPerSecond() /
                                  nio_write_benchmark.ticks;
    if (!nio_write_benchmark.result) {
        start = TimGetTicks();
        while (nio_write_benchmark.verifyBytes < 65536UL) {
            offset = nio_write_benchmark.verifyBytes;
            nio_write_benchmark.failedOffset = offset;
            ++nio_write_benchmark.verifyCalls;
            nio_write_benchmark.result = fn_file_read(&io, uri, offset, data,
                                                       sizeof(data), &read);
            nio_write_benchmark.palm = NioLibClientLastError();
            if (nio_write_benchmark.result) {
                nio_write_benchmark.stage = nioVerifyRead;
                break;
            }
            count = read.bytes_read;
            if (count != sizeof(data)) {
                nio_write_benchmark.result = FN_ERR_IO;
                nio_write_benchmark.stage = nioVerifyShort;
                break;
            }
            for (i = 0; i < sizeof(data); ++i) {
                expected = (UInt8)((offset + i) ^ ((offset + i) >> 8));
                if (data[i] != expected) {
                    nio_write_benchmark.result = FN_ERR_IO;
                    nio_write_benchmark.stage = nioVerifyPattern;
                    break;
                }
                nio_write_benchmark.checksum += data[i];
            }
            if (nio_write_benchmark.result) break;
            nio_write_benchmark.verifyBytes += count;
        }
        if (!nio_write_benchmark.result) {
            /* Extra read outside write timing: reject an unexpected tail. */
            nio_write_benchmark.failedOffset = 65536UL;
            ++nio_write_benchmark.verifyCalls;
            nio_write_benchmark.result = fn_file_read(&io, uri, 65536UL, data,
                                                       1, &read);
            nio_write_benchmark.palm = NioLibClientLastError();
            if (nio_write_benchmark.result) nio_write_benchmark.stage = nioVerifyRead;
            else if (read.bytes_read) {
                nio_write_benchmark.result = FN_ERR_IO;
                nio_write_benchmark.stage = nioVerifySize;
            } else {
                nio_write_benchmark.stage = nioWriteDone;
                nio_write_benchmark.failedOffset = 0;
            }
        }
        nio_write_benchmark.verifyTicks = TimGetTicks() - start;
    }
    nio_write_benchmark.errors = nio_write_benchmark.result ? 1 : 0;
    start = TimGetTicks();
    fn_shutdown();
    nio_write_benchmark.cleanupResult = NioLibClientLastResult();
    nio_write_benchmark.cleanupPalm = NioLibClientLastError();
    nio_write_benchmark.cleanupTicks = TimGetTicks() - start;
    NioLibClientCleanupReport(&cleanup);
    if (!nio_write_benchmark.cleanupResult &&
        (cleanup.uncertain || cleanup.ambiguousOperation))
        nio_write_benchmark.cleanupResult = FN_ERR_TIMEOUT;
}
