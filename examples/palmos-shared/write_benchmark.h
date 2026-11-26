#ifndef NIO_WRITE_BENCHMARK_H
#define NIO_WRITE_BENCHMARK_H
#include "NioLibClient.h"
/* All measurements are app-owned; no transport instrumentation. */
typedef struct {
    UInt32 bytes, ticks, rate, verifyBytes, verifyTicks, checksum;
    UInt32 initTicks, cleanupTicks, failedOffset, started;
    UInt16 calls, verifyCalls, errors, result, palm;
    UInt16 cleanupResult, cleanupPalm, stage;
} NioTestWriteResult;
extern NioTestWriteResult nio_write_benchmark;
#define nioWriteIdle 0
#define nioWriteInit 1
#define nioWriteError 2
#define nioWriteShort 3
#define nioVerifyRead 4
#define nioVerifyShort 5
#define nioVerifyPattern 6
#define nioVerifySize 7
#define nioWriteDone 8
void NioTestWriteBenchmark(const Char *uri);
#endif
