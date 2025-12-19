/*
 * json-asm: Benchmark tool
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include "json_asm.h"

/* Default iterations */
#define DEFAULT_ITERATIONS 1000
#define WARMUP_ITERATIONS 10

/* Get high-resolution time in nanoseconds */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Load file into memory */
static char *load_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fclose(f);
        fprintf(stderr, "Error: Empty or invalid file: %s\n", path);
        return NULL;
    }

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);

    if (read != (size_t)len) {
        free(buf);
        fprintf(stderr, "Error: Read error\n");
        return NULL;
    }

    buf[len] = '\0';
    *size = (size_t)len;
    return buf;
}

/* Format size with units */
static void format_size(size_t bytes, char *buf, size_t buf_size) {
    if (bytes >= 1024 * 1024) {
        snprintf(buf, buf_size, "%.2f MB", (double)bytes / (1024 * 1024));
    } else if (bytes >= 1024) {
        snprintf(buf, buf_size, "%.2f KB", (double)bytes / 1024);
    } else {
        snprintf(buf, buf_size, "%zu B", bytes);
    }
}

/* Format throughput with units */
static void format_throughput(double bytes_per_sec, char *buf, size_t buf_size) {
    if (bytes_per_sec >= 1e9) {
        snprintf(buf, buf_size, "%.2f GB/s", bytes_per_sec / 1e9);
    } else if (bytes_per_sec >= 1e6) {
        snprintf(buf, buf_size, "%.2f MB/s", bytes_per_sec / 1e6);
    } else if (bytes_per_sec >= 1e3) {
        snprintf(buf, buf_size, "%.2f KB/s", bytes_per_sec / 1e3);
    } else {
        snprintf(buf, buf_size, "%.2f B/s", bytes_per_sec);
    }
}

/* Benchmark result */
typedef struct {
    uint64_t min_ns;
    uint64_t max_ns;
    uint64_t total_ns;
    uint64_t iterations;
} bench_result;

static void bench_result_init(bench_result *r) {
    r->min_ns = UINT64_MAX;
    r->max_ns = 0;
    r->total_ns = 0;
    r->iterations = 0;
}

static void bench_result_add(bench_result *r, uint64_t ns) {
    if (ns < r->min_ns) r->min_ns = ns;
    if (ns > r->max_ns) r->max_ns = ns;
    r->total_ns += ns;
    r->iterations++;
}

static double bench_result_avg_ns(const bench_result *r) {
    return (double)r->total_ns / (double)r->iterations;
}

/* Benchmark parsing */
static void bench_parse(const char *json, size_t len, int iterations, bench_result *result) {
    bench_result_init(result);

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        json_doc *doc = json_parse(json, len);
        if (doc) json_doc_free(doc);
    }

    /* Benchmark */
    for (int i = 0; i < iterations; i++) {
        uint64_t start = get_time_ns();
        json_doc *doc = json_parse(json, len);
        uint64_t end = get_time_ns();

        if (doc) {
            json_doc_free(doc);
            bench_result_add(result, end - start);
        }
    }
}

/* Benchmark stringification */
static void bench_stringify(const char *json, size_t len, int iterations, bench_result *result) {
    bench_result_init(result);

    json_doc *doc = json_parse(json, len);
    if (!doc) {
        fprintf(stderr, "Error: Failed to parse JSON for stringify benchmark\n");
        return;
    }

    json_val *root = json_doc_root(doc);

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        char *s = json_stringify(root);
        if (s) free(s);
    }

    /* Benchmark */
    for (int i = 0; i < iterations; i++) {
        uint64_t start = get_time_ns();
        char *s = json_stringify(root);
        uint64_t end = get_time_ns();

        if (s) {
            free(s);
            bench_result_add(result, end - start);
        }
    }

    json_doc_free(doc);
}

/* Print benchmark results */
static void print_results(const char *name, size_t size, const bench_result *result) {
    if (result->iterations == 0) {
        printf("  %-15s  (no valid iterations)\n", name);
        return;
    }

    double avg_ns = bench_result_avg_ns(result);
    double avg_sec = avg_ns / 1e9;
    double throughput = (double)size / avg_sec;

    char size_str[32];
    char throughput_str[32];
    format_size(size, size_str, sizeof(size_str));
    format_throughput(throughput, throughput_str, sizeof(throughput_str));

    printf("  %-15s  %10s  %12.2f us  %12s\n",
           name, size_str, avg_ns / 1000.0, throughput_str);
}

/* Print usage */
static void usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  -f, --file <path>    JSON file to benchmark\n");
    printf("  -n, --iterations <n> Number of iterations (default: %d)\n", DEFAULT_ITERATIONS);
    printf("  -h, --help           Show this help\n");
    printf("\n");
    printf("If no file is specified, uses built-in test data.\n");
}

/* Built-in test JSON */
static const char *builtin_json =
    "{"
    "  \"users\": ["
    "    {\"id\": 1, \"name\": \"Alice\", \"email\": \"alice@example.com\", \"active\": true},"
    "    {\"id\": 2, \"name\": \"Bob\", \"email\": \"bob@example.com\", \"active\": false},"
    "    {\"id\": 3, \"name\": \"Charlie\", \"email\": \"charlie@example.com\", \"active\": true}"
    "  ],"
    "  \"metadata\": {"
    "    \"version\": \"1.0.0\","
    "    \"generated\": \"2024-01-01T00:00:00Z\","
    "    \"count\": 3"
    "  },"
    "  \"tags\": [\"json\", \"test\", \"benchmark\", \"performance\"]"
    "}";

int main(int argc, char **argv) {
    const char *file_path = NULL;
    int iterations = DEFAULT_ITERATIONS;

    static struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"iterations", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:n:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                file_path = optarg;
                break;
            case 'n':
                iterations = atoi(optarg);
                if (iterations <= 0) iterations = DEFAULT_ITERATIONS;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    /* Initialize library */
    json_init();

    printf("json-asm benchmark v%s\n", json_version());
    printf("==================================\n\n");

    /* Print CPU features */
    uint32_t features = json_get_cpu_features();
    printf("CPU Features: 0x%08x\n", features);

#if defined(__x86_64__)
    printf("Architecture: x86-64\n");
    if (features & JSON_CPU_AVX512BW) printf("  Using: AVX-512\n");
    else if (features & JSON_CPU_AVX2) printf("  Using: AVX2\n");
    else if (features & JSON_CPU_SSE42) printf("  Using: SSE4.2\n");
#elif defined(__aarch64__)
    printf("Architecture: ARM64\n");
    if (features & JSON_CPU_SVE2) printf("  Using: SVE2\n");
    else if (features & JSON_CPU_SVE) printf("  Using: SVE\n");
    else if (features & JSON_CPU_NEON) printf("  Using: NEON\n");
#endif

    printf("Iterations: %d\n\n", iterations);

    /* Load JSON */
    char *json;
    size_t json_len;

    if (file_path) {
        json = load_file(file_path, &json_len);
        if (!json) return 1;
        printf("File: %s\n", file_path);
    } else {
        json_len = strlen(builtin_json);
        json = malloc(json_len + 1);
        memcpy(json, builtin_json, json_len + 1);
        printf("Using built-in test data\n");
    }

    char size_str[32];
    format_size(json_len, size_str, sizeof(size_str));
    printf("Size: %s\n\n", size_str);

    /* Verify parsing works */
    json_doc *doc = json_parse(json, json_len);
    if (!doc) {
        json_error_info err = json_get_error();
        fprintf(stderr, "Error: Failed to parse JSON: %s\n", err.message);
        free(json);
        return 1;
    }

    size_t value_count = json_doc_count(doc);
    printf("Values: %zu\n", value_count);

    size_t mem = json_doc_memory(doc);
    format_size(mem, size_str, sizeof(size_str));
    printf("Memory: %s (%.1f bytes/value)\n\n", size_str, (double)mem / value_count);

    json_doc_free(doc);

    /* Run benchmarks */
    printf("Results:\n");
    printf("  %-15s  %10s  %12s  %12s\n", "Operation", "Size", "Time", "Throughput");
    printf("  %-15s  %10s  %12s  %12s\n", "---------", "----", "----", "----------");

    bench_result parse_result;
    bench_parse(json, json_len, iterations, &parse_result);
    print_results("Parse", json_len, &parse_result);

    bench_result stringify_result;
    bench_stringify(json, json_len, iterations, &stringify_result);
    print_results("Stringify", json_len, &stringify_result);

    printf("\n");

    free(json);
    return 0;
}
