/*
 * Performance benchmarks for libc4
 * Compares performance metrics with Go implementation reference
 */

#include <c4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ITERATIONS 10000
#define BUFFER_SIZE 1024

/* Timer utilities */
typedef struct {
    clock_t start;
    clock_t end;
} timer_t;

static void timer_start(timer_t* t) {
    t->start = clock();
}

static double timer_stop(timer_t* t) {
    t->end = clock();
    return ((double)(t->end - t->start)) / CLOCKS_PER_SEC;
}

/* Benchmark: ID parsing */
static void bench_id_parse(void) {
    const char* test_id = "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdCbYyRZx4yYnBNkbPYFmX8gvgJE2k34N5ADC7H5CCa";
    timer_t timer;
    int i;

    printf("Benchmarking c4id_parse() with %d iterations...\n", ITERATIONS);

    timer_start(&timer);
    for (i = 0; i < ITERATIONS; i++) {
        c4_id_t* id = c4id_parse(test_id);
        c4id_free(id);
    }
    double elapsed = timer_stop(&timer);

    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Ops/sec: %.0f\n", ITERATIONS / elapsed);
    printf("  Time/op: %.3f µs\n", (elapsed / ITERATIONS) * 1000000);
    printf("\n");
}

/* Benchmark: ID string conversion */
static void bench_id_string(void) {
    const char* test_id = "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdCbYyRZx4yYnBNkbPYFmX8gvgJE2k34N5ADC7H5CCa";
    c4_id_t* id = c4id_parse(test_id);
    timer_t timer;
    int i;

    printf("Benchmarking c4id_string() with %d iterations...\n", ITERATIONS);

    timer_start(&timer);
    for (i = 0; i < ITERATIONS; i++) {
        char* str = c4id_string(id);
        free(str);
    }
    double elapsed = timer_stop(&timer);

    c4id_free(id);

    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Ops/sec: %.0f\n", ITERATIONS / elapsed);
    printf("  Time/op: %.3f µs\n", (elapsed / ITERATIONS) * 1000000);
    printf("\n");
}

/* Benchmark: Encoder with small data */
static void bench_encoder_small(void) {
    const char* data = "Hello, World!";
    size_t data_len = strlen(data);
    timer_t timer;
    int i;

    printf("Benchmarking c4id_encoder (small data: %zu bytes) with %d iterations...\n",
           data_len, ITERATIONS);

    timer_start(&timer);
    for (i = 0; i < ITERATIONS; i++) {
        c4id_encoder_t* enc = c4id_encoder_new();
        c4id_encoder_write(enc, data, data_len);
        c4_id_t* id = c4id_encoder_id(enc);
        c4id_free(id);
        c4id_encoder_free(enc);
    }
    double elapsed = timer_stop(&timer);

    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Ops/sec: %.0f\n", ITERATIONS / elapsed);
    printf("  Time/op: %.3f µs\n", (elapsed / ITERATIONS) * 1000000);
    printf("  Throughput: %.2f MB/s\n",
           (data_len * ITERATIONS / (1024.0 * 1024.0)) / elapsed);
    printf("\n");
}

/* Benchmark: Encoder with medium data */
static void bench_encoder_medium(void) {
    unsigned char buffer[BUFFER_SIZE];
    timer_t timer;
    int i, j;

    /* Fill buffer with test data */
    for (j = 0; j < BUFFER_SIZE; j++) {
        buffer[j] = (unsigned char)(j % 256);
    }

    printf("Benchmarking c4id_encoder (medium data: %d bytes) with %d iterations...\n",
           BUFFER_SIZE, ITERATIONS);

    timer_start(&timer);
    for (i = 0; i < ITERATIONS; i++) {
        c4id_encoder_t* enc = c4id_encoder_new();
        c4id_encoder_write(enc, buffer, BUFFER_SIZE);
        c4_id_t* id = c4id_encoder_id(enc);
        c4id_free(id);
        c4id_encoder_free(enc);
    }
    double elapsed = timer_stop(&timer);

    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Ops/sec: %.0f\n", ITERATIONS / elapsed);
    printf("  Time/op: %.3f µs\n", (elapsed / ITERATIONS) * 1000000);
    printf("  Throughput: %.2f MB/s\n",
           (BUFFER_SIZE * ITERATIONS / (1024.0 * 1024.0)) / elapsed);
    printf("\n");
}

/* Benchmark: Encoder reset and reuse */
static void bench_encoder_reuse(void) {
    const char* data = "Test data for reuse";
    size_t data_len = strlen(data);
    timer_t timer;
    int i;

    printf("Benchmarking c4id_encoder_reset() with %d iterations...\n", ITERATIONS);

    c4id_encoder_t* enc = c4id_encoder_new();

    timer_start(&timer);
    for (i = 0; i < ITERATIONS; i++) {
        c4id_encoder_write(enc, data, data_len);
        c4_id_t* id = c4id_encoder_id(enc);
        c4id_free(id);
        c4id_encoder_reset(enc);
    }
    double elapsed = timer_stop(&timer);

    c4id_encoder_free(enc);

    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Ops/sec: %.0f\n", ITERATIONS / elapsed);
    printf("  Time/op: %.3f µs\n", (elapsed / ITERATIONS) * 1000000);
    printf("\n");
}

/* Benchmark: Digest operations */
static void bench_digest_sum(void) {
    unsigned char data1[64] = {0};
    unsigned char data2[64] = {0};
    timer_t timer;
    int i, j;

    /* Fill with test data */
    for (j = 0; j < 64; j++) {
        data1[j] = (unsigned char)j;
        data2[j] = (unsigned char)(j + 64);
    }

    printf("Benchmarking c4id_digest_sum() with %d iterations...\n", ITERATIONS);

    c4_digest_t* d1 = c4id_digest_new(data1, 64);
    c4_digest_t* d2 = c4id_digest_new(data2, 64);

    timer_start(&timer);
    for (i = 0; i < ITERATIONS; i++) {
        c4_digest_t* sum = c4id_digest_sum(d1, d2);
        c4id_digest_free(sum);
    }
    double elapsed = timer_stop(&timer);

    c4id_digest_free(d1);
    c4id_digest_free(d2);

    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Ops/sec: %.0f\n", ITERATIONS / elapsed);
    printf("  Time/op: %.3f µs\n", (elapsed / ITERATIONS) * 1000000);
    printf("\n");
}

/* Benchmark: ID comparison */
static void bench_id_cmp(void) {
    const char* id1_str = "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdCbYyRZx4yYnBNkbPYFmX8gvgJE2k34N5ADC7H5CCa";
    const char* id2_str = "c45XWoriYjdEAdiVdVHWCKPPvpKdwbLG3EyyxQkSGbikpMDVDf84wWMg1xPSS4AXQR4Qa64WFkH3E7nJP8M35LnBEmX";
    c4_id_t* id1 = c4id_parse(id1_str);
    c4_id_t* id2 = c4id_parse(id2_str);
    timer_t timer;
    int i;

    printf("Benchmarking c4id_cmp() with %d iterations...\n", ITERATIONS);

    timer_start(&timer);
    for (i = 0; i < ITERATIONS; i++) {
        (void)c4id_cmp(id1, id2);
    }
    double elapsed = timer_stop(&timer);

    c4id_free(id1);
    c4id_free(id2);

    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Ops/sec: %.0f\n", ITERATIONS / elapsed);
    printf("  Time/op: %.3f µs\n", (elapsed / ITERATIONS) * 1000000);
    printf("\n");
}

int main(void) {
    printf("===========================================\n");
    printf("libc4 Performance Benchmarks\n");
    printf("===========================================\n\n");

    c4_init();

    bench_id_parse();
    bench_id_string();
    bench_encoder_small();
    bench_encoder_medium();
    bench_encoder_reuse();
    bench_digest_sum();
    bench_id_cmp();

    printf("===========================================\n");
    printf("Benchmark suite completed\n");
    printf("===========================================\n");
    printf("\nNote: For comparison with Go implementation:\n");
    printf("  Run: go test -bench=. github.com/Avalanche-io/c4\n");
    printf("\n");

    return 0;
}
