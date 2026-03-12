#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void fpsimd_logic_case(uint8_t* out);
void fpsimd_perm_case(uint8_t* out);
void fpsimd_arith_case(uint8_t* out);
void fpsimd_shift_case(uint8_t* out);
void fpsimd_fp_scalar_case(uint64_t* out);

static int fail_mem(const char* name, size_t index, uint64_t got, uint64_t want) {
  printf("FAIL section=%s index=%zu got=0x%016" PRIx64 " want=0x%016" PRIx64 "\n",
         name, index, got, want);
  return 1;
}

static int check_bytes(const char* name, const uint8_t* got, const uint8_t* want, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (got[i] != want[i]) {
      return fail_mem(name, i, got[i], want[i]);
    }
  }
  return 0;
}

static int check_u64(const char* name, const uint64_t* got, const uint64_t* want, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (got[i] != want[i]) {
      return fail_mem(name, i, got[i], want[i]);
    }
  }
  return 0;
}

static int test_logic(void) {
  static const uint8_t a[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  static const uint8_t b[16] = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
                                0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
  static const uint8_t mask[16] = {0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f,
                                   0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f, 0xf0, 0x0f};
  uint8_t got[16 * 7];
  uint8_t want[16 * 7];
  memset(got, 0, sizeof(got));
  memset(want, 0, sizeof(want));
  fpsimd_logic_case(got);

  for (size_t i = 0; i < 16; ++i) {
    want[i] = a[i] & b[i];
    want[16 + i] = a[i] ^ b[i];
    want[32 + i] = (uint8_t)((a[i] & (uint8_t)~mask[i]) | (b[i] & mask[i]));
    want[48 + i] = (uint8_t)((a[i] & mask[i]) | (b[i] & (uint8_t)~mask[i]));
    want[64 + i] = (uint8_t)((a[i] & mask[i]) | (b[i] & (uint8_t)~mask[i]));
    want[80 + i] = (a[i] == 0) ? 0xffu : 0x00u;
    want[96 + i] = (a[i] >= b[i]) ? 0xffu : 0x00u;
  }
  return check_bytes("logic", got, want, sizeof(got));
}

static int test_perm(void) {
  static const uint8_t a[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  static const uint8_t b[16] = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
                                0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f};
  static const uint16_t ha[8] = {0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007};
  static const uint16_t hb[8] = {0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007};
  static const uint32_t sa[4] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
  static const uint32_t sb[4] = {0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu, 0xddddddddu};
  uint8_t got[16 * 4];
  uint8_t want[16 * 4];
  memset(got, 0, sizeof(got));
  memset(want, 0, sizeof(want));
  fpsimd_perm_case(got);

  for (size_t i = 0; i < 11; ++i) {
    want[i] = a[i + 5];
  }
  for (size_t i = 0; i < 5; ++i) {
    want[11 + i] = b[i];
  }
  for (size_t i = 0; i < 8; ++i) {
    want[16 + i * 2] = a[i];
    want[16 + i * 2 + 1] = b[i];
  }
  {
    uint16_t* out = (uint16_t*)(want + 32);
    for (size_t i = 0; i < 4; ++i) {
      out[i] = ha[i * 2];
      out[4 + i] = hb[i * 2];
    }
  }
  {
    uint32_t* out = (uint32_t*)(want + 48);
    out[0] = sa[0];
    out[1] = sb[0];
    out[2] = sa[2];
    out[3] = sb[2];
  }
  return check_bytes("perm", got, want, sizeof(got));
}

static int test_arith(void) {
  static const uint8_t add_a[16] = {1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46};
  static const uint8_t add_b[16] = {3, 8, 13, 18, 23, 28, 33, 38, 43, 48, 53, 58, 63, 68, 73, 78};
  static const uint16_t sub_a[8] = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000};
  static const uint16_t sub_b[8] = {7, 11, 13, 17, 19, 23, 29, 31};
  static const uint32_t mul_a[4] = {7, 11, 13, 17};
  static const uint32_t mul_b[4] = {19, 23, 29, 31};
  uint8_t got[16 * 3];
  uint8_t want[16 * 3];
  memset(got, 0, sizeof(got));
  memset(want, 0, sizeof(want));
  fpsimd_arith_case(got);

  for (size_t i = 0; i < 16; ++i) {
    want[i] = (uint8_t)(add_a[i] + add_b[i]);
  }
  {
    uint16_t* out = (uint16_t*)(want + 16);
    for (size_t i = 0; i < 8; ++i) {
      out[i] = (uint16_t)(sub_a[i] - sub_b[i]);
    }
  }
  {
    uint32_t* out = (uint32_t*)(want + 32);
    for (size_t i = 0; i < 4; ++i) {
      out[i] = mul_a[i] * mul_b[i];
    }
  }
  return check_bytes("arith", got, want, sizeof(got));
}

static int test_shift(void) {
  static const uint16_t ushr_src[8] = {0x1234, 0x5678, 0x9abc, 0xdef0, 0x00ff, 0xff00, 0x0f0f, 0xf0f0};
  static const int32_t sshr_src[4] = {0x00000100, -512, 0x7fffffff, (int32_t)0x80000000u};
  static const uint16_t shrn_src[8] = {0x1234, 0x5678, 0x9abc, 0xdef0, 0x00ff, 0xff00, 0x0f0f, 0xf0f0};
  static const uint8_t uxtl_src[8] = {0x00, 0x7f, 0x80, 0xff, 0x12, 0x34, 0x56, 0x78};
  static const int16_t sxtl_src[4] = {1, 32767, -32768, -1};
  uint8_t got[72];
  uint8_t want[72];
  memset(got, 0, sizeof(got));
  memset(want, 0, sizeof(want));
  fpsimd_shift_case(got);

  {
    uint16_t* out = (uint16_t*)(want + 0);
    for (size_t i = 0; i < 8; ++i) {
      out[i] = (uint16_t)(ushr_src[i] >> 3);
    }
  }
  {
    uint32_t* out = (uint32_t*)(want + 16);
    for (size_t i = 0; i < 4; ++i) {
      out[i] = (uint32_t)(sshr_src[i] >> 5);
    }
  }
  for (size_t i = 0; i < 8; ++i) {
    want[32 + i] = (uint8_t)(shrn_src[i] >> 8);
  }
  {
    uint16_t* out = (uint16_t*)(want + 40);
    for (size_t i = 0; i < 8; ++i) {
      out[i] = uxtl_src[i];
    }
  }
  {
    uint32_t* out = (uint32_t*)(want + 56);
    for (size_t i = 0; i < 4; ++i) {
      out[i] = (uint32_t)(int32_t)sxtl_src[i];
    }
  }
  return check_bytes("shift", got, want, sizeof(got));
}

static int test_fp_scalar(void) {
  static const uint64_t want[] = {
      0xc018000000000000ull,
      0x401a000000000000ull,
      0x400c000000000000ull,
      0x400c000000000000ull,
      0x4008000000000000ull,
      0xc000000000000000ull,
      0xc045000000000000ull,
      0x4045000000000000ull,
      3ull,
      3ull,
      0x4014000000000000ull,
  };
  uint64_t got[sizeof(want) / sizeof(want[0])];
  memset(got, 0, sizeof(got));
  fpsimd_fp_scalar_case(got);
  return check_u64("fp_scalar", got, want, sizeof(want) / sizeof(want[0]));
}

int main(void) {
  if (test_logic() != 0) {
    return 1;
  }
  if (test_perm() != 0) {
    return 1;
  }
  if (test_arith() != 0) {
    return 1;
  }
  if (test_shift() != 0) {
    return 1;
  }
  if (test_fp_scalar() != 0) {
    return 1;
  }
  puts("FPSIMD_SELFTEST PASS");
  return 0;
}
