#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define FPSR_IOC (1u << 0)
#define FPSR_IXC (1u << 4)

static inline uint64_t read_fpsr(void) {
  uint64_t value;
  __asm__ volatile("mrs %0, fpsr" : "=r"(value));
  return value;
}

static inline void write_fpsr(uint64_t value) {
  __asm__ volatile("msr fpsr, %0" : : "r"(value));
}

static inline uint64_t fcvtzu_u64(double value) {
  uint64_t out;
  __asm__ volatile("fcvtzu %x0, %d1" : "=r"(out) : "w"(value));
  return out;
}

static inline int64_t fcvtzs_s64(double value) {
  int64_t out;
  __asm__ volatile("fcvtzs %x0, %d1" : "=r"(out) : "w"(value));
  return out;
}

static int fail_u64(const char* name, uint64_t got, uint64_t want) {
  printf("FAIL name=%s got=0x%016" PRIx64 " want=0x%016" PRIx64 "\n", name, got, want);
  return 1;
}

static int fail_s64(const char* name, int64_t got, int64_t want) {
  printf("FAIL name=%s got=%" PRId64 " want=%" PRId64 "\n", name, got, want);
  return 1;
}

int main(void) {
  union {
    uint64_t u;
    double d;
  } inf = {.u = 0x7ff0000000000000ull};

  write_fpsr(0);
  if (fcvtzu_u64(7.0) != 7u) {
    return fail_u64("fcvtzu-exact", fcvtzu_u64(7.0), 7u);
  }
  if (read_fpsr() != 0u) {
    return fail_u64("fcvtzu-exact-fpsr", read_fpsr(), 0u);
  }

  write_fpsr(0);
  if (fcvtzu_u64(3.75) != 3u) {
    return fail_u64("fcvtzu-frac", fcvtzu_u64(3.75), 3u);
  }
  if ((read_fpsr() & (FPSR_IOC | FPSR_IXC)) != FPSR_IXC) {
    return fail_u64("fcvtzu-frac-fpsr", read_fpsr(), FPSR_IXC);
  }

  write_fpsr(0);
  if (fcvtzu_u64(-1.0) != 0u) {
    return fail_u64("fcvtzu-neg", fcvtzu_u64(-1.0), 0u);
  }
  if ((read_fpsr() & (FPSR_IOC | FPSR_IXC)) != FPSR_IOC) {
    return fail_u64("fcvtzu-neg-fpsr", read_fpsr(), FPSR_IOC);
  }

  write_fpsr(0);
  if (fcvtzu_u64(inf.d) != UINT64_MAX) {
    return fail_u64("fcvtzu-inf", fcvtzu_u64(inf.d), UINT64_MAX);
  }
  if ((read_fpsr() & (FPSR_IOC | FPSR_IXC)) != FPSR_IOC) {
    return fail_u64("fcvtzu-inf-fpsr", read_fpsr(), FPSR_IOC);
  }

  write_fpsr(0);
  if (fcvtzs_s64(-3.75) != -3) {
    return fail_s64("fcvtzs-frac", fcvtzs_s64(-3.75), -3);
  }
  if ((read_fpsr() & (FPSR_IOC | FPSR_IXC)) != FPSR_IXC) {
    return fail_u64("fcvtzs-frac-fpsr", read_fpsr(), FPSR_IXC);
  }

  write_fpsr(0);
  if (fcvtzs_s64(inf.d) != INT64_MAX) {
    return fail_s64("fcvtzs-inf", fcvtzs_s64(inf.d), INT64_MAX);
  }
  if ((read_fpsr() & (FPSR_IOC | FPSR_IXC)) != FPSR_IOC) {
    return fail_u64("fcvtzs-inf-fpsr", read_fpsr(), FPSR_IOC);
  }

  puts("FPINT_SELFTEST PASS");
  return 0;
}
