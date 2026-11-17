#ifndef NIO_TEST_BENCHMARK_H
#define NIO_TEST_BENCHMARK_H
#include "NioLibClient.h"
/* App-owned measurements; no library instrumentation/public ABI is added. */
typedef struct {
    UInt32 bytes, ticks, initTicks, cleanupTicks, checksum, rate;
    UInt16 calls, errors, result, palm, cleanupResult, cleanupPalm;
} NioTestBenchmarkResult;
extern NioTestBenchmarkResult nio_benchmark;
extern UInt32 nio_benchmark_started;
#define nioBenchmarkInit 1
#define nioBenchmarkRead 2
#define nioBenchmarkShort 3
#define nioBenchmarkPattern 4
extern UInt16 nio_benchmark_failure;
void NioTestBenchmark(const Char *uri);
#endif
