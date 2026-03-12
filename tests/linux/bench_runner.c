#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define PERF_BASE 0x09020000ull
#define PERF_SIZE 0x1000u

#define REG_ID        0x00
#define REG_CTRL      0x08
#define REG_CASE_ID   0x10
#define REG_ARG0      0x18
#define REG_ARG1      0x20
#define REG_STATUS    0x28
#define REG_HOST_NS   0x30
#define REG_STEPS     0x38
#define REG_TLB_HITS  0x40
#define REG_TLB_MISS  0x48
#define REG_PAGEWALK  0x50
#define REG_BUS_READ  0x58
#define REG_BUS_WRITE 0x60

#define CMD_BEGIN     1ull
#define CMD_END       2ull
#define CMD_EXIT      3ull
#define CMD_FLUSH_TLB 4ull

#define CASE_BASE64_ENC_4M   1ull
#define CASE_BASE64_DEC_4M   2ull
#define CASE_FNV1A_16M       3ull
#define CASE_TLB_SEQ_HOT_8M  4ull
#define CASE_TLB_SEQ_COLD_32M 5ull
#define CASE_TLB_RAND_32M    6ull

struct perf_mmio {
  int fd;
  volatile uint8_t* base;
};

struct perf_result {
  uint64_t host_ns;
  uint64_t steps;
  uint64_t tlb_hits;
  uint64_t tlb_misses;
  uint64_t page_walks;
  uint64_t bus_reads;
  uint64_t bus_writes;
};

static uint64_t readq_mmio(const struct perf_mmio* mmio, size_t off) {
  return *(volatile uint64_t*)(mmio->base + off);
}

static void writeq_mmio(const struct perf_mmio* mmio, size_t off, uint64_t value) {
  *(volatile uint64_t*)(mmio->base + off) = value;
}

static int perf_open(struct perf_mmio* mmio) {
  memset(mmio, 0, sizeof(*mmio));
  mmio->fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (mmio->fd < 0) {
    perror("open /dev/mem");
    return -1;
  }
  mmio->base = mmap(NULL, PERF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mmio->fd, PERF_BASE);
  if (mmio->base == MAP_FAILED) {
    perror("mmap perf mailbox");
    close(mmio->fd);
    mmio->fd = -1;
    return -1;
  }
  return 0;
}

static void perf_close(struct perf_mmio* mmio) {
  if (mmio->base && mmio->base != MAP_FAILED) {
    munmap((void*)mmio->base, PERF_SIZE);
  }
  if (mmio->fd >= 0) {
    close(mmio->fd);
  }
}

static void perf_begin(const struct perf_mmio* mmio, uint64_t case_id, uint64_t arg0) {
  writeq_mmio(mmio, REG_CASE_ID, case_id);
  writeq_mmio(mmio, REG_ARG0, arg0);
  writeq_mmio(mmio, REG_ARG1, 0);
  writeq_mmio(mmio, REG_CTRL, CMD_BEGIN);
}

static struct perf_result perf_end(const struct perf_mmio* mmio, uint64_t checksum) {
  struct perf_result result;
  writeq_mmio(mmio, REG_ARG1, checksum);
  writeq_mmio(mmio, REG_CTRL, CMD_END);
  result.host_ns = readq_mmio(mmio, REG_HOST_NS);
  result.steps = readq_mmio(mmio, REG_STEPS);
  result.tlb_hits = readq_mmio(mmio, REG_TLB_HITS);
  result.tlb_misses = readq_mmio(mmio, REG_TLB_MISS);
  result.page_walks = readq_mmio(mmio, REG_PAGEWALK);
  result.bus_reads = readq_mmio(mmio, REG_BUS_READ);
  result.bus_writes = readq_mmio(mmio, REG_BUS_WRITE);
  return result;
}

static void perf_flush_tlb(const struct perf_mmio* mmio) {
  writeq_mmio(mmio, REG_CTRL, CMD_FLUSH_TLB);
}

static void perf_exit_guest(const struct perf_mmio* mmio) {
  writeq_mmio(mmio, REG_CTRL, CMD_EXIT);
}

static uint64_t fnv1a64(const uint8_t* data, size_t len) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < len; ++i) {
    h ^= data[i];
    h *= 1099511628211ull;
  }
  return h;
}

static void fill_pattern(uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    data[i] = (uint8_t)((i * 131u + 17u) & 0xFFu);
  }
}

static size_t base64_encode_buf(const uint8_t* src, size_t len, uint8_t* out) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i = 0;
  size_t o = 0;
  while (i + 2 < len) {
    const uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) | (uint32_t)src[i + 2];
    out[o++] = (uint8_t)table[(v >> 18) & 0x3F];
    out[o++] = (uint8_t)table[(v >> 12) & 0x3F];
    out[o++] = (uint8_t)table[(v >> 6) & 0x3F];
    out[o++] = (uint8_t)table[v & 0x3F];
    i += 3;
  }
  if (i < len) {
    const uint32_t v = ((uint32_t)src[i] << 16) | ((i + 1 < len) ? ((uint32_t)src[i + 1] << 8) : 0u);
    out[o++] = (uint8_t)table[(v >> 18) & 0x3F];
    out[o++] = (uint8_t)table[(v >> 12) & 0x3F];
    out[o++] = (i + 1 < len) ? (uint8_t)table[(v >> 6) & 0x3F] : (uint8_t)'=';
    out[o++] = (uint8_t)'=';
  }
  return o;
}

static void build_base64_decode_table(uint8_t table[256]) {
  memset(table, 0xFF, 256);
  for (int i = 0; i < 26; ++i) {
    table[(unsigned char)('A' + i)] = (uint8_t)i;
    table[(unsigned char)('a' + i)] = (uint8_t)(26 + i);
  }
  for (int i = 0; i < 10; ++i) {
    table[(unsigned char)('0' + i)] = (uint8_t)(52 + i);
  }
  table[(unsigned char)'+'] = 62;
  table[(unsigned char)'/'] = 63;
}

static size_t base64_decode_buf(const uint8_t* src, size_t len, uint8_t* out) {
  uint8_t table[256];
  build_base64_decode_table(table);
  size_t i = 0;
  size_t o = 0;
  while (i + 3 < len) {
    const uint8_t a = table[src[i]];
    const uint8_t b = table[src[i + 1]];
    const uint8_t c = (src[i + 2] == '=') ? 0 : table[src[i + 2]];
    const uint8_t d = (src[i + 3] == '=') ? 0 : table[src[i + 3]];
    const uint32_t v = ((uint32_t)a << 18) | ((uint32_t)b << 12) | ((uint32_t)c << 6) | (uint32_t)d;
    out[o++] = (uint8_t)((v >> 16) & 0xFF);
    if (src[i + 2] != '=') {
      out[o++] = (uint8_t)((v >> 8) & 0xFF);
    }
    if (src[i + 3] != '=') {
      out[o++] = (uint8_t)(v & 0xFF);
    }
    i += 4;
  }
  return o;
}

static uint64_t page_stride_hash(uint8_t* buf, size_t len, const size_t* order, size_t count, size_t rounds) {
  (void)len;
  uint64_t sum = 0;
  for (size_t r = 0; r < rounds; ++r) {
    for (size_t i = 0; i < count; ++i) {
      const size_t page = order ? order[i] : i;
      const size_t off = page * 4096u;
      buf[off] ^= (uint8_t)(r + i);
      sum = (sum * 1315423911ull) ^ buf[off];
    }
  }
  return sum;
}

static void shuffle_pages(size_t* order, size_t count) {
  uint64_t state = 0x123456789abcdef0ull;
  for (size_t i = count; i > 1; --i) {
    state ^= state << 7;
    state ^= state >> 9;
    state ^= state << 8;
    const size_t j = (size_t)(state % i);
    const size_t tmp = order[i - 1];
    order[i - 1] = order[j];
    order[j] = tmp;
  }
}

static void print_result(const char* name, uint64_t bytes, uint64_t checksum, const struct perf_result* r) {
  printf("BENCH-RESULT name=%s bytes=%" PRIu64 " checksum=0x%016" PRIx64
         " host_ns=%" PRIu64 " steps=%" PRIu64 " tlb_hit=%" PRIu64
         " tlb_miss=%" PRIu64 " page_walk=%" PRIu64 " bus_read=%" PRIu64
         " bus_write=%" PRIu64 "\n",
         name, bytes, checksum, r->host_ns, r->steps, r->tlb_hits, r->tlb_misses,
         r->page_walks, r->bus_reads, r->bus_writes);
  fflush(stdout);
}

static int run_suite(const struct perf_mmio* mmio) {
  const size_t raw_4m = 4u * 1024u * 1024u;
  const size_t raw_16m = 16u * 1024u * 1024u;
  const size_t buf_8m = 8u * 1024u * 1024u;
  const size_t buf_32m = 32u * 1024u * 1024u;
  uint8_t* src = malloc(raw_16m);
  uint8_t* enc = malloc(((raw_4m + 2u) / 3u) * 4u + 16u);
  uint8_t* dec = malloc(raw_4m + 16u);
  uint8_t* hot = malloc(buf_8m);
  uint8_t* cold = malloc(buf_32m);
  size_t* order = malloc((buf_32m / 4096u) * sizeof(size_t));
  if (!src || !enc || !dec || !hot || !cold || !order) {
    fprintf(stderr, "alloc failed\n");
    return 1;
  }

  fill_pattern(src, raw_16m);
  fill_pattern(hot, buf_8m);
  fill_pattern(cold, buf_32m);

  printf("BENCH-START name=base64-enc-4m bytes=%zu\n", raw_4m);
  perf_begin(mmio, CASE_BASE64_ENC_4M, raw_4m);
  const size_t enc_len = base64_encode_buf(src, raw_4m, enc);
  const uint64_t enc_sum = fnv1a64(enc, enc_len);
  const struct perf_result enc_res = perf_end(mmio, enc_sum);
  print_result("base64-enc-4m", raw_4m, enc_sum, &enc_res);

  printf("BENCH-START name=base64-dec-4m bytes=%zu\n", raw_4m);
  perf_begin(mmio, CASE_BASE64_DEC_4M, raw_4m);
  const size_t dec_len = base64_decode_buf(enc, enc_len, dec);
  const uint64_t dec_sum = fnv1a64(dec, dec_len);
  const struct perf_result dec_res = perf_end(mmio, dec_sum);
  print_result("base64-dec-4m", raw_4m, dec_sum, &dec_res);

  printf("BENCH-START name=fnv1a-16m bytes=%zu\n", raw_16m);
  perf_begin(mmio, CASE_FNV1A_16M, raw_16m);
  const uint64_t fnv_sum = fnv1a64(src, raw_16m);
  const struct perf_result fnv_res = perf_end(mmio, fnv_sum);
  print_result("fnv1a-16m", raw_16m, fnv_sum, &fnv_res);

  printf("BENCH-START name=tlb-seq-hot-8m bytes=%zu\n", buf_8m * 8u);
  (void)page_stride_hash(hot, buf_8m, NULL, buf_8m / 4096u, 1u);
  perf_begin(mmio, CASE_TLB_SEQ_HOT_8M, buf_8m * 8u);
  const uint64_t hot_sum = page_stride_hash(hot, buf_8m, NULL, buf_8m / 4096u, 8u);
  const struct perf_result hot_res = perf_end(mmio, hot_sum);
  print_result("tlb-seq-hot-8m", buf_8m * 8u, hot_sum, &hot_res);

  printf("BENCH-START name=tlb-seq-cold-32m bytes=%zu\n", buf_32m * 2u);
  perf_flush_tlb(mmio);
  perf_begin(mmio, CASE_TLB_SEQ_COLD_32M, buf_32m * 2u);
  const uint64_t cold_sum = page_stride_hash(cold, buf_32m, NULL, buf_32m / 4096u, 2u);
  const struct perf_result cold_res = perf_end(mmio, cold_sum);
  print_result("tlb-seq-cold-32m", buf_32m * 2u, cold_sum, &cold_res);

  for (size_t i = 0; i < buf_32m / 4096u; ++i) {
    order[i] = i;
  }
  shuffle_pages(order, buf_32m / 4096u);
  printf("BENCH-START name=tlb-rand-32m bytes=%zu\n", buf_32m * 2u);
  perf_flush_tlb(mmio);
  perf_begin(mmio, CASE_TLB_RAND_32M, buf_32m * 2u);
  const uint64_t rand_sum = page_stride_hash(cold, buf_32m, order, buf_32m / 4096u, 2u);
  const struct perf_result rand_res = perf_end(mmio, rand_sum);
  print_result("tlb-rand-32m", buf_32m * 2u, rand_sum, &rand_res);

  free(order);
  free(cold);
  free(hot);
  free(dec);
  free(enc);
  free(src);
  return 0;
}

int main(void) {
  struct perf_mmio mmio;
  if (perf_open(&mmio) != 0) {
    return 1;
  }

  const uint64_t id = readq_mmio(&mmio, REG_ID);
  printf("PERF-MMIO id=0x%016" PRIx64 " status=%" PRIu64 "\n", id, readq_mmio(&mmio, REG_STATUS));
  fflush(stdout);
  if (id != 0x504552464d424f58ull) {
    fprintf(stderr, "unexpected perf mailbox id\n");
    perf_close(&mmio);
    return 1;
  }

  const int rc = run_suite(&mmio);
  printf("BENCH-SUITE rc=%d\n", rc);
  fflush(stdout);
  perf_exit_guest(&mmio);
  perf_close(&mmio);
  return rc;
}
