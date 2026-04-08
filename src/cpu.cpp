#include "aarchvm/cpu.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <cfenv>
#include <bit>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace aarchvm {

namespace {

class BigUInt {
 public:
  BigUInt() = default;
  explicit BigUInt(std::uint64_t value) {
    if (value != 0u) {
      limbs_.push_back(static_cast<std::uint32_t>(value));
      const std::uint32_t high = static_cast<std::uint32_t>(value >> 32);
      if (high != 0u) {
        limbs_.push_back(high);
      }
    }
  }

  bool is_zero() const { return limbs_.empty(); }

  unsigned msb_index() const {
    const std::uint32_t limb = limbs_.back();
    return static_cast<unsigned>((limbs_.size() - 1u) * 32u + (31u - std::countl_zero(limb)));
  }

  void shift_left(unsigned shift) {
    if (is_zero() || shift == 0u) {
      return;
    }
    const std::size_t limb_shift = shift / 32u;
    const unsigned bit_shift = shift % 32u;
    std::vector<std::uint32_t> shifted(limbs_.size() + limb_shift + (bit_shift != 0u ? 1u : 0u), 0u);
    if (bit_shift == 0u) {
      for (std::size_t i = 0; i < limbs_.size(); ++i) {
        shifted[i + limb_shift] = limbs_[i];
      }
      limbs_ = std::move(shifted);
      return;
    }
    std::uint32_t carry = 0u;
    std::size_t out = limb_shift;
    for (std::uint32_t limb : limbs_) {
      const std::uint64_t wide = (static_cast<std::uint64_t>(limb) << bit_shift) | carry;
      shifted[out++] = static_cast<std::uint32_t>(wide);
      carry = static_cast<std::uint32_t>(wide >> 32);
    }
    if (carry != 0u) {
      shifted[out] = carry;
    }
    limbs_ = std::move(shifted);
    normalize();
  }

  static int compare(const BigUInt& lhs, const BigUInt& rhs) {
    if (lhs.limbs_.size() != rhs.limbs_.size()) {
      return lhs.limbs_.size() < rhs.limbs_.size() ? -1 : 1;
    }
    for (std::size_t i = lhs.limbs_.size(); i > 0; --i) {
      const std::uint64_t lhs_limb = lhs.limbs_[i - 1u];
      const std::uint64_t rhs_limb = rhs.limbs_[i - 1u];
      if (lhs_limb != rhs_limb) {
        return lhs_limb < rhs_limb ? -1 : 1;
      }
    }
    return 0;
  }

  friend bool operator<(const BigUInt& lhs, const BigUInt& rhs) { return compare(lhs, rhs) < 0; }

  friend BigUInt operator+(const BigUInt& lhs, const BigUInt& rhs) {
    BigUInt sum;
    const std::size_t count = lhs.limbs_.size() > rhs.limbs_.size() ? lhs.limbs_.size() : rhs.limbs_.size();
    sum.limbs_.assign(count + 1u, 0u);
    std::uint64_t carry = 0u;
    for (std::size_t i = 0; i < count; ++i) {
      const std::uint64_t lhs_limb = i < lhs.limbs_.size() ? lhs.limbs_[i] : 0u;
      const std::uint64_t rhs_limb = i < rhs.limbs_.size() ? rhs.limbs_[i] : 0u;
      const std::uint64_t wide = lhs_limb + rhs_limb + carry;
      sum.limbs_[i] = static_cast<std::uint32_t>(wide);
      carry = wide >> 32;
    }
    sum.limbs_[count] = static_cast<std::uint32_t>(carry);
    sum.normalize();
    return sum;
  }

  friend BigUInt operator-(const BigUInt& lhs, const BigUInt& rhs) {
    BigUInt diff;
    diff.limbs_.assign(lhs.limbs_.size(), 0u);
    std::uint64_t borrow = 0u;
    for (std::size_t i = 0; i < lhs.limbs_.size(); ++i) {
      const std::uint64_t lhs_limb = lhs.limbs_[i];
      const std::uint64_t rhs_limb = i < rhs.limbs_.size() ? rhs.limbs_[i] : 0u;
      const std::uint64_t subtrahend = rhs_limb + borrow;
      diff.limbs_[i] = static_cast<std::uint32_t>(lhs_limb - subtrahend);
      borrow = lhs_limb < subtrahend ? 1u : 0u;
    }
    diff.normalize();
    return diff;
  }

  friend BigUInt operator*(const BigUInt& lhs, const BigUInt& rhs) {
    if (lhs.is_zero() || rhs.is_zero()) {
      return BigUInt{};
    }
    BigUInt product;
    product.limbs_.assign(lhs.limbs_.size() + rhs.limbs_.size(), 0u);
    for (std::size_t i = 0; i < lhs.limbs_.size(); ++i) {
      std::uint64_t carry = 0u;
      for (std::size_t j = 0; j < rhs.limbs_.size(); ++j) {
        const std::uint64_t wide = static_cast<std::uint64_t>(lhs.limbs_[i]) *
                                       static_cast<std::uint64_t>(rhs.limbs_[j]) +
                                   product.limbs_[i + j] + carry;
        product.limbs_[i + j] = static_cast<std::uint32_t>(wide);
        carry = wide >> 32;
      }
      std::size_t k = i + rhs.limbs_.size();
      while (carry != 0u) {
        const std::uint64_t wide = static_cast<std::uint64_t>(product.limbs_[k]) + carry;
        product.limbs_[k] = static_cast<std::uint32_t>(wide);
        carry = wide >> 32;
        ++k;
      }
    }
    product.normalize();
    return product;
  }

 private:
  void normalize() {
    while (!limbs_.empty() && limbs_.back() == 0u) {
      limbs_.pop_back();
    }
  }

  std::vector<std::uint32_t> limbs_;
};

bool env_flag_enabled(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && value[0] != '\0';
}

bool irq_take_trace_enabled() {
  static const bool enabled = env_flag_enabled("AARCHVM_TRACE_IRQ_TAKE");
  return enabled;
}

bool branch_zero_trace_enabled() {
  static const bool enabled = env_flag_enabled("AARCHVM_TRACE_BRANCH_ZERO");
  return enabled;
}

bool debug_slow_mode_enabled() {
  static const bool enabled = env_flag_enabled("AARCHVM_DEBUG_SLOW");
  return enabled;
}

bool feat_fp16_absent(std::uint64_t id_aa64pfr0_el1) {
  return ((id_aa64pfr0_el1 >> 16) & 0xFu) == 0u;
}

bool insn_requires_fp16_feature(std::uint32_t insn) {
  const auto matches = [&](std::uint32_t mask, std::uint32_t value) {
    return (insn & mask) == value;
  };
  const std::uint32_t ftype = (insn >> 22) & 0x3u;

  // When FEAT_FP16 is absent, the half-precision data-processing space is
  // UNDEFINED. Only the H<->S/D conversion instructions remain architecturally
  // allocated.
  const bool scalar_half =
      (ftype == 3u) &&
      (matches(0xFF20FC00u, 0x1E202800u) ||   // FADD Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E203800u) ||   // FSUB Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E200800u) ||   // FMUL Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E201800u) ||   // FDIV Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E204800u) ||   // FMAX Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E205800u) ||   // FMIN Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E206800u) ||   // FMAXNM Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E207800u) ||   // FMINNM Hd, Hn, Hm
       matches(0xFF20FC00u, 0x1E208800u) ||   // FNMUL Hd, Hn, Hm
       matches(0xFF3FFC00u, 0x5E39D800u) ||   // FRECPE Hd, Hn
       matches(0xFF3FFC00u, 0x5E39F800u) ||   // FRECPX Hd, Hn
       matches(0xFF3FFC00u, 0x7E39D800u) ||   // FRSQRTE Hd, Hn
       matches(0xFFE0FC00u, 0x5EC03C00u) ||   // FRSQRTS Hd, Hn, Hm
       matches(0xFF208000u, 0x1F000000u) ||   // FMADD/FMSUB/FNMADD/FNMSUB (half)
       matches(0xFF208000u, 0x1F008000u) ||
       matches(0xFF208000u, 0x1F200000u) ||
       matches(0xFF208000u, 0x1F208000u) ||
       matches(0xFF201FE0u, 0x1E201000u) ||   // FMOV Hd, #imm
       matches(0xFF3FFC00u, 0x1E204000u) ||   // FMOV Hd, Hn
       matches(0xFF3FFC00u, 0x1E20C000u) ||   // FABS Hd, Hn
       matches(0xFF3FFC00u, 0x1E214000u) ||   // FNEG Hd, Hn
       matches(0xFF3FFC00u, 0x1E21C000u) ||   // FSQRT Hd, Hn
       matches(0xFF3FFC00u, 0x1E220000u) ||   // SCVTF Hd, Wn|Xn
       matches(0xFF3FFC00u, 0x9E220000u) ||
       matches(0xFF3FFC00u, 0x1E230000u) ||   // UCVTF Hd, Wn|Xn
       matches(0xFF3FFC00u, 0x9E230000u) ||
       matches(0xFF3FFC00u, 0x1E244000u) ||   // FRINTN/P/M/Z/A/X/I Hd, Hn
       matches(0xFF3FFC00u, 0x1E24C000u) ||
       matches(0xFF3FFC00u, 0x1E254000u) ||
       matches(0xFF3FFC00u, 0x1E25C000u) ||
       matches(0xFF3FFC00u, 0x1E264000u) ||
       matches(0xFF3FFC00u, 0x1E274000u) ||
       matches(0xFF3FFC00u, 0x1E27C000u) ||
       matches(0xFF3FFC00u, 0x1E380000u) ||   // FCVTZS Hd-source variants
       matches(0xFF3FFC00u, 0x9E380000u) ||
       matches(0xFF3FFC00u, 0x1E390000u) ||   // FCVTZU Hd-source variants
       matches(0xFF3FFC00u, 0x9E390000u) ||
       matches(0xFF20FC00u, 0x1E200000u) ||   // FCVT{N/U/A/P/M}{S/U} Wd|Xd, Hn
       matches(0xFF3FFC00u, 0x1E260000u) ||   // FMOV Wt, Hn
       matches(0xFF3FFC00u, 0x1E270000u) ||   // FMOV Hd, Wn
       matches(0xFF200C00u, 0x1E200C00u) ||   // FCSEL Hd, Hn, Hm, cond
       matches(0xFF3CAF00u, 0x1E200400u) ||   // FCCMP/FCCMPE Hn, Hm, #nzcv, cond
       (insn & 0xFF20FC1Fu) == 0x1E202000u || // FCMP/FCMPE Hn, Hm
       (insn & 0xFF20FC1Fu) == 0x1E202010u ||
       (insn & 0xFF20FC1Fu) == 0x1E202008u || // FCMP/FCMPE Hn, #0.0
       (insn & 0xFF20FC1Fu) == 0x1E202018u ||
       matches(0xFFFFFC00u, 0x7EC21400u) ||   // FABD Hd, Hn, Hm
       matches(0xFFFFFC00u, 0x5EF8D800u) ||   // FCMEQ Hd, Hn, #0.0
       matches(0xFFFFFC00u, 0x7EF8C800u) ||   // FCMGE Hd, Hn, #0.0
       matches(0xFFFFFC00u, 0x5EF8C800u) ||   // FCMGT Hd, Hn, #0.0
       matches(0xFFFFFC00u, 0x7EF8D800u) ||   // FCMLE Hd, Hn, #0.0
       matches(0xFFFFFC00u, 0x5EF8E800u));    // FCMLT Hd, Hn, #0.0

  const bool scalar_half_compare_reg =
      matches(0xFFFFFC00u, 0x5E422400u) ||     // FCMEQ Hd, Hn, Hm
      matches(0xFFFFFC00u, 0x7E422400u) ||     // FCMGE Hd, Hn, Hm
      matches(0xFFFFFC00u, 0x7EC22400u) ||     // FCMGT Hd, Hn, Hm
      matches(0xFFFFFC00u, 0x7E422C00u) ||     // FACGE Hd, Hn, Hm
      matches(0xFFFFFC00u, 0x7EC22C00u);       // FACGT Hd, Hn, Hm

  const bool vector_half =
      matches(0xFF20FC00u, 0x5E001C00u) ||     // FMULX Hd, Hn, Hm
      matches(0xFFE0FC00u, 0x5E403C00u) ||     // FRECPS Hd, Hn, Hm
      matches(0xBF20FC00u, 0x0E001400u) ||     // FADD/FSUB Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      matches(0xBF20FC00u, 0x2E001C00u) ||     // FMUL Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      (matches(0xBF20FC00u, 0x0E001C00u) &&    // FMULX Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
       ((insn >> 22) & 1u) != 0u) ||          // distinguish from INS/MOV (element)
      matches(0xBFE0FC00u, 0x2E403C00u) ||     // FDIV Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      matches(0xBFFFFC00u, 0x2EC21400u) ||     // FABD Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      matches(0xBFFFFC00u, 0x0E422400u) ||     // FCMEQ Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      matches(0xBFFFFC00u, 0x2E422400u) ||     // FCMGE Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      matches(0xBFFFFC00u, 0x2EC22400u) ||     // FCMGT Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      matches(0xBFFFFC00u, 0x0EF8D800u) ||     // FCMEQ Vd.4H|8H, Vn.4H|8H, #0.0
      matches(0xBFFFFC00u, 0x2EF8C800u) ||     // FCMGE Vd.4H|8H, Vn.4H|8H, #0.0
      matches(0xBFFFFC00u, 0x0EF8C800u) ||     // FCMGT Vd.4H|8H, Vn.4H|8H, #0.0
      matches(0xBFFFFC00u, 0x2EF8D800u) ||     // FCMLE Vd.4H|8H, Vn.4H|8H, #0.0
      matches(0xBFFFFC00u, 0x0EF8E800u) ||     // FCMLT Vd.4H|8H, Vn.4H|8H, #0.0
      matches(0x9F20FC00u, 0x0E003400u) ||     // FMAX/FMIN Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
      (matches(0x9F20FC00u, 0x0E000400u) &&    // FMAXNM/FMINNM Vd.4H|8H, Vn.4H|8H, Vm.4H|8H
       ((insn >> 22) & 1u) != 0u) ||          // distinguish from DUP (element)
      matches(0xBF80F400u, 0x2F009000u);       // FMULX Vd.4H|8H, Vn.4H|8H, Vm.H[index]

  return scalar_half || scalar_half_compare_reg || vector_half;
}

bool insn_undef_due_to_absent_fp_asimd_feature(std::uint32_t insn,
                                               std::uint64_t pfr0,
                                               std::uint64_t isar0,
                                               std::uint64_t isar1) {
  const auto matches = [&](std::uint32_t mask, std::uint32_t value) {
    return (insn & mask) == value;
  };

  const std::uint32_t aes = static_cast<std::uint32_t>((isar0 >> 4) & 0xFu);
  const std::uint32_t sha1 = static_cast<std::uint32_t>((isar0 >> 8) & 0xFu);
  const std::uint32_t sha2 = static_cast<std::uint32_t>((isar0 >> 12) & 0xFu);
  const std::uint32_t rdm = static_cast<std::uint32_t>((isar0 >> 28) & 0xFu);
  const std::uint32_t sha3 = static_cast<std::uint32_t>((isar0 >> 32) & 0xFu);
  const std::uint32_t sm3 = static_cast<std::uint32_t>((isar0 >> 36) & 0xFu);
  const std::uint32_t sm4 = static_cast<std::uint32_t>((isar0 >> 40) & 0xFu);
  const std::uint32_t dotprod = static_cast<std::uint32_t>((isar0 >> 44) & 0xFu);
  const std::uint32_t fhm = static_cast<std::uint32_t>((isar0 >> 48) & 0xFu);
  const std::uint32_t jscvt = static_cast<std::uint32_t>((isar1 >> 12) & 0xFu);
  const std::uint32_t fcma = static_cast<std::uint32_t>((isar1 >> 16) & 0xFu);
  const std::uint32_t frintts = static_cast<std::uint32_t>((isar1 >> 32) & 0xFu);
  const std::uint32_t bf16 = static_cast<std::uint32_t>((isar1 >> 44) & 0xFu);
  const std::uint32_t i8mm = static_cast<std::uint32_t>((isar1 >> 52) & 0xFu);

  if (jscvt == 0u && matches(0xFFFFFC00u, 0x1E7E0000u)) { // FJCVTZS Wd, Dn
    return true;
  }

  if (feat_fp16_absent(pfr0) && insn_requires_fp16_feature(insn)) {
    return true;
  }

  if (frintts == 0u &&
      (matches(0xFFFFFC00u, 0x1E284000u) ||   // FRINT32Z Sd, Sn
       matches(0xFFFFFC00u, 0x1E684000u) ||   // FRINT32Z Dd, Dn
       matches(0xFFFFFC00u, 0x1E28C000u) ||   // FRINT32X Sd, Sn
       matches(0xFFFFFC00u, 0x1E68C000u) ||   // FRINT32X Dd, Dn
       matches(0xFFFFFC00u, 0x1E294000u) ||   // FRINT64Z Sd, Sn
       matches(0xFFFFFC00u, 0x1E694000u) ||   // FRINT64Z Dd, Dn
       matches(0xFFFFFC00u, 0x1E29C000u) ||   // FRINT64X Sd, Sn
       matches(0xFFFFFC00u, 0x1E69C000u) ||   // FRINT64X Dd, Dn
       matches(0xFFFFFC00u, 0x0E21E800u) ||   // FRINT32Z Vd.2S, Vn.2S
       matches(0xFFFFFC00u, 0x4E21E800u) ||   // FRINT32Z Vd.4S, Vn.4S
       matches(0xFFFFFC00u, 0x4E61E800u) ||   // FRINT32Z Vd.2D, Vn.2D
       matches(0xFFFFFC00u, 0x2E21E800u) ||   // FRINT32X Vd.2S, Vn.2S
       matches(0xFFFFFC00u, 0x6E21E800u) ||   // FRINT32X Vd.4S, Vn.4S
       matches(0xFFFFFC00u, 0x6E61E800u) ||   // FRINT32X Vd.2D, Vn.2D
       matches(0xFFFFFC00u, 0x0E21F800u) ||   // FRINT64Z Vd.2S, Vn.2S
       matches(0xFFFFFC00u, 0x4E21F800u) ||   // FRINT64Z Vd.4S, Vn.4S
       matches(0xFFFFFC00u, 0x4E61F800u) ||   // FRINT64Z Vd.2D, Vn.2D
       matches(0xFFFFFC00u, 0x2E21F800u) ||   // FRINT64X Vd.2S, Vn.2S
       matches(0xFFFFFC00u, 0x6E21F800u) ||   // FRINT64X Vd.4S, Vn.4S
       matches(0xFFFFFC00u, 0x6E61F800u))) {  // FRINT64X Vd.2D, Vn.2D
    return true;
  }

  if (bf16 == 0u &&
      (matches(0xFFFFFC00u, 0x1E634000u) ||   // BFCVT Hd, Sn
       ((((insn >> 22) & 0x3u) == 2u) &&      // BFCVTN/BFCVTN2 use the BF16 ftype
        (matches(0xFF3FFC00u, 0x0E216800u) ||
         matches(0xFF3FFC00u, 0x4E216800u))) ||
       matches(0xBFF8FF5Cu, 0x2EC0FC00u) ||   // BFMLALB/BFMLALT
       matches(0xBFF8FF5Cu, 0x2E40FC00u) ||   // BFDOT
       matches(0xBFF8FF5Cu, 0x2E40EC00u))) {  // BFMMLA
    return true;
  }

  if (fcma == 0u &&
      (matches(0xBF20EC00u, 0x2E00E400u) ||   // FCADD
       matches(0xBF20EC00u, 0x2E00CC00u) ||   // FCMLA (vector)
       matches(0xBF809400u, 0x2F001000u) ||   // FCMLA (by element, half-precision)
       matches(0xBF809400u, 0x2F801000u))) {  // FCMLA (by element, single-precision)
    return true;
  }

  if (rdm == 0u &&
      (matches(0xBF20FC00u, 0x2E008400u) ||   // SQRDMLAH (vector)
       matches(0xBF20FC00u, 0x2E008C00u) ||   // SQRDMLSH (vector)
       matches(0xFF20FC00u, 0x7E008400u) ||   // SQRDMLAH (scalar)
       matches(0xFF20FC00u, 0x7E008C00u) ||   // SQRDMLSH (scalar)
       matches(0xFF00F400u, 0x6F00D000u) ||   // SQRDMLAH (by element, vector)
       matches(0xFF00F400u, 0x6F00F000u) ||   // SQRDMLSH (by element, vector)
       matches(0xFF00F400u, 0x7F00D000u) ||   // SQRDMLAH (by element, scalar)
       matches(0xFF00F400u, 0x7F00F000u))) {  // SQRDMLSH (by element, scalar)
    return true;
  }

  if (dotprod == 0u &&
      (matches(0xBFF8FF5Cu, 0x2E809400u) ||   // UDOT
       matches(0xBFF8FF5Cu, 0x0E809400u))) {  // SDOT
    return true;
  }

  if (fhm == 0u &&
      (matches(0xBFF8FF5Cu, 0x0E20EC00u) ||   // FMLAL (vector)
       matches(0xFFFCFE50u, 0x6E28CC40u) ||   // FMLAL2 (vector)
       matches(0xBFE0FC5Cu, 0x0EA0EC0Cu) ||   // FMLSL (vector)
       matches(0xFFFCFF58u, 0x6EB4CE50u) ||   // FMLSL2 (vector)
       matches(0xBFC0F800u, 0x0F800000u) ||   // FMLAL/FMLAL2 (by element)
       matches(0xBFC0F800u, 0x0F804000u))) {  // FMLSL/FMLSL2 (by element)
    return true;
  }

  if (i8mm == 0u &&
      (matches(0xFFF0FC00u, 0x4E80A400u) ||   // SMMLA
       matches(0xFFF0FC00u, 0x6E80A400u) ||   // UMMLA
       matches(0xFFF0FC00u, 0x4E80AC00u) ||   // USMMLA
       matches(0xBFF0FC00u, 0x0E809C00u) ||   // USDOT (vector)
       matches(0xFF80F400u, 0x4F80F000u) ||   // USDOT (by element)
       matches(0xFF80F400u, 0x4F00F000u))) {  // SUDOT (by element)
    return true;
  }

  if (aes == 0u &&
      (matches(0xFFFFFC00u, 0x4E284800u) ||   // AESE
       matches(0xFFFFFC00u, 0x4E285800u) ||   // AESD
       matches(0xFFFFFC00u, 0x4E286800u) ||   // AESMC
       matches(0xFFFFFC00u, 0x4E287800u))) {  // AESIMC
    return true;
  }

  if (aes < 2u &&
      (matches(0xBF20FC00u, 0x2E209C00u) ||   // PMUL
       matches(0xBF20FC00u, 0x0E20E000u))) {  // PMULL/PMULL2
    return true;
  }

  if (sha1 == 0u &&
      (matches(0xFFF8FF5Cu, 0x5E000000u) ||   // SHA1C
       matches(0xFFF8FF5Cu, 0x5E001000u) ||   // SHA1P
       matches(0xFFF8FF5Cu, 0x5E002000u) ||   // SHA1M
       matches(0xFFF8FF5Cu, 0x5E003000u) ||   // SHA1SU0
       matches(0xFFFFFC00u, 0x5E280800u) ||   // SHA1H
       matches(0xFFFFFC00u, 0x5E281800u))) {  // SHA1SU1
    return true;
  }

  if (sha2 == 0u &&
      (matches(0xFFF8FF5Cu, 0x5E004000u) ||   // SHA256H
       matches(0xFFF8FF5Cu, 0x5E005000u) ||   // SHA256H2
       matches(0xFFF8FF5Cu, 0x5E006000u) ||   // SHA256SU1
       matches(0xFFFFFC00u, 0x5E282800u))) {  // SHA256SU0
    return true;
  }

  if (sha3 == 0u &&
      (matches(0xFFE08000u, 0xCE000000u) ||   // EOR3
       matches(0xFFE08000u, 0xCE200000u) ||   // BCAX
       matches(0xFFE0FC00u, 0xCE608C00u) ||   // RAX1
       matches(0xFFE00000u, 0xCE800000u))) {  // XAR
    return true;
  }

  if (sm3 == 0u &&
      (matches(0xFFE08000u, 0xCE400000u) ||   // SM3SS1
       matches(0xFFE0CC00u, 0xCE408000u) ||   // SM3TT1A
       matches(0xFFE0CC00u, 0xCE408400u) ||   // SM3TT1B
       matches(0xFFE0CC00u, 0xCE408800u) ||   // SM3TT2A
       matches(0xFFE0CC00u, 0xCE408C00u) ||   // SM3TT2B
       matches(0xFFE0FC00u, 0xCE60C000u) ||   // SM3PARTW1
       matches(0xFFE0FC00u, 0xCE60C400u))) {  // SM3PARTW2
    return true;
  }

  if (sm4 == 0u &&
      (matches(0xFFFFFC00u, 0xCEC08400u) ||   // SM4E
       matches(0xFFE0FC00u, 0xCE60C800u))) {  // SM4EKEY
    return true;
  }

  if (sha2 < 2u &&
      (matches(0xFFE0FC00u, 0xCE608000u) ||   // SHA512H
       matches(0xFFE0FC00u, 0xCE608400u) ||   // SHA512H2
       matches(0xFFFFFC00u, 0xCEC08000u) ||   // SHA512SU0
       matches(0xFFE0FC00u, 0xCE608800u))) {  // SHA512SU1
    return true;
  }

  return false;
}

bool brk_halt_mode_enabled() {
  static const bool enabled = []() {
    const char* value = std::getenv("AARCHVM_BRK_MODE");
    return value != nullptr && std::strcmp(value, "halt") == 0;
  }();
  return enabled;
}

constexpr bool is_ldraa_ldrab_encoding(std::uint32_t insn) {
  return (insn & 0xFF200400u) == 0xF8200400u;
}

constexpr std::uint16_t kDecodedFlagSf = 1u << 0;
constexpr std::uint16_t kDecodedFlagSetFlags = 1u << 1;
constexpr std::uint16_t kDecodedFlagSub = 1u << 2;
constexpr std::uint16_t kDecodedFlagNonZero = 1u << 3;
constexpr std::uint16_t kDecodedFlagLink = 1u << 4;
constexpr std::uint16_t kDecodedFlagLoad = 1u << 5;
constexpr std::uint16_t kDecodedFlagSigned = 1u << 6;
constexpr std::uint16_t kDecodedFlagResult64 = 1u << 7;
constexpr std::uint16_t kDecodedFlagInvert = 1u << 8;
constexpr std::uint32_t kTimerVirtPpiIntId = 27u;
constexpr std::uint32_t kTimerPhysPpiIntId = 30u;
constexpr std::uint32_t kDebugExceptionIfsc = 0x22u;
constexpr std::uint64_t kDebugCtrlEnable = 1ull << 0;
constexpr std::uint64_t kDebugCtrlPrivilegeMask = 0x3ull << 1;
constexpr std::uint64_t kDebugCtrlTypeMask = 0x3ull << 3;
constexpr std::uint64_t kDebugCtrlBreakpointBasMask = 0xFull << 5;
constexpr std::uint64_t kDebugCtrlWatchpointBasMask = 0xFFull << 5;
constexpr std::uint32_t kDebugCtrlPrivilegeEl1 = 0x1u;
constexpr std::uint32_t kDebugCtrlPrivilegeEl0 = 0x2u;
constexpr std::uint32_t kDebugCtrlTypeExecute = 0x0u;
constexpr std::uint32_t kDebugCtrlTypeLoad = 0x1u;
constexpr std::uint32_t kDebugCtrlTypeStore = 0x2u;
constexpr std::size_t kDataCacheLineBytes = 64u;
constexpr std::uint64_t kMdscrEl1Tdcc = 1ull << 12u;
constexpr std::uint64_t kMdscrEl1Tda = 1ull << 21u;
constexpr std::uint64_t kMdscrEl1Txfull = 1ull << 29u;
constexpr std::uint64_t kMdscrEl1Rxfull = 1ull << 30u;
constexpr std::uint64_t kSctlrEl1Dze = 1ull << 14u;
constexpr std::uint64_t kSctlrEl1Uct = 1ull << 15u;
constexpr std::uint64_t kSctlrEl1Uma = 1ull << 9u;
constexpr std::uint64_t kSctlrEl1NTwi = 1ull << 16u;
constexpr std::uint64_t kSctlrEl1NTwe = 1ull << 18u;
constexpr std::uint64_t kSctlrEl1Uci = 1ull << 26u;

constexpr std::uint32_t sysreg_key(std::uint32_t op0,
                                   std::uint32_t op1,
                                   std::uint32_t crn,
                                   std::uint32_t crm,
                                   std::uint32_t op2) {
  return (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;
}

constexpr std::uint32_t sysreg_trap_iss(bool is_read,
                                        std::uint32_t op0,
                                        std::uint32_t op1,
                                        std::uint32_t crn,
                                        std::uint32_t crm,
                                        std::uint32_t op2,
                                        std::uint32_t rt) {
  return ((op0 & 0x3u) << 20u) |
         ((op2 & 0x7u) << 17u) |
         ((op1 & 0x7u) << 14u) |
         ((crn & 0xFu) << 10u) |
         ((rt & 0x1Fu) << 5u) |
         ((crm & 0xFu) << 1u) |
         (is_read ? 1u : 0u);
}

std::uint64_t ones(std::uint32_t bits) {
  if (bits == 0) {
    return 0;
  }
  if (bits >= 64) {
    return ~0ull;
  }
  return (1ull << bits) - 1ull;
}

std::uint64_t ror(std::uint64_t value, std::uint32_t shift, std::uint32_t width) {
  if (width == 0) {
    return 0;
  }
  shift %= width;
  const std::uint64_t mask = ones(width);
  value &= mask;
  if (shift == 0) {
    return value;
  }
  return ((value >> shift) | (value << (width - shift))) & mask;
}

bool ranges_overlap(std::uint64_t a_base, std::size_t a_size, std::uint64_t b_base, std::size_t b_size) {
  if (a_size == 0u || b_size == 0u) {
    return false;
  }
  const std::uint64_t a_end = a_base + static_cast<std::uint64_t>(a_size);
  const std::uint64_t b_end = b_base + static_cast<std::uint64_t>(b_size);
  return a_base < b_end && b_base < a_end;
}

std::uint64_t replicate(std::uint64_t value, std::uint32_t esize, std::uint32_t datasize) {
  const std::uint64_t elem_mask = ones(esize);
  const std::uint64_t elem = value & elem_mask;
  std::uint64_t out = 0;
  for (std::uint32_t pos = 0; pos < datasize; pos += esize) {
    out |= (elem << pos);
  }
  return out;
}

std::uint64_t advsimd_expand_imm(bool op, std::uint32_t cmode, std::uint8_t imm8) {
  switch ((cmode >> 1u) & 0x7u) {
    case 0x0u:
      return replicate(static_cast<std::uint64_t>(imm8), 32u, 64u);
    case 0x1u:
      return replicate(static_cast<std::uint64_t>(imm8) << 8u, 32u, 64u);
    case 0x2u:
      return replicate(static_cast<std::uint64_t>(imm8) << 16u, 32u, 64u);
    case 0x3u:
      return replicate(static_cast<std::uint64_t>(imm8) << 24u, 32u, 64u);
    case 0x4u:
      return replicate(static_cast<std::uint64_t>(imm8), 16u, 64u);
    case 0x5u:
      return replicate(static_cast<std::uint64_t>(imm8) << 8u, 16u, 64u);
    case 0x6u:
      if ((cmode & 1u) == 0u) {
        return replicate((static_cast<std::uint64_t>(imm8) << 8u) | 0xFFu, 32u, 64u);
      }
      return replicate((static_cast<std::uint64_t>(imm8) << 16u) | 0xFFFFu, 32u, 64u);
    case 0x7u:
      if ((cmode & 1u) == 0u) {
        return replicate(static_cast<std::uint64_t>(imm8), 8u, 64u);
      }
      if (!op) {
        const std::uint32_t imm32 =
            ((static_cast<std::uint32_t>((imm8 >> 7) & 1u)) << 31u) |
            ((static_cast<std::uint32_t>(((imm8 >> 6) & 1u) ^ 1u)) << 30u) |
            (static_cast<std::uint32_t>((imm8 >> 6) & 1u ? 0x3Fu : 0u) << 25u) |
            ((static_cast<std::uint32_t>(imm8) & 0x3Fu) << 19u);
        return replicate(imm32, 32u, 64u);
      }
      std::uint64_t out = 0;
      for (std::uint32_t bit = 0; bit < 8u; ++bit) {
        out |= (((imm8 >> bit) & 1u) ? 0xFFull : 0ull) << (bit * 8u);
      }
      return out;
  }
  return 0;
}

std::uint64_t vfp_expand_imm(bool is_double, std::uint8_t imm8) {
  const std::uint64_t sign = static_cast<std::uint64_t>((imm8 >> 7) & 1u);
  const std::uint64_t b = static_cast<std::uint64_t>((imm8 >> 6) & 1u);
  const std::uint64_t cd = static_cast<std::uint64_t>((imm8 >> 4) & 0x3u);
  const std::uint64_t efgh = static_cast<std::uint64_t>(imm8 & 0xFu);
  if (is_double) {
    const std::uint64_t exponent = ((b ^ 1u) << 10u) | ((b ? 0xFFull : 0ull) << 2u) | cd;
    const std::uint64_t fraction = efgh << 48u;
    return (sign << 63u) | (exponent << 52u) | fraction;
  }
  const std::uint64_t exponent = ((b ^ 1u) << 7u) | ((b ? 0x1Full : 0ull) << 2u) | cd;
  const std::uint64_t fraction = efgh << 19u;
  return (sign << 31u) | (exponent << 23u) | fraction;
}

std::uint64_t scalar_byte_mask_imm(std::uint8_t imm8) {
  std::uint64_t out = 0;
  for (std::uint32_t bit = 0; bit < 8u; ++bit) {
    out |= (((imm8 >> bit) & 1u) ? 0xFFull : 0ull) << (bit * 8u);
  }
  return out;
}

std::uint64_t bit_reverse(std::uint64_t value, std::uint32_t width) {
  std::uint64_t out = 0;
  for (std::uint32_t i = 0; i < width; ++i) {
    out <<= 1;
    out |= (value >> i) & 1u;
  }
  return out;
}

std::uint32_t cls32(std::uint32_t value) {
  const bool sign = (value >> 31) != 0;
  const std::uint32_t x = sign ? ~value : value;
  if (x == 0) {
    return 31;
  }
  return static_cast<std::uint32_t>(__builtin_clz(x) - 1);
}

std::uint64_t cls64(std::uint64_t value) {
  const bool sign = (value >> 63) != 0;
  const std::uint64_t x = sign ? ~value : value;
  if (x == 0) {
    return 63;
  }
  return static_cast<std::uint64_t>(__builtin_clzll(x) - 1);
}

std::uint64_t clz_bits(std::uint64_t value, std::uint32_t width) {
  value &= ones(width);
  if (value == 0u) {
    return width;
  }
  if (width <= 32u) {
    return static_cast<std::uint64_t>(__builtin_clz(static_cast<std::uint32_t>(value)) - (32u - width));
  }
  return static_cast<std::uint64_t>(__builtin_clzll(value) - (64u - width));
}

std::uint64_t cls_bits(std::uint64_t value, std::uint32_t width) {
  value &= ones(width);
  const std::uint64_t sign = (value >> (width - 1u)) & 1u;
  std::uint64_t count = 0u;
  for (int bit = static_cast<int>(width) - 2; bit >= 0; --bit) {
    if (((value >> static_cast<std::uint32_t>(bit)) & 1u) != sign) {
      break;
    }
    ++count;
  }
  return count;
}

std::uint32_t crc32_update(std::uint32_t crc, std::uint64_t data, std::uint32_t bytes, std::uint32_t poly_reflected) {
  for (std::uint32_t i = 0; i < bytes; ++i) {
    crc ^= static_cast<std::uint8_t>((data >> (8u * i)) & 0xFFu);
    for (std::uint32_t b = 0; b < 8u; ++b) {
      crc = (crc >> 1) ^ ((crc & 1u) ? poly_reflected : 0u);
    }
  }
  return crc;
}

std::uint64_t umulh64(std::uint64_t lhs, std::uint64_t rhs) {
  const std::uint64_t lhs_lo = lhs & 0xFFFFFFFFull;
  const std::uint64_t lhs_hi = lhs >> 32u;
  const std::uint64_t rhs_lo = rhs & 0xFFFFFFFFull;
  const std::uint64_t rhs_hi = rhs >> 32u;

  const std::uint64_t p00 = lhs_lo * rhs_lo;
  const std::uint64_t p01 = lhs_lo * rhs_hi;
  const std::uint64_t p10 = lhs_hi * rhs_lo;
  const std::uint64_t p11 = lhs_hi * rhs_hi;

  const std::uint64_t middle = (p00 >> 32u) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  return p11 + (p01 >> 32u) + (p10 >> 32u) + (middle >> 32u);
}

std::uint64_t smulh64(std::uint64_t lhs, std::uint64_t rhs) {
  std::uint64_t high = umulh64(lhs, rhs);
  if ((lhs >> 63u) != 0) {
    high -= rhs;
  }
  if ((rhs >> 63u) != 0) {
    high -= lhs;
  }
  return high;
}

bool decode_bit_masks(std::uint32_t n,
                      std::uint32_t imms,
                      std::uint32_t immr,
                      std::uint32_t datasize,
                      std::uint64_t& wmask,
                      std::uint64_t& tmask) {
  const std::uint32_t not_imms = (~imms) & 0x3Fu;
  const std::uint32_t concat = (n << 6) | not_imms;
  int len = -1;
  for (int i = 6; i >= 0; --i) {
    if (((concat >> i) & 1u) != 0) {
      len = i;
      break;
    }
  }
  if (len < 1) {
    return false;
  }
  const std::uint32_t levels = (1u << len) - 1u;
  const std::uint32_t s = imms & levels;
  const std::uint32_t r = immr & levels;
  const std::uint32_t esize = 1u << len;
  const std::uint32_t d = (s - r) & levels;
  const std::uint64_t welem = ones(s + 1);
  const std::uint64_t telem = ones(d + 1);
  wmask = replicate(ror(welem, r, esize), esize, datasize);
  tmask = replicate(telem, esize, datasize);
  return true;
}

std::uint64_t vector_get_bits(const std::array<std::uint64_t, 2>& qreg, std::uint32_t lsb, std::uint32_t width) {
  if (width == 0u || width > 64u || lsb >= 128u) {
    return 0;
  }
  if (lsb >= 64u) {
    return (qreg[1] >> (lsb - 64u)) & ones(width);
  }
  if (lsb + width <= 64u) {
    return (qreg[0] >> lsb) & ones(width);
  }
  const std::uint32_t low_bits = 64u - lsb;
  const std::uint64_t low = (qreg[0] >> lsb) & ones(low_bits);
  const std::uint64_t high = qreg[1] & ones(width - low_bits);
  return low | (high << low_bits);
}

void vector_set_bits(std::array<std::uint64_t, 2>& qreg, std::uint32_t lsb, std::uint32_t width, std::uint64_t value) {
  if (width == 0u || width > 64u || lsb >= 128u) {
    return;
  }
  value &= ones(width);
  if (lsb >= 64u) {
    const std::uint32_t off = lsb - 64u;
    const std::uint64_t mask = (width == 64u) ? ~0ull : (ones(width) << off);
    qreg[1] = (qreg[1] & ~mask) | ((value << off) & mask);
    return;
  }
  if (lsb + width <= 64u) {
    const std::uint64_t mask = (width == 64u) ? ~0ull : (ones(width) << lsb);
    qreg[0] = (qreg[0] & ~mask) | ((value << lsb) & mask);
    return;
  }
  const std::uint32_t low_bits = 64u - lsb;
  const std::uint32_t high_bits = width - low_bits;
  const std::uint64_t low_mask = ones(low_bits) << lsb;
  const std::uint64_t high_mask = ones(high_bits);
  qreg[0] = (qreg[0] & ~low_mask) | ((value & ones(low_bits)) << lsb);
  qreg[1] = (qreg[1] & ~high_mask) | ((value >> low_bits) & high_mask);
}

std::uint64_t vector_get_elem(const std::array<std::uint64_t, 2>& qreg, std::uint32_t esize_bits, std::uint32_t index) {
  return vector_get_bits(qreg, esize_bits * index, esize_bits);
}

void vector_set_elem(std::array<std::uint64_t, 2>& qreg,
                     std::uint32_t esize_bits,
                     std::uint32_t index,
                     std::uint64_t value) {
  vector_set_bits(qreg, esize_bits * index, esize_bits, value);
}

std::uint32_t highest_set_bit(std::uint32_t value) {
  for (std::uint32_t bit = 31; bit != static_cast<std::uint32_t>(-1); --bit) {
    if (((value >> bit) & 1u) != 0u) {
      return bit;
    }
  }
  return 0u;
}

constexpr std::uint64_t kFpsrIoc = 1ull << 0;
constexpr std::uint64_t kFpsrDzc = 1ull << 1;
constexpr std::uint64_t kFpsrOfc = 1ull << 2;
constexpr std::uint64_t kFpsrUfc = 1ull << 3;
constexpr std::uint64_t kFpsrIxc = 1ull << 4;
constexpr std::uint64_t kFpsrIdc = 1ull << 7;
constexpr std::uint64_t kFpsrQc = 1ull << 27;

enum class FpToIntRoundingMode {
  TieEven,
  PosInf,
  NegInf,
  Zero,
  TieAway,
};

template <typename UIntT>
constexpr UIntT fp_sign_mask_bits();

enum class HostFpBinaryOp {
  Add,
  Sub,
  Mul,
  Div,
};

int fpcr_rounding_mode_to_host(std::uint32_t fpcr_mode) {
  switch (fpcr_mode & 0x3u) {
    case 0u: return FE_TONEAREST;
    case 1u: return FE_UPWARD;
    case 2u: return FE_DOWNWARD;
    case 3u: return FE_TOWARDZERO;
  }
  return FE_TONEAREST;
}

constexpr std::uint32_t kFpcrCtlDn = 1u << 2;
constexpr std::uint32_t kFpcrCtlFz = 1u << 3;
constexpr std::uint32_t kFpcrCtlAhp = 1u << 4;

inline bool fpcr_default_nan_enabled(std::uint32_t fpcr_mode) {
  return (fpcr_mode & kFpcrCtlDn) != 0u;
}

inline bool fpcr_flush_to_zero_enabled(std::uint32_t fpcr_mode) {
  return (fpcr_mode & kFpcrCtlFz) != 0u;
}

inline bool fpcr_alternative_half_enabled(std::uint32_t fpcr_mode) {
  return (fpcr_mode & kFpcrCtlAhp) != 0u;
}

template <typename UIntT>
std::optional<UIntT> fp_process_nan_binary(UIntT lhs_bits,
                                           UIntT rhs_bits,
                                           std::uint32_t fpcr_mode,
                                           std::uint64_t& fpsr_bits);

template <typename UIntT>
std::optional<UIntT> fp_process_nan_ternary(UIntT op1_bits,
                                            UIntT op2_bits,
                                            UIntT op3_bits,
                                            std::uint32_t fpcr_mode,
                                            std::uint64_t& fpsr_bits);

template <typename UIntT>
UIntT fp_flush_input_denormal_bits(UIntT bits,
                                   std::uint32_t fpcr_mode,
                                   std::uint64_t& fpsr_bits);

template <typename UIntT>
UIntT fp_flush_output_denormal_bits(UIntT bits,
                                    std::uint32_t fpcr_mode,
                                    std::uint64_t& fpsr_bits);

template <typename UIntT>
constexpr UIntT fp_sign_mask_bits();

template <typename UIntT>
constexpr UIntT fp_exp_mask_bits();

template <typename UIntT>
constexpr UIntT fp_frac_mask_bits();

template <typename UIntT>
constexpr std::uint32_t fp_frac_bits_count();

template <typename UIntT>
constexpr std::uint32_t fp_exp_bits_count();

template <typename UIntT>
constexpr int fp_exp_bias();

template <typename UIntT>
struct ExactFpValue {
  bool finite = true;
  bool zero = true;
  bool negative = false;
  BigUInt magnitude{};
  int exp2 = 0;
};

template <typename UIntT>
constexpr UIntT fp_zero_bits(bool sign);

template <typename UIntT>
bool fp_add_sub_tiny_before_round(UIntT lhs_bits,
                                  UIntT rhs_bits,
                                  bool subtract_rhs,
                                  bool* negative_result);

template <typename UIntT>
bool fp_mul_tiny_before_round(UIntT lhs_bits, UIntT rhs_bits, bool* negative_result);

template <typename UIntT>
bool fp_div_tiny_before_round(UIntT lhs_bits, UIntT rhs_bits, bool* negative_result);

template <typename UIntT>
bool fp_fma_tiny_before_round(UIntT lhs_bits,
                              UIntT rhs_bits,
                              UIntT addend_bits,
                              bool* negative_result);

template <typename UIntT>
bool fp_sqrt_tiny_before_round(UIntT bits);

std::uint64_t host_fp_exceptions_to_fpsr(int exceptions) {
  std::uint64_t fpsr_bits = 0u;
  if ((exceptions & FE_INVALID) != 0) {
    fpsr_bits |= kFpsrIoc;
  }
  if ((exceptions & FE_DIVBYZERO) != 0) {
    fpsr_bits |= kFpsrDzc;
  }
  if ((exceptions & FE_OVERFLOW) != 0) {
    fpsr_bits |= kFpsrOfc;
  }
  if ((exceptions & FE_UNDERFLOW) != 0) {
    fpsr_bits |= kFpsrUfc;
  }
  if ((exceptions & FE_INEXACT) != 0) {
    fpsr_bits |= kFpsrIxc;
  }
  return fpsr_bits;
}

template <typename FloatT, typename Fn>
FloatT host_fp_eval(std::uint32_t fpcr_mode, int& exceptions, Fn&& fn) {
  fenv_t saved_env{};
  std::fegetenv(&saved_env);
  std::fesetround(fpcr_rounding_mode_to_host(fpcr_mode));
  std::feclearexcept(FE_ALL_EXCEPT);
  volatile FloatT result = fn();
  exceptions = std::fetestexcept(FE_ALL_EXCEPT);
  std::fesetenv(&saved_env);
  return static_cast<FloatT>(result);
}

template <typename FloatT>
FloatT host_sqrt_runtime(FloatT value);

template <>
float host_sqrt_runtime<float>(float value) {
  const auto fn = static_cast<float (*)(float)>(std::sqrt);
  return fn(value);
}

template <>
double host_sqrt_runtime<double>(double value) {
  const auto fn = static_cast<double (*)(double)>(std::sqrt);
  return fn(value);
}

template <typename UIntT, typename FloatT>
UIntT fp_binary_arith_bits(UIntT lhs_bits,
                           UIntT rhs_bits,
                           std::uint32_t fpcr_mode,
                           HostFpBinaryOp op,
                           std::uint64_t& fpsr_bits) {
  fpsr_bits = 0u;
  if (const auto nan = fp_process_nan_binary(lhs_bits, rhs_bits, fpcr_mode, fpsr_bits); nan.has_value()) {
    return *nan;
  }
  lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
  rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
  if (fpcr_flush_to_zero_enabled(fpcr_mode)) {
    bool negative_result = false;
    bool tiny = false;
    switch (op) {
      case HostFpBinaryOp::Add:
        tiny = fp_add_sub_tiny_before_round(lhs_bits, rhs_bits, false, &negative_result);
        break;
      case HostFpBinaryOp::Sub:
        tiny = fp_add_sub_tiny_before_round(lhs_bits, rhs_bits, true, &negative_result);
        break;
      case HostFpBinaryOp::Mul:
        tiny = fp_mul_tiny_before_round(lhs_bits, rhs_bits, &negative_result);
        break;
      case HostFpBinaryOp::Div:
        tiny = fp_div_tiny_before_round(lhs_bits, rhs_bits, &negative_result);
        break;
    }
    if (tiny) {
      fpsr_bits |= kFpsrUfc;
      return fp_zero_bits<UIntT>(negative_result);
    }
  }
  const FloatT lhs = std::bit_cast<FloatT>(lhs_bits);
  const FloatT rhs = std::bit_cast<FloatT>(rhs_bits);
  int exceptions = 0;
  const FloatT result = host_fp_eval<FloatT>(fpcr_mode, exceptions, [&]() {
    volatile FloatT a = lhs;
    volatile FloatT b = rhs;
    switch (op) {
      case HostFpBinaryOp::Add:
        return static_cast<FloatT>(a + b);
      case HostFpBinaryOp::Sub:
        return static_cast<FloatT>(a - b);
      case HostFpBinaryOp::Mul:
        return static_cast<FloatT>(a * b);
      case HostFpBinaryOp::Div:
        return static_cast<FloatT>(a / b);
    }
    return lhs;
  });
  fpsr_bits |= host_fp_exceptions_to_fpsr(exceptions);
  UIntT result_bits = std::bit_cast<UIntT>(result);
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

template <typename UIntT, typename FloatT>
UIntT fp_mulx_bits(UIntT lhs_bits, UIntT rhs_bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  fpsr_bits = 0u;
  if (const auto nan = fp_process_nan_binary(lhs_bits, rhs_bits, fpcr_mode, fpsr_bits); nan.has_value()) {
    return *nan;
  }
  lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
  rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
  if (fpcr_flush_to_zero_enabled(fpcr_mode)) {
    bool negative_result = false;
    if (fp_mul_tiny_before_round(lhs_bits, rhs_bits, &negative_result)) {
      fpsr_bits |= kFpsrUfc;
      return fp_zero_bits<UIntT>(negative_result);
    }
  }
  const UIntT sign_mask = fp_sign_mask_bits<UIntT>();
  const UIntT exp_mask = []() constexpr -> UIntT {
    if constexpr (sizeof(UIntT) == 4u) {
      return 0x7F800000u;
    } else {
      return 0x7FF0000000000000ull;
    }
  }();
  const UIntT frac_mask = []() constexpr -> UIntT {
    if constexpr (sizeof(UIntT) == 4u) {
      return 0x007FFFFFu;
    } else {
      return 0x000FFFFFFFFFFFFFull;
    }
  }();
  const bool lhs_zero = (lhs_bits & ~sign_mask) == 0u;
  const bool rhs_zero = (rhs_bits & ~sign_mask) == 0u;
  const bool lhs_inf = (lhs_bits & exp_mask) == exp_mask && (lhs_bits & frac_mask) == 0u;
  const bool rhs_inf = (rhs_bits & exp_mask) == exp_mask && (rhs_bits & frac_mask) == 0u;
  if ((lhs_zero && rhs_inf) || (rhs_zero && lhs_inf)) {
    UIntT result_bits = std::bit_cast<UIntT>(static_cast<FloatT>(2.0));
    if (((lhs_bits ^ rhs_bits) & sign_mask) != 0u) {
      result_bits |= sign_mask;
    }
    return result_bits;
  }
  int exceptions = 0;
  const FloatT lhs = std::bit_cast<FloatT>(lhs_bits);
  const FloatT rhs = std::bit_cast<FloatT>(rhs_bits);
  const FloatT result = host_fp_eval<FloatT>(fpcr_mode, exceptions, [&]() {
    volatile FloatT a = lhs;
    volatile FloatT b = rhs;
    return static_cast<FloatT>(a * b);
  });
  fpsr_bits |= host_fp_exceptions_to_fpsr(exceptions);
  UIntT result_bits = std::bit_cast<UIntT>(result);
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

template <typename UIntT, typename FloatT>
UIntT fp_fma_bits(UIntT lhs_bits,
                  UIntT rhs_bits,
                  UIntT addend_bits,
                  std::uint32_t fpcr_mode,
                  bool negate_product,
                  bool negate_addend,
                  std::uint64_t& fpsr_bits) {
  if (negate_product) {
    lhs_bits ^= fp_sign_mask_bits<UIntT>();
  }
  if (negate_addend) {
    addend_bits ^= fp_sign_mask_bits<UIntT>();
  }
  fpsr_bits = 0u;
  if (const auto nan = fp_process_nan_ternary(lhs_bits, rhs_bits, addend_bits, fpcr_mode, fpsr_bits);
      nan.has_value()) {
    return *nan;
  }
  lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
  rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
  addend_bits = fp_flush_input_denormal_bits(addend_bits, fpcr_mode, fpsr_bits);
  if (fpcr_flush_to_zero_enabled(fpcr_mode)) {
    bool negative_result = false;
    if (fp_fma_tiny_before_round(lhs_bits, rhs_bits, addend_bits, &negative_result)) {
      fpsr_bits |= kFpsrUfc;
      return fp_zero_bits<UIntT>(negative_result);
    }
  }
  const FloatT lhs = std::bit_cast<FloatT>(lhs_bits);
  const FloatT rhs = std::bit_cast<FloatT>(rhs_bits);
  const FloatT addend = std::bit_cast<FloatT>(addend_bits);
  int exceptions = 0;
  const FloatT result = host_fp_eval<FloatT>(fpcr_mode, exceptions, [&]() {
    volatile FloatT a = lhs;
    volatile FloatT b = rhs;
    volatile FloatT c = addend;
    return static_cast<FloatT>(std::fma(static_cast<FloatT>(a),
                                        static_cast<FloatT>(b),
                                        static_cast<FloatT>(c)));
  });
  fpsr_bits |= host_fp_exceptions_to_fpsr(exceptions);
  UIntT result_bits = std::bit_cast<UIntT>(result);
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

template <typename UIntT>
bool fp_is_nan_bits(UIntT bits) {
  if constexpr (sizeof(UIntT) == 2u) {
    return (bits & 0x7C00u) == 0x7C00u && (bits & 0x03FFu) != 0u;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0u;
  } else {
    return (bits & 0x7FF0000000000000ull) == 0x7FF0000000000000ull &&
           (bits & 0x000FFFFFFFFFFFFFull) != 0u;
  }
}

template <typename UIntT>
bool fp_is_signaling_nan_bits(UIntT bits) {
  if (!fp_is_nan_bits(bits)) {
    return false;
  }
  if constexpr (sizeof(UIntT) == 2u) {
    return (bits & 0x0200u) == 0u;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return (bits & 0x00400000u) == 0u;
  } else {
    return (bits & 0x0008000000000000ull) == 0u;
  }
}

template <typename UIntT>
std::uint64_t fp_compare_fpsr_bits(UIntT lhs_bits, UIntT rhs_bits, bool signaling_compare) {
  const bool lhs_nan = fp_is_nan_bits(lhs_bits);
  const bool rhs_nan = fp_is_nan_bits(rhs_bits);
  if (fp_is_signaling_nan_bits(lhs_bits) || fp_is_signaling_nan_bits(rhs_bits) ||
      (signaling_compare && (lhs_nan || rhs_nan))) {
    return kFpsrIoc;
  }
  return 0u;
}

template <typename UIntT>
constexpr UIntT fp_sign_mask_bits() {
  if constexpr (sizeof(UIntT) == 2u) {
    return 0x8000u;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return 0x80000000u;
  } else {
    return 0x8000000000000000ull;
  }
}

template <typename UIntT>
constexpr UIntT fp_exp_mask_bits() {
  if constexpr (sizeof(UIntT) == 2u) {
    return 0x7C00u;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return 0x7F800000u;
  } else {
    return 0x7FF0000000000000ull;
  }
}

template <typename UIntT>
constexpr UIntT fp_frac_mask_bits() {
  if constexpr (sizeof(UIntT) == 2u) {
    return 0x03FFu;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return 0x007FFFFFu;
  } else {
    return 0x000FFFFFFFFFFFFFull;
  }
}

template <typename UIntT>
constexpr std::uint32_t fp_frac_bits_count() {
  if constexpr (sizeof(UIntT) == 2u) {
    return 10u;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return 23u;
  } else {
    return 52u;
  }
}

template <typename UIntT>
constexpr UIntT fp_quiet_nan_bit() {
  if constexpr (sizeof(UIntT) == 2u) {
    return 0x0200u;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return 0x00400000u;
  } else {
    return 0x0008000000000000ull;
  }
}

template <typename UIntT>
UIntT fp_quiet_nan_bits(UIntT bits) {
  return bits | fp_quiet_nan_bit<UIntT>();
}

template <typename UIntT>
constexpr UIntT fp_default_nan_bits() {
  return fp_exp_mask_bits<UIntT>() | fp_quiet_nan_bit<UIntT>();
}

template <typename UIntT>
UIntT fp_process_nan_operand_bits(UIntT bits,
                                  std::uint32_t fpcr_mode,
                                  bool raise_invalid,
                                  std::uint64_t& fpsr_bits) {
  if (fp_is_signaling_nan_bits(bits)) {
    if (raise_invalid) {
      fpsr_bits |= kFpsrIoc;
    }
    return fpcr_default_nan_enabled(fpcr_mode) ? fp_default_nan_bits<UIntT>()
                                               : fp_quiet_nan_bits(bits);
  }
  if (fp_is_nan_bits(bits)) {
    return fpcr_default_nan_enabled(fpcr_mode) ? fp_default_nan_bits<UIntT>() : bits;
  }
  return bits;
}

template <typename UIntT>
std::optional<UIntT> fp_process_nan_unary(UIntT bits,
                                          std::uint32_t fpcr_mode,
                                          bool raise_invalid,
                                          std::uint64_t& fpsr_bits) {
  if (!fp_is_nan_bits(bits)) {
    return std::nullopt;
  }
  return fp_process_nan_operand_bits(bits, fpcr_mode, raise_invalid, fpsr_bits);
}

template <typename UIntT>
std::optional<UIntT> fp_process_nan_binary(UIntT lhs_bits,
                                           UIntT rhs_bits,
                                           std::uint32_t fpcr_mode,
                                           std::uint64_t& fpsr_bits) {
  if (fp_is_signaling_nan_bits(lhs_bits)) {
    return fp_process_nan_operand_bits(lhs_bits, fpcr_mode, true, fpsr_bits);
  }
  if (fp_is_signaling_nan_bits(rhs_bits)) {
    return fp_process_nan_operand_bits(rhs_bits, fpcr_mode, true, fpsr_bits);
  }
  if (fp_is_nan_bits(lhs_bits)) {
    return fp_process_nan_operand_bits(lhs_bits, fpcr_mode, false, fpsr_bits);
  }
  if (fp_is_nan_bits(rhs_bits)) {
    return fp_process_nan_operand_bits(rhs_bits, fpcr_mode, false, fpsr_bits);
  }
  return std::nullopt;
}

template <typename UIntT>
std::optional<UIntT> fp_process_nan_ternary(UIntT op1_bits,
                                            UIntT op2_bits,
                                            UIntT op3_bits,
                                            std::uint32_t fpcr_mode,
                                            std::uint64_t& fpsr_bits) {
  if (fp_is_signaling_nan_bits(op1_bits)) {
    return fp_process_nan_operand_bits(op1_bits, fpcr_mode, true, fpsr_bits);
  }
  if (fp_is_signaling_nan_bits(op2_bits)) {
    return fp_process_nan_operand_bits(op2_bits, fpcr_mode, true, fpsr_bits);
  }
  if (fp_is_signaling_nan_bits(op3_bits)) {
    return fp_process_nan_operand_bits(op3_bits, fpcr_mode, true, fpsr_bits);
  }
  if (fp_is_nan_bits(op1_bits)) {
    return fp_process_nan_operand_bits(op1_bits, fpcr_mode, false, fpsr_bits);
  }
  if (fp_is_nan_bits(op2_bits)) {
    return fp_process_nan_operand_bits(op2_bits, fpcr_mode, false, fpsr_bits);
  }
  if (fp_is_nan_bits(op3_bits)) {
    return fp_process_nan_operand_bits(op3_bits, fpcr_mode, false, fpsr_bits);
  }
  return std::nullopt;
}

template <typename UIntT>
constexpr std::uint32_t fp_exp_bits_count() {
  if constexpr (sizeof(UIntT) == 2u) {
    return 5u;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return 8u;
  } else {
    return 11u;
  }
}

template <typename UIntT>
constexpr int fp_exp_bias() {
  if constexpr (sizeof(UIntT) == 2u) {
    return 15;
  } else if constexpr (sizeof(UIntT) == 4u) {
    return 127;
  } else {
    return 1023;
  }
}

template <typename UIntT>
bool fp_is_inf_bits(UIntT bits) {
  return (bits & fp_exp_mask_bits<UIntT>()) == fp_exp_mask_bits<UIntT>() &&
         (bits & fp_frac_mask_bits<UIntT>()) == 0u;
}

template <typename UIntT>
constexpr UIntT fp_inf_bits(bool sign) {
  return (sign ? fp_sign_mask_bits<UIntT>() : 0u) | fp_exp_mask_bits<UIntT>();
}

template <typename UIntT>
constexpr UIntT fp_zero_bits(bool sign) {
  return sign ? fp_sign_mask_bits<UIntT>() : 0u;
}

template <typename UIntT>
bool fp_is_subnormal_bits(UIntT bits) {
  return (bits & fp_exp_mask_bits<UIntT>()) == 0u &&
         (bits & fp_frac_mask_bits<UIntT>()) != 0u;
}

template <typename UIntT>
UIntT fp_flush_input_denormal_bits(UIntT bits,
                                   std::uint32_t fpcr_mode,
                                   std::uint64_t& fpsr_bits) {
  if (fpcr_flush_to_zero_enabled(fpcr_mode) && fp_is_subnormal_bits(bits)) {
    fpsr_bits |= kFpsrIdc;
    return fp_zero_bits<UIntT>((bits & fp_sign_mask_bits<UIntT>()) != 0u);
  }
  return bits;
}

template <typename UIntT>
UIntT fp_flush_output_denormal_bits(UIntT bits,
                                    std::uint32_t fpcr_mode,
                                    std::uint64_t& fpsr_bits) {
  if (fpcr_flush_to_zero_enabled(fpcr_mode) && fp_is_subnormal_bits(bits)) {
    fpsr_bits &= ~kFpsrIxc;
    fpsr_bits |= kFpsrUfc;
    return fp_zero_bits<UIntT>((bits & fp_sign_mask_bits<UIntT>()) != 0u);
  }
  return bits;
}

template <typename UIntT>
ExactFpValue<UIntT> fp_exact_value_bits(UIntT bits) {
  ExactFpValue<UIntT> value{};
  value.negative = (bits & fp_sign_mask_bits<UIntT>()) != 0u;
  const std::uint32_t frac_bits = fp_frac_bits_count<UIntT>();
  const std::uint32_t exp_bits = fp_exp_bits_count<UIntT>();
  const UIntT exp_mask = fp_exp_mask_bits<UIntT>();
  const UIntT frac_mask = fp_frac_mask_bits<UIntT>();
  const std::uint32_t exp = static_cast<std::uint32_t>((bits & exp_mask) >> frac_bits);
  const UIntT frac = bits & frac_mask;
  if (exp == ((1u << exp_bits) - 1u)) {
    value.finite = false;
    return value;
  }
  if (exp == 0u) {
    if (frac == 0u) {
      return value;
    }
    value.zero = false;
    value.magnitude = BigUInt{static_cast<std::uint64_t>(frac)};
    value.exp2 = 1 - fp_exp_bias<UIntT>() - static_cast<int>(frac_bits);
    return value;
  }
  value.zero = false;
  BigUInt implicit{1u};
  implicit.shift_left(frac_bits);
  value.magnitude = implicit + BigUInt{static_cast<std::uint64_t>(frac)};
  value.exp2 = static_cast<int>(exp) - fp_exp_bias<UIntT>() - static_cast<int>(frac_bits);
  return value;
}

BigUInt exact_scale_pow2(BigUInt value, unsigned shift) {
  value.shift_left(shift);
  return value;
}

bool exact_abs_lt_power_of_two(const BigUInt& magnitude, int exp2, int threshold_exp2) {
  if (magnitude.is_zero()) {
    return false;
  }
  if (exp2 >= threshold_exp2) {
    return false;
  }
  return magnitude.msb_index() < static_cast<unsigned>(threshold_exp2 - exp2);
}

template <typename UIntT>
bool fp_exact_tiny_sum_terms(bool lhs_negative,
                             const BigUInt& lhs_magnitude,
                             int lhs_exp2,
                             bool rhs_negative,
                             const BigUInt& rhs_magnitude,
                             int rhs_exp2,
                             bool* negative_result) {
  const int common_exp2 = std::min(lhs_exp2, rhs_exp2);
  const BigUInt lhs = exact_scale_pow2(lhs_magnitude, static_cast<unsigned>(lhs_exp2 - common_exp2));
  const BigUInt rhs = exact_scale_pow2(rhs_magnitude, static_cast<unsigned>(rhs_exp2 - common_exp2));
  BigUInt magnitude{};
  if (lhs_negative == rhs_negative) {
    magnitude = lhs + rhs;
    *negative_result = lhs_negative;
  } else {
    const int cmp = BigUInt::compare(lhs, rhs);
    if (cmp == 0) {
      return false;
    }
    if (cmp > 0) {
      magnitude = lhs - rhs;
      *negative_result = lhs_negative;
    } else {
      magnitude = rhs - lhs;
      *negative_result = rhs_negative;
    }
  }
  return exact_abs_lt_power_of_two(magnitude, common_exp2, 1 - fp_exp_bias<UIntT>());
}

template <typename UIntT>
bool fp_add_sub_tiny_before_round(UIntT lhs_bits,
                                  UIntT rhs_bits,
                                  bool subtract_rhs,
                                  bool* negative_result) {
  const ExactFpValue<UIntT> lhs = fp_exact_value_bits(lhs_bits);
  const ExactFpValue<UIntT> rhs = fp_exact_value_bits(rhs_bits);
  if (!lhs.finite || !rhs.finite || (lhs.zero && rhs.zero)) {
    return false;
  }
  return fp_exact_tiny_sum_terms<UIntT>(lhs.negative,
                                        lhs.magnitude,
                                        lhs.exp2,
                                        subtract_rhs ? !rhs.negative : rhs.negative,
                                        rhs.magnitude,
                                        rhs.exp2,
                                        negative_result);
}

template <typename UIntT>
bool fp_mul_tiny_before_round(UIntT lhs_bits, UIntT rhs_bits, bool* negative_result) {
  const ExactFpValue<UIntT> lhs = fp_exact_value_bits(lhs_bits);
  const ExactFpValue<UIntT> rhs = fp_exact_value_bits(rhs_bits);
  if (!lhs.finite || !rhs.finite || lhs.zero || rhs.zero) {
    return false;
  }
  *negative_result = lhs.negative != rhs.negative;
  return exact_abs_lt_power_of_two(lhs.magnitude * rhs.magnitude,
                                   lhs.exp2 + rhs.exp2,
                                   1 - fp_exp_bias<UIntT>());
}

template <typename UIntT>
bool fp_div_tiny_before_round(UIntT lhs_bits, UIntT rhs_bits, bool* negative_result) {
  const ExactFpValue<UIntT> lhs = fp_exact_value_bits(lhs_bits);
  const ExactFpValue<UIntT> rhs = fp_exact_value_bits(rhs_bits);
  if (!lhs.finite || !rhs.finite || lhs.zero || rhs.zero) {
    return false;
  }
  *negative_result = lhs.negative != rhs.negative;
  const int delta = lhs.exp2 - rhs.exp2 - (1 - fp_exp_bias<UIntT>());
  if (delta >= 0) {
    return exact_scale_pow2(lhs.magnitude, static_cast<unsigned>(delta)) < rhs.magnitude;
  }
  return lhs.magnitude < exact_scale_pow2(rhs.magnitude, static_cast<unsigned>(-delta));
}

template <typename UIntT>
bool fp_fma_tiny_before_round(UIntT lhs_bits,
                              UIntT rhs_bits,
                              UIntT addend_bits,
                              bool* negative_result) {
  const ExactFpValue<UIntT> lhs = fp_exact_value_bits(lhs_bits);
  const ExactFpValue<UIntT> rhs = fp_exact_value_bits(rhs_bits);
  const ExactFpValue<UIntT> addend = fp_exact_value_bits(addend_bits);
  if (!lhs.finite || !rhs.finite || !addend.finite) {
    return false;
  }
  if (lhs.zero || rhs.zero) {
    if (addend.zero) {
      return false;
    }
    *negative_result = addend.negative;
    return exact_abs_lt_power_of_two(addend.magnitude, addend.exp2, 1 - fp_exp_bias<UIntT>());
  }
  if (addend.zero) {
    *negative_result = lhs.negative != rhs.negative;
    return exact_abs_lt_power_of_two(lhs.magnitude * rhs.magnitude,
                                     lhs.exp2 + rhs.exp2,
                                     1 - fp_exp_bias<UIntT>());
  }
  return fp_exact_tiny_sum_terms<UIntT>(lhs.negative != rhs.negative,
                                        lhs.magnitude * rhs.magnitude,
                                        lhs.exp2 + rhs.exp2,
                                        addend.negative,
                                        addend.magnitude,
                                        addend.exp2,
                                        negative_result);
}

template <typename UIntT>
bool fp_sqrt_tiny_before_round(UIntT bits) {
  const ExactFpValue<UIntT> value = fp_exact_value_bits(bits);
  if (!value.finite || value.zero || value.negative) {
    return false;
  }
  return exact_abs_lt_power_of_two(value.magnitude,
                                   value.exp2,
                                   2 * (1 - fp_exp_bias<UIntT>()));
}

template <typename UIntT>
constexpr UIntT fp_abs_bits(UIntT bits) {
  return bits & ~fp_sign_mask_bits<UIntT>();
}

template <typename UIntT>
constexpr UIntT fp_neg_bits(UIntT bits) {
  return bits ^ fp_sign_mask_bits<UIntT>();
}

template <typename UIntT>
constexpr UIntT fp_max_normal_bits(bool sign) {
  if constexpr (sizeof(UIntT) == 4u) {
    return (sign ? 0x80000000u : 0u) | 0x7F7FFFFFu;
  } else {
    return (sign ? 0x8000000000000000ull : 0u) | 0x7FEFFFFFFFFFFFFFull;
  }
}

template <typename UIntT>
constexpr UIntT fp_two_bits() {
  if constexpr (sizeof(UIntT) == 4u) {
    return 0x40000000u;
  } else {
    return 0x4000000000000000ull;
  }
}

template <typename UIntT>
constexpr UIntT fp_one_point_five_bits() {
  if constexpr (sizeof(UIntT) == 4u) {
    return 0x3FC00000u;
  } else {
    return 0x3FF8000000000000ull;
  }
}

template <typename UIntT>
bool fp_is_exact_sqrt_positive_finite_bits(UIntT bits) {
  static_assert(sizeof(UIntT) == 4u || sizeof(UIntT) == 8u);
  const UIntT exp_mask = fp_exp_mask_bits<UIntT>();
  const UIntT frac_mask = fp_frac_mask_bits<UIntT>();
  const std::uint32_t frac_bits = fp_frac_bits_count<UIntT>();
  const UIntT exp = (bits & exp_mask) >> frac_bits;
  const UIntT frac = bits & frac_mask;
  std::uint64_t significand = 0u;
  int exponent = 0;

  if (exp == 0u) {
    if (frac == 0u) {
      return true;
    }
    significand = static_cast<std::uint64_t>(frac);
    exponent = 1 - fp_exp_bias<UIntT>() - static_cast<int>(frac_bits);
  } else {
    significand = (1ull << frac_bits) | static_cast<std::uint64_t>(frac);
    exponent = static_cast<int>(exp) - fp_exp_bias<UIntT>() - static_cast<int>(frac_bits);
  }

  const unsigned shift = static_cast<unsigned>(__builtin_ctzll(significand));
  significand >>= shift;
  exponent += static_cast<int>(shift);
  if ((exponent & 1) != 0) {
    return false;
  }

  std::uint64_t root = static_cast<std::uint64_t>(std::sqrt(static_cast<long double>(significand)));
  while (true) {
    const std::uint64_t next = root + 1u;
    if (next > significand / next) {
      break;
    }
    if (next * next > significand) {
      break;
    }
    root = next;
  }
  while (root > significand / root || root * root > significand) {
    --root;
  }
  return root <= significand / root && root * root == significand;
}

template <typename UIntT, typename FloatT>
UIntT fp_sqrt_bits(UIntT bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(UIntT) == 4u || sizeof(UIntT) == 8u);
  fpsr_bits = 0u;

  if (const auto nan = fp_process_nan_unary(bits, fpcr_mode, true, fpsr_bits); nan.has_value()) {
    return *nan;
  }

  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  const bool sign = (bits & fp_sign_mask_bits<UIntT>()) != 0u;
  const UIntT abs_bits = bits & ~fp_sign_mask_bits<UIntT>();
  if (abs_bits == 0u) {
    return bits;
  }
  if (fp_is_inf_bits(bits)) {
    if (!sign) {
      return bits;
    }
    fpsr_bits |= kFpsrIoc;
    return fp_default_nan_bits<UIntT>();
  }
  if (sign) {
    fpsr_bits |= kFpsrIoc;
    return fp_default_nan_bits<UIntT>();
  }
  if (fpcr_flush_to_zero_enabled(fpcr_mode) && fp_sqrt_tiny_before_round(bits)) {
    fpsr_bits |= kFpsrUfc;
    return fp_zero_bits<UIntT>(false);
  }

  const FloatT value = std::bit_cast<FloatT>(bits);
  int exceptions = 0;
  const FloatT result = host_fp_eval<FloatT>(fpcr_mode, exceptions, [&]() {
    volatile FloatT a = value;
    return static_cast<FloatT>(host_sqrt_runtime<FloatT>(static_cast<FloatT>(a)));
  });
  fpsr_bits |= host_fp_exceptions_to_fpsr(exceptions);
  UIntT result_bits = std::bit_cast<UIntT>(result);
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

bool fp_round_overflow_to_inf(std::uint32_t mode, bool sign) {
  switch (mode & 0x3u) {
    case 0u: return true;
    case 1u: return !sign;
    case 2u: return sign;
    default: return false;
  }
}

template <typename UIntT, typename FloatT>
UIntT fp_minmax_result_bits(UIntT lhs_bits,
                            UIntT rhs_bits,
                            std::uint32_t fpcr_mode,
                            bool is_min,
                            bool numeric_variant,
                            std::uint64_t& fpsr_bits) {
  fpsr_bits = 0u;
  lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
  rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
  const bool default_nan = fpcr_default_nan_enabled(fpcr_mode);
  const bool lhs_nan = fp_is_nan_bits(lhs_bits);
  const bool rhs_nan = fp_is_nan_bits(rhs_bits);
  const bool lhs_snan = fp_is_signaling_nan_bits(lhs_bits);
  const bool rhs_snan = fp_is_signaling_nan_bits(rhs_bits);
  const auto finalize = [&](UIntT result_bits) -> UIntT {
    if (numeric_variant) {
      result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
    }
    return result_bits;
  };

  if (lhs_snan) {
    fpsr_bits |= kFpsrIoc;
    return finalize(default_nan ? fp_default_nan_bits<UIntT>() : fp_quiet_nan_bits(lhs_bits));
  }
  if (rhs_snan) {
    fpsr_bits |= kFpsrIoc;
    return finalize(default_nan ? fp_default_nan_bits<UIntT>() : fp_quiet_nan_bits(rhs_bits));
  }

  if (numeric_variant) {
    if (lhs_nan && rhs_nan) {
      return finalize(default_nan ? fp_default_nan_bits<UIntT>() : lhs_bits);
    }
    if (lhs_nan) {
      return finalize(rhs_bits);
    }
    if (rhs_nan) {
      return finalize(lhs_bits);
    }
  } else {
    if (lhs_nan || rhs_nan) {
      if (default_nan) {
        return finalize(fp_default_nan_bits<UIntT>());
      }
      return finalize(lhs_nan ? lhs_bits : rhs_bits);
    }
  }

  const FloatT lhs = std::bit_cast<FloatT>(lhs_bits);
  const FloatT rhs = std::bit_cast<FloatT>(rhs_bits);
  if (lhs < rhs) {
    return finalize(is_min ? lhs_bits : rhs_bits);
  }
  if (lhs > rhs) {
    return finalize(is_min ? rhs_bits : lhs_bits);
  }

  const UIntT sign_mask = fp_sign_mask_bits<UIntT>();
  const bool lhs_zero = (lhs_bits & ~sign_mask) == 0u;
  const bool rhs_zero = (rhs_bits & ~sign_mask) == 0u;
  if (lhs_zero && rhs_zero) {
    return finalize(is_min ? (lhs_bits | rhs_bits) : (lhs_bits & rhs_bits));
  }
  return finalize(lhs_bits);
}

std::uint64_t fp_round_shift_right(std::uint64_t value,
                                   std::uint32_t shift,
                                   std::uint32_t mode,
                                   bool sign,
                                   bool& inexact) {
  if (shift == 0u) {
    inexact = false;
    return value;
  }
  if (value == 0u) {
    inexact = false;
    return 0u;
  }
  if (shift >= 128u) {
    inexact = true;
    switch (mode & 0x3u) {
      case 1u: return sign ? 0u : 1u;
      case 2u: return sign ? 1u : 0u;
      default: return 0u;
    }
  }

  std::uint64_t quotient = 0u;
  std::uint64_t remainder = 0u;
  if (shift >= 64u) {
    quotient = 0u;
    remainder = value;
  } else {
    quotient = value >> shift;
    remainder = value & ones(shift);
  }
  inexact = remainder != 0u;
  if (!inexact) {
    return quotient;
  }

  bool round_up = false;
  switch (mode & 0x3u) {
    case 0u: {
      if (shift < 64u) {
        const std::uint64_t half = 1ull << (shift - 1u);
        round_up = remainder > half || (remainder == half && (quotient & 1u) != 0u);
      } else {
        round_up = false;
      }
      break;
    }
    case 1u:
      round_up = !sign;
      break;
    case 2u:
      round_up = sign;
      break;
    case 3u:
      round_up = false;
      break;
  }
  return quotient + (round_up ? 1u : 0u);
}

std::uint32_t fp64_to_fp32_bits(std::uint64_t bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  fpsr_bits = 0u;
  bool sign = (bits >> 63u) != 0u;
  std::uint32_t sign_bit = sign ? 0x80000000u : 0u;
  std::uint32_t exp = static_cast<std::uint32_t>((bits >> 52u) & 0x7FFu);
  std::uint64_t frac = bits & 0x000FFFFFFFFFFFFFull;

  if (exp == 0x7FFu) {
    if (frac == 0u) {
      return sign_bit | 0x7F800000u;
    }
    if (fpcr_default_nan_enabled(fpcr_mode)) {
      if (fp_is_signaling_nan_bits(bits)) {
        fpsr_bits |= kFpsrIoc;
      }
      return fp_default_nan_bits<std::uint32_t>();
    }
    if (fp_is_signaling_nan_bits(bits)) {
      fpsr_bits |= kFpsrIoc;
    }
    std::uint32_t payload = static_cast<std::uint32_t>(frac >> 29u);
    payload |= 0x00400000u;
    if ((payload & 0x003FFFFFu) == 0u) {
      payload |= 1u;
    }
    return sign_bit | 0x7F800000u | (payload & 0x007FFFFFu);
  }

  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  sign = (bits >> 63u) != 0u;
  sign_bit = sign ? 0x80000000u : 0u;
  exp = static_cast<std::uint32_t>((bits >> 52u) & 0x7FFu);
  frac = bits & 0x000FFFFFFFFFFFFFull;

  if (exp == 0u && frac == 0u) {
    return sign_bit;
  }

  std::uint64_t significand = 0u;
  int exponent = 0;
  if (exp == 0u) {
    const int msb = 63 - __builtin_clzll(frac);
    const int normalize_shift = 52 - msb;
    significand = frac << normalize_shift;
    exponent = -1022 - normalize_shift;
  } else {
    significand = (1ull << 52u) | frac;
    exponent = static_cast<int>(exp) - 1023;
  }

  const auto overflow_result = [&]() -> std::uint32_t {
    fpsr_bits |= kFpsrOfc | kFpsrIxc;
    const bool to_inf =
        (fpcr_mode & 0x3u) == 0u ||
        ((fpcr_mode & 0x3u) == 1u && !sign) ||
        ((fpcr_mode & 0x3u) == 2u && sign);
    return sign_bit | (to_inf ? 0x7F800000u : 0x7F7FFFFFu);
  };

  if (exponent > 127) {
    return overflow_result();
  }

  // With AH effectively 0 in the current model, Arm requires FZ to detect
  // denormalized single-precision outputs before rounding is applied. Without
  // this, values just below the min-normal boundary would incorrectly round up
  // to 0x00800000 instead of flushing to signed zero.
  if (fpcr_flush_to_zero_enabled(fpcr_mode) && exponent < -126) {
    fpsr_bits |= kFpsrUfc;
    return sign_bit;
  }

  bool inexact = false;
  if (exponent >= -126) {
    std::uint64_t rounded = fp_round_shift_right(significand, 29u, fpcr_mode, sign, inexact);
    if (rounded == (1ull << 24u)) {
      rounded >>= 1u;
      ++exponent;
    }
    if (exponent > 127) {
      return overflow_result();
    }
    if (inexact) {
      fpsr_bits |= kFpsrIxc;
    }
    const std::uint32_t result_bits = sign_bit |
                                      (static_cast<std::uint32_t>(exponent + 127) << 23u) |
                                      (static_cast<std::uint32_t>(rounded) & 0x007FFFFFu);
    return fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  }

  const std::uint32_t sub_shift = 29u + static_cast<std::uint32_t>(-126 - exponent);
  std::uint64_t rounded = fp_round_shift_right(significand, sub_shift, fpcr_mode, sign, inexact);
  if (rounded >= (1ull << 23u)) {
    if (inexact) {
      fpsr_bits |= kFpsrUfc | kFpsrIxc;
    }
    return sign_bit | 0x00800000u;
  }
  if (inexact) {
    fpsr_bits |= kFpsrUfc | kFpsrIxc;
  }
  const std::uint32_t result_bits = sign_bit | static_cast<std::uint32_t>(rounded);
  return fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
}

std::uint32_t fp64_to_fp32_bits_round_to_odd(std::uint64_t bits,
                                             std::uint32_t fpcr_mode,
                                             std::uint64_t& fpsr_bits) {
  fpsr_bits = 0u;
  bool sign = (bits >> 63u) != 0u;
  std::uint32_t sign_bit = sign ? 0x80000000u : 0u;
  std::uint32_t exp = static_cast<std::uint32_t>((bits >> 52u) & 0x7FFu);
  std::uint64_t frac = bits & 0x000FFFFFFFFFFFFFull;

  if (exp == 0x7FFu) {
    if (frac == 0u) {
      return sign_bit | 0x7F800000u;
    }
    if (fpcr_default_nan_enabled(fpcr_mode)) {
      if (fp_is_signaling_nan_bits(bits)) {
        fpsr_bits |= kFpsrIoc;
      }
      return fp_default_nan_bits<std::uint32_t>();
    }
    if (fp_is_signaling_nan_bits(bits)) {
      fpsr_bits |= kFpsrIoc;
    }
    std::uint32_t payload = static_cast<std::uint32_t>(frac >> 29u);
    payload |= 0x00400000u;
    if ((payload & 0x003FFFFFu) == 0u) {
      payload |= 1u;
    }
    return sign_bit | 0x7F800000u | (payload & 0x007FFFFFu);
  }

  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  sign = (bits >> 63u) != 0u;
  sign_bit = sign ? 0x80000000u : 0u;
  exp = static_cast<std::uint32_t>((bits >> 52u) & 0x7FFu);
  frac = bits & 0x000FFFFFFFFFFFFFull;

  if (exp == 0u && frac == 0u) {
    return sign_bit;
  }

  std::uint64_t significand = 0u;
  int exponent = 0;
  if (exp == 0u) {
    const int msb = 63 - __builtin_clzll(frac);
    const int normalize_shift = 52 - msb;
    significand = frac << normalize_shift;
    exponent = -1022 - normalize_shift;
  } else {
    significand = (1ull << 52u) | frac;
    exponent = static_cast<int>(exp) - 1023;
  }

  const auto overflow_result = [&]() -> std::uint32_t {
    fpsr_bits |= kFpsrOfc | kFpsrIxc;
    return sign_bit | 0x7F7FFFFFu;
  };

  const auto round_shift_right_odd = [](std::uint64_t value, std::uint32_t shift, bool& inexact) {
    std::uint64_t quotient = 0u;
    std::uint64_t remainder = 0u;
    if (shift >= 64u) {
      quotient = 0u;
      remainder = value;
    } else {
      quotient = value >> shift;
      remainder = value & ones(shift);
    }
    inexact = remainder != 0u;
    if (inexact && (quotient & 1u) == 0u) {
      ++quotient;
    }
    return quotient;
  };

  if (exponent > 127) {
    return overflow_result();
  }

  // FCVTXN shares the same denormalized-output flushing rule as FCVT:
  // when AH is 0, FZ observes the pre-rounding tiny result rather than the
  // post-rounding single-precision encoding.
  if (fpcr_flush_to_zero_enabled(fpcr_mode) && exponent < -126) {
    fpsr_bits |= kFpsrUfc;
    return sign_bit;
  }

  bool inexact = false;
  if (exponent >= -126) {
    std::uint64_t rounded = round_shift_right_odd(significand, 29u, inexact);
    if (rounded == (1ull << 24u)) {
      rounded >>= 1u;
      ++exponent;
    }
    if (exponent > 127) {
      return overflow_result();
    }
    if (inexact) {
      fpsr_bits |= kFpsrIxc;
    }
    const std::uint32_t result_bits = sign_bit |
                                      (static_cast<std::uint32_t>(exponent + 127) << 23u) |
                                      (static_cast<std::uint32_t>(rounded) & 0x007FFFFFu);
    return fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  }

  const std::uint32_t sub_shift = 29u + static_cast<std::uint32_t>(-126 - exponent);
  const std::uint64_t rounded = round_shift_right_odd(significand, sub_shift, inexact);
  if (inexact) {
    fpsr_bits |= kFpsrUfc | kFpsrIxc;
  }
  const std::uint32_t result_bits = sign_bit | static_cast<std::uint32_t>(rounded);
  return fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
}

std::uint32_t fp_recip_estimate_integer(std::uint32_t a_in, bool increased_precision) {
  std::uint64_t a = a_in;
  if (!increased_precision) {
    a = a * 2u + 1u;
    const std::uint64_t b = (1ull << 19) / a;
    return static_cast<std::uint32_t>((b + 1u) / 2u);
  }
  a = a * 2u + 1u;
  const std::uint64_t b = (1ull << 26) / a;
  return static_cast<std::uint32_t>((b + 1u) / 2u);
}

std::uint32_t fp_rsqrt_estimate_integer(std::uint32_t a_in, bool increased_precision) {
  std::uint64_t a = a_in;
  if (!increased_precision) {
    if (a < 256u) {
      a = a * 2u + 1u;
    } else {
      a &= ~1ull;
      a = (a + 1u) * 2u;
    }
    std::uint64_t b = 512u;
    while (a * (b + 1u) * (b + 1u) < (1ull << 28)) {
      ++b;
    }
    return static_cast<std::uint32_t>((b + 1u) / 2u);
  }
  if (a < 2048u) {
    a = a * 2u + 1u;
  } else {
    a &= ~1ull;
    a = (a + 1u) * 2u;
  }
  std::uint64_t b = 8192u;
  while (a * (b + 1u) * (b + 1u) < (1ull << 39)) {
    ++b;
  }
  return static_cast<std::uint32_t>((b + 1u) / 2u);
}

std::uint32_t advsimd_unsigned_recip_estimate_bits(std::uint32_t operand) {
  if ((operand >> 31u) == 0u) {
    return 0xFFFFFFFFu;
  }
  const std::uint32_t estimate = fp_recip_estimate_integer((operand >> 23u) & 0x1FFu, false);
  return (estimate & 0x1FFu) << 23u;
}

std::uint32_t advsimd_unsigned_rsqrt_estimate_bits(std::uint32_t operand) {
  if (((operand >> 30u) & 0x3u) == 0u) {
    return 0xFFFFFFFFu;
  }
  const std::uint32_t estimate = fp_rsqrt_estimate_integer((operand >> 23u) & 0x1FFu, false);
  return (estimate & 0x1FFu) << 23u;
}

template <typename UIntT>
UIntT fp_recip_estimate_bits(UIntT bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(UIntT) == 4u || sizeof(UIntT) == 8u);
  fpsr_bits = 0u;
  if (const auto nan = fp_process_nan_unary(bits, fpcr_mode, true, fpsr_bits); nan.has_value()) {
    return *nan;
  }
  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  const bool sign = (bits & fp_sign_mask_bits<UIntT>()) != 0u;
  const UIntT sign_bit = sign ? fp_sign_mask_bits<UIntT>() : 0u;
  const UIntT exp = (bits & fp_exp_mask_bits<UIntT>()) >> fp_frac_bits_count<UIntT>();
  const UIntT frac = bits & fp_frac_mask_bits<UIntT>();
  const UIntT exp_all_ones = fp_exp_mask_bits<UIntT>() >> fp_frac_bits_count<UIntT>();

  if (exp == exp_all_ones) {
    if (frac == 0u) {
      return fp_zero_bits<UIntT>(sign);
    }
  }

  if (exp == 0u && frac == 0u) {
    fpsr_bits |= kFpsrDzc;
    return fp_inf_bits<UIntT>(sign);
  }

  if constexpr (sizeof(UIntT) == 4u) {
    if (exp == 0u && frac < (1u << 21u)) {
      fpsr_bits |= kFpsrOfc | kFpsrIxc;
      return fp_round_overflow_to_inf(fpcr_mode, sign) ? fp_inf_bits<UIntT>(sign) : fp_max_normal_bits<UIntT>(sign);
    }
  } else {
    if (exp == 0u && frac < (1ull << 50u)) {
      fpsr_bits |= kFpsrOfc | kFpsrIxc;
      return fp_round_overflow_to_inf(fpcr_mode, sign) ? fp_inf_bits<UIntT>(sign) : fp_max_normal_bits<UIntT>(sign);
    }
  }

  std::uint64_t fraction = 0u;
  int exp_value = static_cast<int>(exp);
  if constexpr (sizeof(UIntT) == 4u) {
    fraction = static_cast<std::uint64_t>(frac) << 29u;
  } else {
    fraction = static_cast<std::uint64_t>(frac);
  }

  if (exp == 0u) {
    if ((fraction & (1ull << 51u)) == 0u) {
      exp_value = -1;
      fraction = (fraction & ((1ull << 50u) - 1u)) << 2u;
    } else {
      fraction = (fraction & ((1ull << 51u) - 1u)) << 1u;
    }
  }

  const bool increased_precision = false;
  const std::uint32_t scaled = increased_precision
      ? (0x800u | static_cast<std::uint32_t>((fraction >> 41u) & 0x7FFu))
      : (0x100u | static_cast<std::uint32_t>((fraction >> 44u) & 0xFFu));
  int result_exp = 0;
  if constexpr (sizeof(UIntT) == 4u) {
    result_exp = 253 - exp_value;
  } else {
    result_exp = 2045 - exp_value;
  }
  const std::uint32_t estimate = fp_recip_estimate_integer(scaled, increased_precision);
  fraction = increased_precision
      ? (static_cast<std::uint64_t>(estimate & 0xFFFu) << 40u)
      : (static_cast<std::uint64_t>(estimate & 0xFFu) << 44u);
  if (result_exp == 0) {
    fraction = (1ull << 51u) | (fraction >> 1u);
  } else if (result_exp == -1) {
    fraction = (1ull << 50u) | (fraction >> 2u);
    result_exp = 0;
  }

  UIntT result_bits = 0u;
  if constexpr (sizeof(UIntT) == 4u) {
    result_bits = sign_bit |
                  (static_cast<UIntT>(result_exp) << 23u) |
                  static_cast<UIntT>((fraction >> 29u) & 0x007FFFFFu);
  } else {
    result_bits = sign_bit |
                  (static_cast<UIntT>(result_exp) << 52u) |
                  static_cast<UIntT>(fraction & 0x000FFFFFFFFFFFFFull);
  }
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

template <typename UIntT>
UIntT fp_rsqrt_estimate_bits(UIntT bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(UIntT) == 4u || sizeof(UIntT) == 8u);
  fpsr_bits = 0u;
  if (const auto nan = fp_process_nan_unary(bits, fpcr_mode, true, fpsr_bits); nan.has_value()) {
    return *nan;
  }
  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  const bool sign = (bits & fp_sign_mask_bits<UIntT>()) != 0u;
  const UIntT exp = (bits & fp_exp_mask_bits<UIntT>()) >> fp_frac_bits_count<UIntT>();
  const UIntT frac = bits & fp_frac_mask_bits<UIntT>();
  const UIntT exp_all_ones = fp_exp_mask_bits<UIntT>() >> fp_frac_bits_count<UIntT>();

  if (exp == exp_all_ones) {
    if (frac == 0u) {
      if (sign) {
        fpsr_bits |= kFpsrIoc;
        return fp_default_nan_bits<UIntT>();
      }
      return fp_zero_bits<UIntT>(false);
    }
  }

  if (exp == 0u && frac == 0u) {
    fpsr_bits |= kFpsrDzc;
    return fp_inf_bits<UIntT>(sign);
  }

  if (sign) {
    fpsr_bits |= kFpsrIoc;
    return fp_default_nan_bits<UIntT>();
  }

  std::uint64_t fraction = 0u;
  int exp_value = static_cast<int>(exp);
  if constexpr (sizeof(UIntT) == 4u) {
    fraction = static_cast<std::uint64_t>(frac) << 29u;
  } else {
    fraction = static_cast<std::uint64_t>(frac);
  }

  if (exp == 0u) {
    while ((fraction & (1ull << 51u)) == 0u) {
      fraction <<= 1u;
      --exp_value;
    }
    fraction <<= 1u;
  }

  const bool increased_precision = false;
  std::uint32_t scaled = 0u;
  if (!increased_precision) {
    if ((exp_value & 1) == 0) {
      scaled = 0x100u | static_cast<std::uint32_t>((fraction >> 44u) & 0xFFu);
    } else {
      scaled = 0x080u | static_cast<std::uint32_t>((fraction >> 45u) & 0x7Fu);
    }
  } else {
    if ((exp_value & 1) == 0) {
      scaled = 0x800u | static_cast<std::uint32_t>((fraction >> 41u) & 0x7FFu);
    } else {
      scaled = 0x400u | static_cast<std::uint32_t>((fraction >> 42u) & 0x3FFu);
    }
  }
  const std::uint32_t estimate = fp_rsqrt_estimate_integer(scaled, increased_precision);
  UIntT result_bits = 0u;
  if constexpr (sizeof(UIntT) == 4u) {
    const int result_exp = (380 - exp_value) / 2;
    result_bits = (static_cast<UIntT>(result_exp) << 23u) |
                  static_cast<UIntT>((estimate & 0xFFu) << 15u);
  } else {
    const int result_exp = (3068 - exp_value) / 2;
    result_bits = (static_cast<UIntT>(result_exp) << 52u) |
                  static_cast<UIntT>(static_cast<std::uint64_t>(estimate & 0xFFu) << 44u);
  }
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

template <typename UIntT, typename FloatT>
UIntT fp_recip_step_fused_bits(UIntT op1_bits_in, UIntT op2_bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(UIntT) == 4u || sizeof(UIntT) == 8u);
  fpsr_bits = 0u;

  const UIntT sign_mask = fp_sign_mask_bits<UIntT>();
  const UIntT exp_mask = fp_exp_mask_bits<UIntT>();
  const UIntT frac_mask = fp_frac_mask_bits<UIntT>();
  const UIntT exp_all_ones = exp_mask >> fp_frac_bits_count<UIntT>();
  const UIntT op1_bits = op1_bits_in ^ sign_mask;

  if (const auto nan = fp_process_nan_binary(op1_bits, op2_bits, fpcr_mode, fpsr_bits); nan.has_value()) {
    return *nan;
  }

  const UIntT lhs_bits = fp_flush_input_denormal_bits(op1_bits, fpcr_mode, fpsr_bits);
  const UIntT rhs_bits = fp_flush_input_denormal_bits(op2_bits, fpcr_mode, fpsr_bits);

  const bool sign1 = (lhs_bits & sign_mask) != 0u;
  const bool sign2 = (rhs_bits & sign_mask) != 0u;
  const UIntT exp1 = (lhs_bits & exp_mask) >> fp_frac_bits_count<UIntT>();
  const UIntT exp2 = (rhs_bits & exp_mask) >> fp_frac_bits_count<UIntT>();
  const UIntT frac1 = lhs_bits & frac_mask;
  const UIntT frac2 = rhs_bits & frac_mask;
  const bool inf1 = exp1 == exp_all_ones && frac1 == 0u;
  const bool inf2 = exp2 == exp_all_ones && frac2 == 0u;
  const bool zero1 = (lhs_bits & ~sign_mask) == 0u;
  const bool zero2 = (rhs_bits & ~sign_mask) == 0u;

  if ((inf1 && zero2) || (zero1 && inf2)) {
    return fp_two_bits<UIntT>();
  }
  if (inf1 || inf2) {
    return fp_inf_bits<UIntT>(sign1 ^ sign2);
  }

  const FloatT op1 = std::bit_cast<FloatT>(lhs_bits);
  const FloatT op2 = std::bit_cast<FloatT>(rhs_bits);
  const long double exact = static_cast<long double>(op1) * static_cast<long double>(op2) + 2.0L;
  if (exact == 0.0L) {
    return fp_zero_bits<UIntT>((fpcr_mode & 0x3u) == 2u);
  }
  int exceptions = 0;
  const FloatT result = host_fp_eval<FloatT>(fpcr_mode, exceptions, [&]() {
    volatile FloatT a = op1;
    volatile FloatT b = op2;
    return static_cast<FloatT>(std::fma(static_cast<FloatT>(a),
                                        static_cast<FloatT>(b),
                                        static_cast<FloatT>(2.0)));
  });
  fpsr_bits |= host_fp_exceptions_to_fpsr(exceptions);
  UIntT result_bits = std::bit_cast<UIntT>(result);
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

template <typename UIntT, typename FloatT>
UIntT fp_rsqrt_step_fused_bits(UIntT op1_bits_in, UIntT op2_bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(UIntT) == 4u || sizeof(UIntT) == 8u);
  fpsr_bits = 0u;

  const UIntT sign_mask = fp_sign_mask_bits<UIntT>();
  const UIntT exp_mask = fp_exp_mask_bits<UIntT>();
  const UIntT frac_mask = fp_frac_mask_bits<UIntT>();
  const UIntT exp_all_ones = exp_mask >> fp_frac_bits_count<UIntT>();
  const UIntT op1_bits = op1_bits_in ^ sign_mask;

  if (const auto nan = fp_process_nan_binary(op1_bits, op2_bits, fpcr_mode, fpsr_bits); nan.has_value()) {
    return *nan;
  }

  const UIntT lhs_bits = fp_flush_input_denormal_bits(op1_bits, fpcr_mode, fpsr_bits);
  const UIntT rhs_bits = fp_flush_input_denormal_bits(op2_bits, fpcr_mode, fpsr_bits);

  const bool sign1 = (lhs_bits & sign_mask) != 0u;
  const bool sign2 = (rhs_bits & sign_mask) != 0u;
  const UIntT exp1 = (lhs_bits & exp_mask) >> fp_frac_bits_count<UIntT>();
  const UIntT exp2 = (rhs_bits & exp_mask) >> fp_frac_bits_count<UIntT>();
  const UIntT frac1 = lhs_bits & frac_mask;
  const UIntT frac2 = rhs_bits & frac_mask;
  const bool inf1 = exp1 == exp_all_ones && frac1 == 0u;
  const bool inf2 = exp2 == exp_all_ones && frac2 == 0u;
  const bool zero1 = (lhs_bits & ~sign_mask) == 0u;
  const bool zero2 = (rhs_bits & ~sign_mask) == 0u;

  if ((inf1 && zero2) || (zero1 && inf2)) {
    return fp_one_point_five_bits<UIntT>();
  }
  if (inf1 || inf2) {
    return fp_inf_bits<UIntT>(sign1 ^ sign2);
  }

  const FloatT op1 = std::bit_cast<FloatT>(lhs_bits);
  const FloatT op2 = std::bit_cast<FloatT>(rhs_bits);
  const long double numerator_exact = static_cast<long double>(op1) * static_cast<long double>(op2) + 3.0L;
  if (numerator_exact == 0.0L) {
    return fp_zero_bits<UIntT>((fpcr_mode & 0x3u) == 2u);
  }
  int exceptions = 0;
  const FloatT result = host_fp_eval<FloatT>(fpcr_mode, exceptions, [&]() {
    volatile FloatT a = op1;
    volatile FloatT b = op2;
    const FloatT numerator = static_cast<FloatT>(std::fma(static_cast<FloatT>(a),
                                                          static_cast<FloatT>(b),
                                                          static_cast<FloatT>(3.0)));
    return static_cast<FloatT>(std::ldexp(numerator, -1));
  });
  fpsr_bits |= host_fp_exceptions_to_fpsr(exceptions);
  UIntT result_bits = std::bit_cast<UIntT>(result);
  result_bits = fp_flush_output_denormal_bits(result_bits, fpcr_mode, fpsr_bits);
  return result_bits;
}

template <typename UIntT>
UIntT fp_recip_exponent_bits(UIntT bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(UIntT) == 4u || sizeof(UIntT) == 8u);
  fpsr_bits = 0u;
  if (fp_is_nan_bits(bits)) {
    return fp_process_nan_operand_bits(bits, fpcr_mode, true, fpsr_bits);
  }
  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  const UIntT sign = bits & fp_sign_mask_bits<UIntT>();
  const UIntT exp_mask = fp_exp_mask_bits<UIntT>();
  const std::uint32_t frac_bits = fp_frac_bits_count<UIntT>();
  const UIntT exp_all_ones = exp_mask >> frac_bits;
  const UIntT exp = (bits & exp_mask) >> frac_bits;
  const UIntT max_exp = exp_all_ones - 1u;

  const UIntT result_exp = (exp == 0u) ? max_exp : ((~exp) & exp_all_ones);
  return sign | (result_exp << frac_bits);
}

std::uint64_t fp32_to_fp64_bits(std::uint32_t bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  fpsr_bits = 0u;
  bool sign = (bits >> 31u) != 0u;
  std::uint64_t sign_bit = sign ? 0x8000000000000000ull : 0u;
  std::uint32_t exp = (bits >> 23u) & 0xFFu;
  std::uint32_t frac = bits & 0x007FFFFFu;

  if (exp == 0xFFu) {
    if (frac == 0u) {
      return sign_bit | 0x7FF0000000000000ull;
    }
    if (fpcr_default_nan_enabled(fpcr_mode)) {
      if (fp_is_signaling_nan_bits(bits)) {
        fpsr_bits |= kFpsrIoc;
      }
      return fp_default_nan_bits<std::uint64_t>();
    }
    if (fp_is_signaling_nan_bits(bits)) {
      fpsr_bits |= kFpsrIoc;
    }
    std::uint64_t payload = static_cast<std::uint64_t>(frac) << 29u;
    payload |= 0x0008000000000000ull;
    return sign_bit | 0x7FF0000000000000ull | (payload & 0x000FFFFFFFFFFFFFull);
  }

  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  sign = (bits >> 31u) != 0u;
  sign_bit = sign ? 0x8000000000000000ull : 0u;
  exp = (bits >> 23u) & 0xFFu;
  frac = bits & 0x007FFFFFu;

  if (exp == 0u && frac == 0u) {
    return sign_bit;
  }

  const double result = static_cast<double>(std::bit_cast<float>(bits));
  return std::bit_cast<std::uint64_t>(result);
}

template <typename DstUIntT>
DstUIntT fp16_to_fp_bits(std::uint16_t bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(DstUIntT) == 4u || sizeof(DstUIntT) == 8u);

  constexpr std::uint32_t kHalfExpBits = 5u;
  constexpr std::uint32_t kHalfFracBits = 10u;
  constexpr std::uint16_t kHalfSignMask = 0x8000u;
  constexpr std::uint16_t kHalfExpMask = 0x7C00u;
  constexpr std::uint16_t kHalfFracMask = 0x03FFu;
  constexpr std::uint16_t kHalfQuietNanBit = 0x0200u;

  fpsr_bits = 0u;
  const bool alt_hp = fpcr_alternative_half_enabled(fpcr_mode);
  const bool sign = (bits & kHalfSignMask) != 0u;
  const DstUIntT sign_bit = sign ? fp_sign_mask_bits<DstUIntT>() : 0u;
  const std::uint32_t exp = static_cast<std::uint32_t>((bits & kHalfExpMask) >> kHalfFracBits);
  const std::uint32_t frac = bits & kHalfFracMask;

  if (exp == ((1u << kHalfExpBits) - 1u) && !alt_hp) {
    if (frac == 0u) {
      return sign_bit | fp_exp_mask_bits<DstUIntT>();
    }
    const bool signaling_nan = (bits & kHalfQuietNanBit) == 0u;
    if (fpcr_default_nan_enabled(fpcr_mode)) {
      if (signaling_nan) {
        fpsr_bits |= kFpsrIoc;
      }
      return fp_default_nan_bits<DstUIntT>();
    }
    if (signaling_nan) {
      fpsr_bits |= kFpsrIoc;
    }
    DstUIntT payload =
        static_cast<DstUIntT>(frac) << (fp_frac_bits_count<DstUIntT>() - kHalfFracBits);
    payload |= fp_quiet_nan_bit<DstUIntT>();
    if ((payload & (fp_quiet_nan_bit<DstUIntT>() - 1u)) == 0u) {
      payload |= 1u;
    }
    return sign_bit | fp_exp_mask_bits<DstUIntT>() | (payload & fp_frac_mask_bits<DstUIntT>());
  }

  if (exp == 0u && frac == 0u) {
    return sign_bit;
  }

  std::uint32_t normalized_frac = frac;
  int exponent = 0;
  if (exp == 0u) {
    exponent = -14;
    while ((normalized_frac & 0x400u) == 0u) {
      normalized_frac <<= 1u;
      --exponent;
    }
    normalized_frac &= kHalfFracMask;
  } else {
    exponent = (alt_hp && exp == ((1u << kHalfExpBits) - 1u)) ? 16 : static_cast<int>(exp) - 15;
  }

  return sign_bit |
         (static_cast<DstUIntT>(exponent + fp_exp_bias<DstUIntT>()) << fp_frac_bits_count<DstUIntT>()) |
         (static_cast<DstUIntT>(normalized_frac) << (fp_frac_bits_count<DstUIntT>() - kHalfFracBits));
}

template <typename SrcUIntT>
std::uint16_t fp_to_fp16_bits(SrcUIntT bits, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(sizeof(SrcUIntT) == 4u || sizeof(SrcUIntT) == 8u);

  constexpr std::uint32_t kHalfFracBits = 10u;
  constexpr std::uint16_t kHalfExpMask = 0x7C00u;
  constexpr std::uint16_t kHalfQuietNanBit = 0x0200u;
  constexpr std::uint16_t kHalfFracMask = 0x03FFu;
  constexpr int kHalfIeeeMaxExp = 15;
  constexpr int kHalfAltMaxExp = 16;
  constexpr int kHalfMinExp = -14;

  fpsr_bits = 0u;
  const bool alt_hp = fpcr_alternative_half_enabled(fpcr_mode);
  bool sign = (bits & fp_sign_mask_bits<SrcUIntT>()) != 0u;
  const std::uint16_t sign_bit = sign ? 0x8000u : 0u;
  using SrcUnsigned =
      std::conditional_t<sizeof(SrcUIntT) == 4u, std::uint32_t, std::uint64_t>;
  const std::uint32_t src_frac_bits = fp_frac_bits_count<SrcUIntT>();
  const SrcUnsigned exp_mask = fp_exp_mask_bits<SrcUIntT>();
  const std::uint32_t exp = static_cast<std::uint32_t>((bits & exp_mask) >> src_frac_bits);
  const SrcUnsigned frac_mask = fp_frac_mask_bits<SrcUIntT>();
  const SrcUnsigned frac = bits & frac_mask;

  if (exp == ((1u << fp_exp_bits_count<SrcUIntT>()) - 1u)) {
    if (frac == 0u) {
      if (alt_hp) {
        fpsr_bits |= kFpsrIoc;
        return sign_bit | static_cast<std::uint16_t>(0x7FFFu);
      }
      return sign_bit | kHalfExpMask;
    }
    if (alt_hp) {
      fpsr_bits |= kFpsrIoc;
      return sign_bit;
    }
    if (fpcr_default_nan_enabled(fpcr_mode)) {
      if (fp_is_signaling_nan_bits(bits)) {
        fpsr_bits |= kFpsrIoc;
      }
      return fp_default_nan_bits<std::uint16_t>();
    }
    if (fp_is_signaling_nan_bits(bits)) {
      fpsr_bits |= kFpsrIoc;
    }
    std::uint16_t payload = static_cast<std::uint16_t>(frac >> (src_frac_bits - kHalfFracBits));
    payload |= kHalfQuietNanBit;
    if ((payload & kHalfFracMask) == kHalfQuietNanBit) {
      payload |= 1u;
    }
    return sign_bit | kHalfExpMask | (payload & kHalfFracMask);
  }

  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  sign = (bits & fp_sign_mask_bits<SrcUIntT>()) != 0u;
  const std::uint16_t sign_bit_after_flush = sign ? 0x8000u : 0u;
  const std::uint32_t exp_after_flush =
      static_cast<std::uint32_t>((bits & exp_mask) >> src_frac_bits);
  const SrcUnsigned frac_after_flush = bits & frac_mask;

  if (exp_after_flush == 0u && frac_after_flush == 0u) {
    return sign_bit_after_flush;
  }

  std::uint64_t significand = 0u;
  int exponent = 0;
  if (exp_after_flush == 0u) {
    const int msb = 63 - __builtin_clzll(static_cast<std::uint64_t>(frac_after_flush));
    const int normalize_shift = static_cast<int>(src_frac_bits) - msb;
    significand = static_cast<std::uint64_t>(frac_after_flush) << normalize_shift;
    exponent = (1 - fp_exp_bias<SrcUIntT>()) - normalize_shift;
  } else {
    significand = (1ull << src_frac_bits) | static_cast<std::uint64_t>(frac_after_flush);
    exponent = static_cast<int>(exp_after_flush) - fp_exp_bias<SrcUIntT>();
  }

  const int half_max_exp = alt_hp ? kHalfAltMaxExp : kHalfIeeeMaxExp;

  const auto overflow_result = [&]() -> std::uint16_t {
    if (alt_hp) {
      fpsr_bits |= kFpsrIoc;
      return sign_bit_after_flush | static_cast<std::uint16_t>(0x7FFFu);
    }
    fpsr_bits |= kFpsrOfc | kFpsrIxc;
    return sign_bit_after_flush |
           (fp_round_overflow_to_inf(fpcr_mode, sign) ? kHalfExpMask : static_cast<std::uint16_t>(0x7BFFu));
  };

  if (exponent > half_max_exp) {
    return overflow_result();
  }

  const std::uint32_t round_shift = src_frac_bits - kHalfFracBits;
  bool inexact = false;
  if (exponent >= kHalfMinExp) {
    std::uint64_t rounded = fp_round_shift_right(significand, round_shift, fpcr_mode, sign, inexact);
    if (rounded == (1ull << (kHalfFracBits + 1u))) {
      rounded >>= 1u;
      ++exponent;
    }
    if (exponent > half_max_exp) {
      return overflow_result();
    }
    if (inexact) {
      fpsr_bits |= kFpsrIxc;
    }
    return sign_bit_after_flush |
           (static_cast<std::uint16_t>(exponent + 15) << kHalfFracBits) |
           static_cast<std::uint16_t>(rounded & kHalfFracMask);
  }

  const std::uint32_t sub_shift = round_shift + static_cast<std::uint32_t>(kHalfMinExp - exponent);
  const std::uint64_t rounded = fp_round_shift_right(significand, sub_shift, fpcr_mode, sign, inexact);
  if (rounded >= (1ull << kHalfFracBits)) {
    if (inexact) {
      fpsr_bits |= kFpsrUfc | kFpsrIxc;
    }
    return sign_bit_after_flush | static_cast<std::uint16_t>(0x0400u);
  }
  if (inexact) {
    fpsr_bits |= kFpsrUfc | kFpsrIxc;
  }
  return sign_bit_after_flush | static_cast<std::uint16_t>(rounded);
}

template <typename DstUIntT, typename IntT>
DstUIntT fp_int_to_fp_bits(IntT value, std::uint32_t fpcr_mode, std::uint64_t& fpsr_bits) {
  static_assert(std::is_integral_v<IntT>);
  static_assert(sizeof(DstUIntT) == 4u || sizeof(DstUIntT) == 8u);

  using UnsignedIntT = std::make_unsigned_t<IntT>;
  constexpr std::uint32_t frac_bits = fp_frac_bits_count<DstUIntT>();
  constexpr int exp_bias = fp_exp_bias<DstUIntT>();
  const bool sign = std::is_signed_v<IntT> && value < 0;
  const DstUIntT sign_bit = sign ? fp_sign_mask_bits<DstUIntT>() : 0u;

  fpsr_bits = 0u;

  UnsignedIntT magnitude = 0u;
  if constexpr (std::is_signed_v<IntT>) {
    if (value < 0) {
      magnitude = static_cast<UnsignedIntT>(-(value + 1)) + 1u;
    } else {
      magnitude = static_cast<UnsignedIntT>(value);
    }
  } else {
    magnitude = value;
  }

  if (magnitude == 0u) {
    return 0u;
  }

  const std::uint32_t leading = static_cast<std::uint32_t>(63u - __builtin_clzll(static_cast<std::uint64_t>(magnitude)));
  const int exponent = static_cast<int>(leading);

  if (leading <= frac_bits) {
    const DstUIntT significand = static_cast<DstUIntT>(static_cast<std::uint64_t>(magnitude) << (frac_bits - leading));
    return sign_bit |
           (static_cast<DstUIntT>(exponent + exp_bias) << frac_bits) |
           (significand & fp_frac_mask_bits<DstUIntT>());
  }

  bool inexact = false;
  std::uint64_t rounded =
      fp_round_shift_right(static_cast<std::uint64_t>(magnitude), leading - frac_bits, fpcr_mode, sign, inexact);
  int rounded_exponent = exponent;
  if (rounded == (1ull << (frac_bits + 1u))) {
    rounded >>= 1u;
    ++rounded_exponent;
  }
  if (inexact) {
    fpsr_bits |= kFpsrIxc;
  }
  return sign_bit |
         (static_cast<DstUIntT>(rounded_exponent + exp_bias) << frac_bits) |
         (static_cast<DstUIntT>(rounded) & fp_frac_mask_bits<DstUIntT>());
}

template <typename UIntT, typename FloatT>
long double fp_to_int_input_value(UIntT bits,
                                  std::uint32_t fpcr_mode,
                                  std::uint64_t& fpsr_bits) {
  bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
  return static_cast<long double>(std::bit_cast<FloatT>(bits));
}

template <typename UIntT>
UIntT fp_to_unsigned_rtz(long double value, std::uint64_t& fpsr_bits) {
  fpsr_bits = 0;
  if (std::isnan(value)) {
    fpsr_bits |= kFpsrIoc;
    return 0;
  }
  constexpr int kBits = static_cast<int>(sizeof(UIntT) * 8u);
  const long double upper_bound = std::ldexp(1.0L, kBits);
  if (!std::isfinite(value) || value < 0.0L || value >= upper_bound) {
    fpsr_bits |= kFpsrIoc;
    if (value < 0.0L) {
      return 0;
    }
    return std::numeric_limits<UIntT>::max();
  }
  const long double truncated = std::trunc(value);
  if (truncated != value) {
    fpsr_bits |= kFpsrIxc;
  }
  return static_cast<UIntT>(truncated);
}

inline bool fp_round_up_from_floor(long double error,
                                   long double floor_value,
                                   FpToIntRoundingMode rounding) {
  switch (rounding) {
    case FpToIntRoundingMode::TieEven:
      return error > 0.5L || (error == 0.5L && std::fmod(std::fabs(floor_value), 2.0L) != 0.0L);
    case FpToIntRoundingMode::PosInf:
      return error != 0.0L;
    case FpToIntRoundingMode::NegInf:
      return false;
    case FpToIntRoundingMode::Zero:
      return error != 0.0L && floor_value < 0.0L;
    case FpToIntRoundingMode::TieAway:
      return error > 0.5L || (error == 0.5L && floor_value >= 0.0L);
  }
  return false;
}

template <typename IntT>
IntT fp_to_signed_rtz(long double value, std::uint64_t& fpsr_bits) {
  fpsr_bits = 0;
  if (std::isnan(value)) {
    fpsr_bits |= kFpsrIoc;
    return 0;
  }
  constexpr int kBits = static_cast<int>(sizeof(IntT) * 8u);
  const long double lower_bound = -std::ldexp(1.0L, kBits - 1);
  const long double upper_bound = std::ldexp(1.0L, kBits - 1);
  if (!std::isfinite(value) || value < lower_bound || value >= upper_bound) {
    fpsr_bits |= kFpsrIoc;
    if (value < 0.0L) {
      return std::numeric_limits<IntT>::min();
    }
    return std::numeric_limits<IntT>::max();
  }
  const long double truncated = std::trunc(value);
  if (truncated != value) {
    fpsr_bits |= kFpsrIxc;
  }
  return static_cast<IntT>(truncated);
}

template <typename UIntT>
UIntT fp_to_unsigned_round(long double value,
                           FpToIntRoundingMode rounding,
                           std::uint64_t& fpsr_bits) {
  fpsr_bits = 0;
  if (std::isnan(value)) {
    fpsr_bits |= kFpsrIoc;
    return 0;
  }
  if (!std::isfinite(value)) {
    fpsr_bits |= kFpsrIoc;
    return value < 0.0L ? 0 : std::numeric_limits<UIntT>::max();
  }

  long double int_result = std::floor(value);
  const long double error = value - int_result;
  if (fp_round_up_from_floor(error, int_result, rounding)) {
    int_result += 1.0L;
  }

  const long double max_value = static_cast<long double>(std::numeric_limits<UIntT>::max());
  if (int_result < 0.0L || int_result > max_value) {
    fpsr_bits |= kFpsrIoc;
    return int_result < 0.0L ? 0 : std::numeric_limits<UIntT>::max();
  }
  if (error != 0.0L) {
    fpsr_bits |= kFpsrIxc;
  }
  return static_cast<UIntT>(int_result);
}

template <typename IntT>
IntT fp_to_signed_round(long double value,
                        FpToIntRoundingMode rounding,
                        std::uint64_t& fpsr_bits) {
  fpsr_bits = 0;
  if (std::isnan(value)) {
    fpsr_bits |= kFpsrIoc;
    return 0;
  }
  if (!std::isfinite(value)) {
    fpsr_bits |= kFpsrIoc;
    return value < 0.0L ? std::numeric_limits<IntT>::min() : std::numeric_limits<IntT>::max();
  }

  long double int_result = std::floor(value);
  const long double error = value - int_result;
  if (fp_round_up_from_floor(error, int_result, rounding)) {
    int_result += 1.0L;
  }

  const long double min_value = static_cast<long double>(std::numeric_limits<IntT>::min());
  const long double max_value = static_cast<long double>(std::numeric_limits<IntT>::max());
  if (int_result < min_value || int_result > max_value) {
    fpsr_bits |= kFpsrIoc;
    return int_result < 0.0L ? std::numeric_limits<IntT>::min() : std::numeric_limits<IntT>::max();
  }
  if (error != 0.0L) {
    fpsr_bits |= kFpsrIxc;
  }
  return static_cast<IntT>(int_result);
}

template <typename IntT, typename WideT>
IntT saturate_signed_value(WideT value) {
  const WideT min_value = static_cast<WideT>(std::numeric_limits<IntT>::min());
  const WideT max_value = static_cast<WideT>(std::numeric_limits<IntT>::max());
  if (value < min_value) {
    return std::numeric_limits<IntT>::min();
  }
  if (value > max_value) {
    return std::numeric_limits<IntT>::max();
  }
  return static_cast<IntT>(value);
}

template <typename UIntT, typename WideT>
UIntT saturate_unsigned_value(WideT value) {
  const WideT max_value = static_cast<WideT>(std::numeric_limits<UIntT>::max());
  if (value > max_value) {
    return std::numeric_limits<UIntT>::max();
  }
  return static_cast<UIntT>(value);
}

template <typename UIntT, typename WideT>
UIntT saturate_signed_to_unsigned_value(WideT value) {
  if (value <= 0) {
    return 0;
  }
  const WideT max_value = static_cast<WideT>(std::numeric_limits<UIntT>::max());
  if (value > max_value) {
    return std::numeric_limits<UIntT>::max();
  }
  return static_cast<UIntT>(value);
}

std::uint64_t round_shift_right_unsigned(std::uint64_t value, std::uint32_t shift, bool round) {
  __uint128_t wide = value;
  if (round) {
    wide += (__uint128_t{1} << (shift - 1u));
  }
  return static_cast<std::uint64_t>(wide >> shift);
}

std::int64_t round_shift_right_signed(std::int64_t value, std::uint32_t shift, bool round) {
  __int128_t wide = value;
  if (round) {
    wide += (__int128_t{1} << (shift - 1u));
  }
  return static_cast<std::int64_t>(wide >> shift);
}

__int128_t round_shift_right_signed_wide(__int128_t value, std::uint32_t shift, bool round) {
  if (round) {
    value += (__int128_t{1} << (shift - 1u));
  }
  return value >> shift;
}

std::int64_t sign_extend_bits(std::uint64_t value, std::uint32_t bits) {
  if (bits == 0u || bits >= 64u) {
    return static_cast<std::int64_t>(value);
  }
  const std::uint32_t shift = 64u - bits;
  return static_cast<std::int64_t>(value << shift) >> shift;
}

enum class ShiftRightNarrowMode {
  PlainUnsigned,
  SignedToSignedSaturating,
  UnsignedToUnsignedSaturating,
  SignedToUnsignedSaturating,
};

std::uint64_t shift_right_narrow_result(std::uint64_t value,
                                        std::uint32_t src_esize_bits,
                                        std::uint32_t dst_esize_bits,
                                        std::uint32_t shift,
                                        bool round,
                                        ShiftRightNarrowMode mode,
                                        bool& saturated) {
  saturated = false;
  if (mode == ShiftRightNarrowMode::PlainUnsigned) {
    return round_shift_right_unsigned(value, shift, round) & ones(dst_esize_bits);
  }

  if (mode == ShiftRightNarrowMode::SignedToSignedSaturating) {
    const std::int64_t shifted = round_shift_right_signed(sign_extend_bits(value, src_esize_bits), shift, round);
    switch (dst_esize_bits) {
      case 8u:
        saturated = shifted < std::numeric_limits<std::int8_t>::min() ||
                    shifted > std::numeric_limits<std::int8_t>::max();
        return static_cast<std::uint64_t>(
            static_cast<std::uint8_t>(saturate_signed_value<std::int8_t>(shifted)));
      case 16u:
        saturated = shifted < std::numeric_limits<std::int16_t>::min() ||
                    shifted > std::numeric_limits<std::int16_t>::max();
        return static_cast<std::uint64_t>(
            static_cast<std::uint16_t>(saturate_signed_value<std::int16_t>(shifted)));
      case 32u:
        saturated = shifted < std::numeric_limits<std::int32_t>::min() ||
                    shifted > std::numeric_limits<std::int32_t>::max();
        return static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(saturate_signed_value<std::int32_t>(shifted)));
      default:
        return 0u;
    }
  }

  if (mode == ShiftRightNarrowMode::UnsignedToUnsignedSaturating) {
    const std::uint64_t shifted = round_shift_right_unsigned(value, shift, round);
    switch (dst_esize_bits) {
      case 8u:
        saturated = shifted > std::numeric_limits<std::uint8_t>::max();
        return saturate_unsigned_value<std::uint8_t>(shifted);
      case 16u:
        saturated = shifted > std::numeric_limits<std::uint16_t>::max();
        return saturate_unsigned_value<std::uint16_t>(shifted);
      case 32u:
        saturated = shifted > std::numeric_limits<std::uint32_t>::max();
        return saturate_unsigned_value<std::uint32_t>(shifted);
      default:
        return 0u;
    }
  }

  const std::int64_t shifted = round_shift_right_signed(sign_extend_bits(value, src_esize_bits), shift, round);
  switch (dst_esize_bits) {
    case 8u:
      saturated = shifted < 0 || shifted > std::numeric_limits<std::uint8_t>::max();
      return saturate_signed_to_unsigned_value<std::uint8_t>(shifted);
    case 16u:
      saturated = shifted < 0 || shifted > std::numeric_limits<std::uint16_t>::max();
      return saturate_signed_to_unsigned_value<std::uint16_t>(shifted);
    case 32u:
      saturated = shifted < 0 || shifted > std::numeric_limits<std::uint32_t>::max();
      return saturate_signed_to_unsigned_value<std::uint32_t>(shifted);
    default:
      return 0u;
  }
}

std::uint64_t sat_unsigned_add_sub(std::uint64_t lhs,
                                   std::uint64_t rhs,
                                   std::uint32_t esize_bits,
                                   bool subtract,
                                   bool& saturated) {
  const std::uint64_t mask = ones(esize_bits);
  lhs &= mask;
  rhs &= mask;
  if (subtract) {
    saturated = lhs < rhs;
    return saturated ? 0u : ((lhs - rhs) & mask);
  }

  const __uint128_t max_value = (esize_bits == 64u)
      ? static_cast<__uint128_t>(std::numeric_limits<std::uint64_t>::max())
      : ((__uint128_t{1} << esize_bits) - 1u);
  __uint128_t sum = static_cast<__uint128_t>(lhs) + static_cast<__uint128_t>(rhs);
  saturated = sum > max_value;
  if (saturated) {
    sum = max_value;
  }
  return static_cast<std::uint64_t>(sum) & mask;
}

std::uint64_t sat_signed_add_sub(std::uint64_t lhs,
                                 std::uint64_t rhs,
                                 std::uint32_t esize_bits,
                                 bool subtract,
                                 bool& saturated) {
  const __int128_t lhs_value = static_cast<__int128_t>(sign_extend_bits(lhs, esize_bits));
  const __int128_t rhs_value = static_cast<__int128_t>(sign_extend_bits(rhs, esize_bits));
  __int128_t result = subtract ? (lhs_value - rhs_value) : (lhs_value + rhs_value);
  const __int128_t min_value = -(__int128_t{1} << (esize_bits - 1u));
  const __int128_t max_value = (__int128_t{1} << (esize_bits - 1u)) - 1u;
  saturated = result < min_value || result > max_value;
  if (result < min_value) {
    result = min_value;
  } else if (result > max_value) {
    result = max_value;
  }
  return static_cast<std::uint64_t>(result) & ones(esize_bits);
}

std::uint64_t sqdmulh_result(std::uint64_t lhs,
                             std::uint64_t rhs,
                             std::uint32_t esize_bits,
                             bool round,
                             bool& saturated) {
  const __int128_t element1 = static_cast<__int128_t>(sign_extend_bits(lhs, esize_bits));
  const __int128_t element2 = static_cast<__int128_t>(sign_extend_bits(rhs, esize_bits));
  const __int128_t product =
      round_shift_right_signed_wide(2 * element1 * element2, esize_bits, round);
  saturated = false;
  switch (esize_bits) {
    case 16u:
      saturated = product < std::numeric_limits<std::int16_t>::min() ||
                  product > std::numeric_limits<std::int16_t>::max();
      return static_cast<std::uint64_t>(
          static_cast<std::uint16_t>(saturate_signed_value<std::int16_t>(product)));
    case 32u:
      saturated = product < std::numeric_limits<std::int32_t>::min() ||
                  product > std::numeric_limits<std::int32_t>::max();
      return static_cast<std::uint64_t>(
          static_cast<std::uint32_t>(saturate_signed_value<std::int32_t>(product)));
    default:
      return 0u;
  }
}

std::uint64_t sqdmull_product_result(std::uint64_t lhs,
                                     std::uint64_t rhs,
                                     std::uint32_t esize_bits,
                                     bool& saturated) {
  const __int128_t element1 = static_cast<__int128_t>(sign_extend_bits(lhs, esize_bits));
  const __int128_t element2 = static_cast<__int128_t>(sign_extend_bits(rhs, esize_bits));
  const __int128_t product = 2 * element1 * element2;
  saturated = false;
  switch (esize_bits) {
    case 16u:
      saturated = product < std::numeric_limits<std::int32_t>::min() ||
                  product > std::numeric_limits<std::int32_t>::max();
      return static_cast<std::uint64_t>(
          static_cast<std::uint32_t>(saturate_signed_value<std::int32_t>(product)));
    case 32u:
      saturated = product < std::numeric_limits<std::int64_t>::min() ||
                  product > std::numeric_limits<std::int64_t>::max();
      return static_cast<std::uint64_t>(saturate_signed_value<std::int64_t>(product));
    default:
      return 0u;
  }
}

std::uint64_t sqdml_long_accumulate_result(std::uint64_t acc,
                                           std::uint64_t lhs,
                                           std::uint64_t rhs,
                                           std::uint32_t esize_bits,
                                           bool subtract,
                                           bool& saturated) {
  bool sat_product = false;
  const std::uint64_t product = sqdmull_product_result(lhs, rhs, esize_bits, sat_product);
  bool sat_acc = false;
  const std::uint64_t result =
      sat_signed_add_sub(acc, product, esize_bits * 2u, subtract, sat_acc);
  saturated = sat_product || sat_acc;
  return result;
}

std::uint64_t long_multiply_result(std::uint64_t lhs,
                                   std::uint64_t rhs,
                                   std::uint32_t src_esize_bits,
                                   bool is_unsigned) {
  const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
  const std::uint64_t src_mask = ones(src_esize_bits);
  const std::uint64_t dst_mask = ones(dst_esize_bits);
  if (is_unsigned) {
    const __uint128_t product =
        static_cast<__uint128_t>(lhs & src_mask) * static_cast<__uint128_t>(rhs & src_mask);
    return static_cast<std::uint64_t>(product) & dst_mask;
  }
  const __int128_t product =
      static_cast<__int128_t>(sign_extend_bits(lhs, src_esize_bits)) *
      static_cast<__int128_t>(sign_extend_bits(rhs, src_esize_bits));
  return static_cast<std::uint64_t>(product) & dst_mask;
}

std::uint64_t long_multiply_accumulate_result(std::uint64_t acc,
                                              std::uint64_t lhs,
                                              std::uint64_t rhs,
                                              std::uint32_t src_esize_bits,
                                              bool subtract,
                                              bool is_unsigned) {
  const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
  const std::uint64_t dst_mask = ones(dst_esize_bits);
  if (is_unsigned) {
    const __uint128_t acc_value = static_cast<__uint128_t>(acc & dst_mask);
    const __uint128_t product = static_cast<__uint128_t>(long_multiply_result(
        lhs,
        rhs,
        src_esize_bits,
        true));
    const __uint128_t result = subtract ? (acc_value - product) : (acc_value + product);
    return static_cast<std::uint64_t>(result) & dst_mask;
  }
  const __int128_t acc_value =
      static_cast<__int128_t>(sign_extend_bits(acc, dst_esize_bits));
  const __int128_t product =
      static_cast<__int128_t>(sign_extend_bits(long_multiply_result(lhs, rhs, src_esize_bits, false),
                                               dst_esize_bits));
  const __int128_t result = subtract ? (acc_value - product) : (acc_value + product);
  return static_cast<std::uint64_t>(result) & dst_mask;
}

} // namespace

Cpu::Cpu(Bus& bus, GicV3& gic, GenericTimer& timer) : bus_(bus), gic_(gic), timer_(timer) {}

void Cpu::set_predecode_enabled(bool enabled) {
  if (predecode_enabled_ == enabled) {
    return;
  }
  predecode_enabled_ = enabled;
  invalidate_decode_all();
}

void Cpu::invalidate_decode_all() {
  for (auto& page : decode_pages_) {
    page.valid = false;
  }
  decode_last_page_ = nullptr;
  ++decode_context_epoch_;
  if (decode_context_epoch_ == 0) {
    decode_context_epoch_ = 1;
  }
}

void Cpu::invalidate_decode_va_page(std::uint64_t va_page) {
  DecodePage& page = decode_pages_[static_cast<std::size_t>((va_page >> 12) & (kDecodeCachePages - 1u))];
  if (page.valid && page.va_page == va_page) {
    page.valid = false;
    if (&page == decode_last_page_) {
      decode_last_page_ = nullptr;
    }
  }
}

void Cpu::invalidate_decode_pa_page(std::uint64_t pa_page) {
  for (auto& page : decode_pages_) {
    if (page.valid && page.pa_page == pa_page) {
      page.valid = false;
      if (&page == decode_last_page_) {
        decode_last_page_ = nullptr;
      }
    }
  }
}

void Cpu::invalidate_decode_va(std::uint64_t va, std::size_t size) {
  const std::uint64_t first_page = va & ~0xFFFull;
  const std::uint64_t last_page = (va + static_cast<std::uint64_t>(size) - 1u) & ~0xFFFull;
  for (std::uint64_t page = first_page;; page += 0x1000u) {
    invalidate_decode_va_page(page);
    if (page == last_page) {
      break;
    }
  }
}

void Cpu::invalidate_decode_pa(std::uint64_t pa, std::size_t size) {
  const std::uint64_t first_page = pa & ~0xFFFull;
  const std::uint64_t last_page = (pa + static_cast<std::uint64_t>(size) - 1u) & ~0xFFFull;
  for (std::uint64_t page = first_page;; page += 0x1000u) {
    invalidate_decode_pa_page(page);
    if (page == last_page) {
      break;
    }
  }
}

void Cpu::invalidate_decode_tlb_context() {
  invalidate_decode_all();
}

void Cpu::on_code_write(std::uint64_t va, std::uint64_t pa, std::size_t size) {
  if (!predecode_enabled_) {
    return;
  }
  invalidate_decode_va(normalize_stage1_address(va, false), size);
  invalidate_decode_pa(pa, size);
}

void Cpu::data_abort(std::uint64_t va, bool cache_maintenance_or_translation) {
  if (exception_taken_this_step_) {
    return;
  }
  const std::uint32_t iss = last_data_abort_iss_override_.has_value()
                                ? *last_data_abort_iss_override_
                                : (last_translation_fault_.has_value()
                                       ? data_abort_iss(*last_translation_fault_, cache_maintenance_or_translation)
                                       : 0u);
  const std::uint64_t far = last_data_fault_va_.value_or(va);
  enter_sync_exception(pc_ - 4, sysregs_.in_el0() ? 0x24u : 0x25u, iss, true, far);
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();
}

bool Cpu::effective_tbi(std::uint64_t va, bool is_instruction) const {
  (void)is_instruction;
  const std::uint64_t tcr = sysregs_.tcr_el1();
  const bool upper_range = ((va >> 55) & 0x1u) != 0u;
  const std::uint32_t tbi_bit = upper_range ? 38u : 37u;
  // The current model declares FEAT_PAuth absent, so TBID0/TBID1 are RES0 and
  // TBI applies equally to instruction and data addresses.
  return ((tcr >> tbi_bit) & 0x1u) != 0u;
}

std::uint64_t Cpu::normalize_stage1_address(std::uint64_t va, bool is_instruction) const {
  if (!effective_tbi(va, is_instruction)) {
    return va;
  }
  constexpr std::uint64_t kTopByteMask = 0xFFull << 56;
  const std::uint64_t masked = va & ~kTopByteMask;
  return ((va >> 55) & 0x1u) != 0u ? (masked | kTopByteMask) : masked;
}

void Cpu::fill_stage1_disabled_output(std::uint64_t effective_va,
                                      AccessType access,
                                      TranslationResult* out_result) const {
  if (out_result == nullptr) {
    return;
  }
  const bool va_upper = (effective_va >> 63) != 0;
  out_result->pa = stage1_disabled_output_pa(effective_va);
  out_result->asid = current_translation_asid(va_upper);
  out_result->level = 3;
  out_result->attr_index = 0;
  out_result->global_entry = true;
  out_result->writable = true;
  out_result->user_accessible = true;
  out_result->executable = true;
  out_result->pxn = false;
  out_result->uxn = false;
  out_result->leaf_shareability = Shareability::OuterShareable;
  out_result->walk_attrs = decode_walk_attributes(va_upper);
  if (access == AccessType::Fetch) {
    // AArch64.S1DisabledOutput(): instruction accesses use Normal memory and
    // depend on SCTLR_EL1.I for the direct-mapped cacheability attributes.
    out_result->mair_attr = (sysregs_.sctlr_el1() & (1ull << 12)) != 0u ? 0xAAu : 0x44u;
    out_result->memory_type = MemoryType::Normal;
  } else {
    // AArch64.S1DisabledOutput(): data accesses use Device-nGnRnE.
    out_result->mair_attr = 0x00u;
    out_result->memory_type = MemoryType::Device;
  }
}

bool Cpu::alignment_check_enabled() const {
  return (sysregs_.sctlr_el1() & (1ull << 1)) != 0u;
}

bool Cpu::sp_alignment_check_enabled() const {
  const std::uint64_t sctlr = sysregs_.sctlr_el1();
  if (sysregs_.in_el0()) {
    return (sctlr & (1ull << 4)) != 0u;
  }
  return (sctlr & (1ull << 3)) != 0u;
}

bool Cpu::maybe_take_sp_alignment_fault(std::uint32_t base_reg) {
  if (base_reg != 31u || !sp_alignment_check_enabled() || (regs_[31] & 0xFu) == 0u) {
    return false;
  }
  last_translation_fault_.reset();
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();
  enter_sync_exception(pc_ - 4u, 0x26u, 0u, false, 0u);
  return true;
}

bool Cpu::maybe_take_data_alignment_fault(std::uint64_t addr,
                                          std::size_t align,
                                          AccessType access,
                                          bool force_check) {
  if (align <= 1u || (addr % align) == 0u) {
    return false;
  }
  if (!force_check && !alignment_check_enabled()) {
    MemoryType memory_type = MemoryType::Unknown;
    if (!sysregs_.mmu_enabled()) {
      const std::uint64_t effective_va = normalize_stage1_address(addr, false);
      const std::uint64_t direct_pa = stage1_disabled_output_pa(effective_va);
      // With stage-1 disabled the architectural output memory type for data
      // accesses is Device-nGnRnE. This model chooses to permit unaligned
      // accesses only for RAM-backed direct-mapped locations, reflecting the
      // implementation-defined "Device location supports unaligned access"
      // latitude in AArch64.S1HasAlignmentFaultDueToMemType().
      memory_type = (bus_.ram_ptr(direct_pa, 1u) != nullptr) ? MemoryType::Normal : MemoryType::Device;
    } else {
      TranslationResult result{};
      TranslationFault fault{};
      const std::uint64_t effective_va = normalize_stage1_address(addr, false);
      // Alignment faults caused by memory type are prioritized after
      // translation and Access Flag faults, but before permission faults.
      // Probe the stage-1 output attributes accordingly.
      if (!walk_page_tables(effective_va, access, &result, &fault, false, false)) {
        return false;
      }
      memory_type = result.memory_type;
    }
    if (memory_type != MemoryType::Device) {
      return false;
    }
  }
  last_translation_fault_.reset();
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();
  const std::uint32_t iss = 0x21u | (access_is_write(access) ? (1u << 6) : 0u);
  enter_sync_exception(pc_ - 4u, sysregs_.in_el0() ? 0x24u : 0x25u, iss, true, addr);
  return true;
}

namespace {

bool load_fast_value(const std::uint8_t* base, std::size_t size, std::uint64_t* out) {
  std::uint64_t value = 0;
  switch (size) {
    case 1u:
      value = base[0];
      break;
    case 2u: {
      std::uint16_t v = 0;
      std::memcpy(&v, base, sizeof(v));
      value = v;
      break;
    }
    case 4u: {
      std::uint32_t v = 0;
      std::memcpy(&v, base, sizeof(v));
      value = v;
      break;
    }
    case 8u:
      std::memcpy(&value, base, sizeof(value));
      break;
    default:
      return false;
  }
  if (out != nullptr) {
    *out = value;
  }
  return true;
}



} // namespace

bool Cpu::translate_data_address_fast(std::uint64_t va, bool write, std::uint64_t* out_pa) {
  return translate_data_address_fast(va, write ? AccessType::Write : AccessType::Read, out_pa);
}

bool Cpu::translate_data_address_fast(std::uint64_t va, AccessType access, std::uint64_t* out_pa) {
  ++perf_counters_.translate_calls;
  last_translation_fault_.reset();
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();

  const std::uint64_t effective_va = normalize_stage1_address(va, false);

  if (!sysregs_.mmu_enabled()) {
    if (out_pa != nullptr) {
      *out_pa = stage1_disabled_output_pa(effective_va);
    }
    return true;
  }

  const bool va_upper = (effective_va >> 63) != 0;
  const std::uint16_t asid = current_translation_asid(va_upper);
  const std::uint64_t page = (effective_va >> 12) & tlb_page_mask();
  const std::uint64_t off = effective_va & 0xFFFull;
  const TlbEntry* hit = nullptr;
  if (tlb_last_data_.valid && tlb_last_data_.va_page == page && tlb_entry_matches_asid(tlb_last_data_, asid)) {
    hit = &tlb_last_data_;
  } else {
    ++perf_counters_.tlb_lookups;
    hit = tlb_lookup(page, asid);
    if (hit != nullptr) {
      tlb_last_data_ = *hit;
      hit = &tlb_last_data_;
    }
  }
  if (hit != nullptr) {
    ++perf_counters_.tlb_hits;
    TranslationResult result{};
    result.pa = (hit->pa_page << 12) | off;
    result.asid = hit->asid;
    result.level = hit->level;
    result.attr_index = hit->attr_index;
    result.mair_attr = hit->mair_attr;
    result.writable = hit->writable;
    result.user_accessible = hit->user_accessible;
    result.executable = hit->executable;
    result.pxn = hit->pxn;
    result.uxn = hit->uxn;
    result.memory_type = hit->memory_type;
    result.leaf_shareability = hit->leaf_shareability;
    result.walk_attrs = hit->walk_attrs;
    TranslationFault fault{};
    if (!access_permitted(result, access, result.level, &fault)) {
      last_translation_fault_ = fault;
      return false;
    }
    if (out_pa != nullptr) {
      *out_pa = result.pa;
    }
    return true;
  }

  ++perf_counters_.tlb_misses;
  TranslationResult result{};
  TranslationFault fault{};
  if (!walk_page_tables(effective_va, access, &result, &fault)) {
    last_translation_fault_ = fault;
    return false;
  }
  tlb_insert(page, result);
  if (out_pa != nullptr) {
    *out_pa = result.pa;
  }
  return true;
}

void Cpu::invalidate_ram_page_caches() {
}

bool Cpu::mmu_read_value(std::uint64_t va,
                         std::size_t size,
                         std::uint64_t* out,
                         bool check_watchpoints) {
  return mmu_read_value(va, size, out, AccessType::Read, check_watchpoints);
}

bool Cpu::mmu_read_value(std::uint64_t va,
                         std::size_t size,
                         std::uint64_t* out,
                         AccessType access,
                         bool check_watchpoints) {
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();
  if (size == 0u || size > sizeof(std::uint64_t)) {
    return false;
  }
  const std::size_t page_off = static_cast<std::size_t>(va & 0xFFFu);
  if (page_off + size <= 0x1000u) {
    std::uint64_t pa = 0;
    if (!translate_data_address_fast(va, access, &pa)) {
      last_data_fault_va_ = va;
      return false;
    }
    if (check_watchpoints && maybe_take_watchpoint_exception(va, size, access)) {
      return false;
    }
    if (!debug_slow_mode_enabled()) {
      if (const std::uint8_t* base = bus_.ram_ptr(pa, size); base != nullptr) {
        return load_fast_value(base, size, out);
      }
    }
    std::uint64_t value = 0;
    if (!bus_.read(pa, size, value)) {
      if (check_watchpoints) {
        note_external_data_abort(access_is_write(access), false, va);
      }
      last_data_fault_va_ = va;
      return false;
    }
    if (out != nullptr) {
      *out = value;
    }
    return true;
  }

  std::uint64_t value = 0;
  std::array<std::uint64_t, sizeof(std::uint64_t)> pas{};
  for (std::size_t i = 0; i < size; ++i) {
    if (!translate_data_address_fast(va + i, access, &pas[i])) {
      last_data_fault_va_ = va + i;
      return false;
    }
  }
  if (check_watchpoints && maybe_take_watchpoint_exception(va, size, access)) {
    return false;
  }
  for (std::size_t i = 0; i < size; ++i) {
    std::uint64_t byte = 0;
    if (!bus_.read(pas[i], 1u, byte)) {
      if (check_watchpoints) {
        note_external_data_abort(access_is_write(access), false, va + i);
      }
      last_data_fault_va_ = va + i;
      return false;
    }
    value |= (byte & 0xFFu) << (i * 8u);
  }
  if (out != nullptr) {
    *out = value;
  }
  return true;
}

bool Cpu::mmu_write_value(std::uint64_t va,
                          std::uint64_t value,
                          std::size_t size,
                          bool check_watchpoints) {
  return mmu_write_value(va, value, size, AccessType::Write, check_watchpoints);
}

bool Cpu::mmu_write_value(std::uint64_t va,
                          std::uint64_t value,
                          std::size_t size,
                          AccessType access,
                          bool check_watchpoints) {
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();
  static const std::optional<std::uint64_t> trace_write_va = []() -> std::optional<std::uint64_t> {
    const char* env = std::getenv("AARCHVM_TRACE_WRITE_VA");
    if (env == nullptr || *env == '\0') {
      return std::nullopt;
    }
    try {
      return static_cast<std::uint64_t>(std::stoull(env, nullptr, 0));
    } catch (...) {
      return std::nullopt;
    }
  }();
  static const std::optional<std::uint64_t> trace_write_pa = []() -> std::optional<std::uint64_t> {
    const char* env = std::getenv("AARCHVM_TRACE_WRITE_PA");
    if (env == nullptr || *env == '\0') {
      return std::nullopt;
    }
    try {
      return static_cast<std::uint64_t>(std::stoull(env, nullptr, 0));
    } catch (...) {
      return std::nullopt;
    }
  }();

  if (size == 0u || size > sizeof(std::uint64_t)) {
    return false;
  }

  const auto log_write_hit = [&](std::uint64_t pa, bool cross_page, const std::array<std::uint64_t, sizeof(std::uint64_t)>* pas) {
    const std::uint64_t current_task = sysregs_.sp_el0();
    std::uint64_t task_stack = 0;
    const bool have_task_stack = mmu_read_value(current_task + 32u, 8u, &task_stack, false);
    std::cerr << std::dec << "WRITE-HIT va=0x" << std::hex << va
              << " pa=0x" << pa
              << " size=" << std::dec << size
              << " value=0x" << std::hex << value
              << " pc=0x" << (pc_ - 4)
              << " sp=0x" << regs_[31]
              << " x29=0x" << reg(29)
              << " x30=0x" << reg(30)
              << " sp_el0=0x" << current_task;
    if (have_task_stack) {
      std::cerr << " task_stack=0x" << task_stack;
    }
    if (cross_page && pas != nullptr) {
      std::cerr << " cross-page=1 pas=";
      for (std::size_t i = 0; i < size; ++i) {
        if (i != 0u) {
          std::cerr << ',';
        }
        std::cerr << "0x" << std::hex << (*pas)[i];
      }
    }
    std::cerr << '\n';
  };

  const std::size_t page_off = static_cast<std::size_t>(va & 0xFFFu);
  if (page_off + size <= 0x1000u) {
    std::uint64_t pa = 0;
    if (!translate_data_address_fast(va, access, &pa)) {
      last_data_fault_va_ = va;
      return false;
    }
    if (check_watchpoints && maybe_take_watchpoint_exception(va, size, access)) {
      return false;
    }
    if ((trace_write_va.has_value() && *trace_write_va >= va && *trace_write_va < (va + size)) ||
        (trace_write_pa.has_value() && *trace_write_pa >= pa && *trace_write_pa < (pa + size))) {
      log_write_hit(pa, false, nullptr);
    }
    bool ok = false;
    if (!debug_slow_mode_enabled()) {
      ok = bus_.write_ram_fast(pa, value, size);
    }
    if (!ok) {
      ok = bus_.write(pa, value, size);
    }
    if (!ok) {
      note_external_data_abort(access_is_write(access), false, va);
      last_data_fault_va_ = va;
      return false;
    }
    on_code_write(va, pa, size);
    clear_exclusive_monitor();
    if (callbacks_.memory_write) {
      callbacks_.memory_write(*this, pa, size);
    }
    return true;
  }

  std::array<std::uint64_t, sizeof(std::uint64_t)> pas{};
  bool trace_cross_page_pa = false;
  for (std::size_t i = 0; i < size; ++i) {
    if (!translate_data_address_fast(va + i, access, &pas[i])) {
      last_data_fault_va_ = va + i;
      return false;
    }
    if (trace_write_pa.has_value() && pas[i] == *trace_write_pa) {
      trace_cross_page_pa = true;
    }
  }
  if ((trace_write_va.has_value() && *trace_write_va >= va && *trace_write_va < (va + size)) ||
      trace_cross_page_pa) {
    log_write_hit(pas[0], true, &pas);
  }
  if (check_watchpoints && maybe_take_watchpoint_exception(va, size, access)) {
    return false;
  }
  for (std::size_t i = 0; i < size; ++i) {
    const std::uint64_t byte = (value >> (i * 8u)) & 0xFFu;
    if (!bus_.write(pas[i], byte, 1u)) {
      note_external_data_abort(access_is_write(access), false, va + i);
      last_data_fault_va_ = va + i;
      return false;
    }
    on_code_write(va + i, pas[i], 1u);
    if (callbacks_.memory_write) {
      callbacks_.memory_write(*this, pas[i], 1u);
    }
  }
  clear_exclusive_monitor();
  return true;
}

std::uint32_t Cpu::external_abort_iss(bool write, bool cache_maintenance_or_translation) const {
  std::uint32_t iss = sync_external_abort_fsc();
  if (write) {
    iss |= 1u << 6;
  }
  if (cache_maintenance_or_translation) {
    iss |= 1u << 8;
    iss |= 1u << 6;
  }
  return iss;
}

void Cpu::note_external_data_abort(bool write,
                                   bool cache_maintenance_or_translation,
                                   std::uint64_t fault_va) {
  last_translation_fault_.reset();
  last_data_abort_iss_override_ = external_abort_iss(write, cache_maintenance_or_translation);
  last_data_fault_va_ = fault_va;
}

void Cpu::clear_exclusive_monitor() {
  exclusive_valid_ = false;
  exclusive_addr_ = 0;
  exclusive_size_ = 0;
  exclusive_pa_count_ = 0;
  exclusive_phys_addrs_.fill(0);
}

bool Cpu::capture_exclusive_phys_addrs(std::uint64_t va,
                                       std::size_t size,
                                       bool write,
                                       std::array<std::uint64_t, 16>* out_pas) {
  if (out_pas == nullptr || size == 0u || size > out_pas->size()) {
    return false;
  }
  out_pas->fill(0);
  for (std::size_t i = 0; i < size; ++i) {
    std::uint64_t pa = 0;
    if (!translate_data_address_fast(va + i, write, &pa)) {
      last_data_fault_va_ = va + i;
      return false;
    }
    (*out_pas)[i] = pa;
  }
  return true;
}

void Cpu::set_exclusive_monitor(std::uint64_t va, std::size_t size) {
  clear_exclusive_monitor();
  if (!capture_exclusive_phys_addrs(va, size, false, &exclusive_phys_addrs_)) {
    return;
  }
  exclusive_valid_ = true;
  exclusive_addr_ = va;
  exclusive_size_ = static_cast<std::uint8_t>(size);
  exclusive_pa_count_ = static_cast<std::uint8_t>(size);
}

bool Cpu::check_exclusive_monitor(std::uint64_t va, std::size_t size, bool* matched) {
  if (matched == nullptr) {
    return false;
  }
  *matched = false;
  if (!exclusive_valid_ || exclusive_size_ != size || exclusive_pa_count_ != size) {
    return true;
  }
  std::array<std::uint64_t, 16> current_pas{};
  if (!capture_exclusive_phys_addrs(va, size, true, &current_pas)) {
    return false;
  }
  *matched = std::equal(current_pas.begin(),
                        current_pas.begin() + static_cast<std::ptrdiff_t>(size),
                        exclusive_phys_addrs_.begin());
  return true;
}

bool Cpu::exclusive_monitor_overlaps(std::uint64_t pa, std::size_t size) const {
  if (!exclusive_valid_ || size == 0u) {
    return false;
  }
  const std::size_t count = std::min<std::size_t>(exclusive_pa_count_, exclusive_phys_addrs_.size());
  for (std::size_t i = 0; i < count; ++i) {
    if (ranges_overlap(exclusive_phys_addrs_[i], 1u, pa, size)) {
      return true;
    }
  }
  return false;
}

Cpu::DecodedInsn Cpu::decode_insn(std::uint32_t insn) const {
  DecodedInsn decoded{};

  if ((insn & 0x7C000000u) == 0x14000000u) {
    decoded.kind = DecodedKind::BImm;
    decoded.simm = static_cast<std::int32_t>(sign_extend((insn & 0x03FFFFFFu) << 2u, 28));
    if ((insn & 0x80000000u) != 0u) {
      decoded.flags |= kDecodedFlagLink;
    }
    return decoded;
  }
  if ((insn & 0xFF000010u) == 0x54000000u) {
    decoded.kind = DecodedKind::BCond;
    decoded.aux = static_cast<std::uint8_t>(insn & 0xFu);
    decoded.simm = static_cast<std::int32_t>(sign_extend(((insn >> 5) & 0x7FFFFu) << 2u, 21));
    return decoded;
  }
  if ((insn & 0x7F000000u) == 0x34000000u || (insn & 0x7F000000u) == 0x35000000u) {
    decoded.kind = DecodedKind::Cbz;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.simm = static_cast<std::int32_t>(sign_extend(((insn >> 5) & 0x7FFFFu) << 2u, 21));
    if ((insn >> 31) != 0u) {
      decoded.flags |= kDecodedFlagSf;
    }
    if ((insn & 0x01000000u) != 0u) {
      decoded.flags |= kDecodedFlagNonZero;
    }
    return decoded;
  }
  if ((insn & 0x7F000000u) == 0x36000000u || (insn & 0x7F000000u) == 0x37000000u) {
    decoded.kind = DecodedKind::Tbz;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.aux = static_cast<std::uint8_t>((((insn >> 31) & 0x1u) << 5) | ((insn >> 19) & 0x1Fu));
    decoded.simm = static_cast<std::int32_t>(sign_extend(((insn >> 5) & 0x3FFFu) << 2u, 16));
    if ((insn & 0x01000000u) != 0u) {
      decoded.flags |= kDecodedFlagNonZero;
    }
    return decoded;
  }
  if ((insn & 0x7F800000u) == 0x52800000u || (insn & 0x7F800000u) == 0x72800000u ||
      (insn & 0x7F800000u) == 0x12800000u) {
    decoded.kind = DecodedKind::MovWide;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.aux = static_cast<std::uint8_t>((insn >> 29) & 0x3u);
    decoded.aux2 = static_cast<std::uint8_t>(((insn >> 21) & 0x3u) * 16u);
    decoded.imm = (insn >> 5) & 0xFFFFu;
    if ((insn >> 31) != 0u) {
      decoded.flags |= kDecodedFlagSf;
    }
    return decoded;
  }
  if ((insn & 0x7FC00000u) == 0x11800000u || (insn & 0x7FC00000u) == 0x51800000u) {
    return decoded;
  }
  if ((insn & 0x7F000000u) == 0x11000000u || (insn & 0x7F000000u) == 0x31000000u ||
      (insn & 0x7F000000u) == 0x51000000u || (insn & 0x7F000000u) == 0x71000000u) {
    decoded.kind = DecodedKind::AddSubImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = (insn >> 10) & 0xFFFu;
    decoded.aux = static_cast<std::uint8_t>(((insn >> 22) & 0x1u) * 12u);
    if ((insn >> 31) != 0u) {
      decoded.flags |= kDecodedFlagSf;
    }
    if ((insn & 0x40000000u) != 0u) {
      decoded.flags |= kDecodedFlagSub;
    }
    if ((insn & 0x20000000u) != 0u) {
      decoded.flags |= kDecodedFlagSetFlags;
    }
    return decoded;
  }
  if ((insn & 0x1F000000u) == 0x0A000000u) {
    const bool sf = (insn >> 31) != 0u;
    const std::uint32_t imm6 = (insn >> 10) & 0x3Fu;
    if (!sf && (imm6 & 0x20u)) {
      return decoded;
    }
    decoded.kind = DecodedKind::LogicalShifted;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.rm = static_cast<std::uint8_t>((insn >> 16) & 0x1Fu);
    decoded.aux = static_cast<std::uint8_t>((insn >> 22) & 0x3u);
    decoded.aux2 = static_cast<std::uint8_t>((insn >> 29) & 0x3u);
    decoded.imm = imm6;
    if (sf) {
      decoded.flags |= kDecodedFlagSf;
    }
    if ((insn & 0x00200000u) != 0u) {
      decoded.flags |= kDecodedFlagInvert;
    }
    return decoded;
  }
  if ((insn & 0x1F000000u) == 0x12000000u) {
    const bool sf = (insn >> 31) != 0u;
    const std::uint32_t n = (insn >> 22) & 0x1u;
    if (!sf && n != 0u) {
      return decoded;
    }
    decoded.kind = DecodedKind::LogicalImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.aux = static_cast<std::uint8_t>((insn >> 29) & 0x3u);
    decoded.aux2 = static_cast<std::uint8_t>(n);
    decoded.imm = static_cast<std::uint32_t>(((insn >> 16) & 0x3Fu) | (((insn >> 10) & 0x3Fu) << 6u));
    if (sf) {
      decoded.flags |= kDecodedFlagSf;
    }
    return decoded;
  }
  if ((insn & 0x1F800000u) == 0x13000000u) {
    const bool sf = (insn >> 31) != 0u;
    const std::uint32_t n = (insn >> 22) & 0x1u;
    if ((sf && n != 1u) || (!sf && n != 0u)) {
      return decoded;
    }
    decoded.kind = DecodedKind::Bitfield;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.aux = static_cast<std::uint8_t>((insn >> 29) & 0x3u);
    decoded.aux2 = static_cast<std::uint8_t>(n);
    decoded.imm = static_cast<std::uint32_t>(((insn >> 16) & 0x3Fu) | (((insn >> 10) & 0x3Fu) << 6u));
    if (sf) {
      decoded.flags |= kDecodedFlagSf;
    }
    return decoded;
  }

  if ((insn & 0x7F200000u) == 0x2B000000u || (insn & 0x7F200000u) == 0x6B000000u ||
      (insn & 0x7F200000u) == 0x4B000000u) {
    const std::uint32_t shift_type = (insn >> 22) & 0x3u;
    if (shift_type > 2u) {
      return decoded;
    }
    decoded.kind = DecodedKind::AddSubShifted;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.rm = static_cast<std::uint8_t>((insn >> 16) & 0x1Fu);
    decoded.aux = static_cast<std::uint8_t>(shift_type);
    decoded.aux2 = static_cast<std::uint8_t>((insn >> 10) & 0x3Fu);
    if ((insn >> 31) != 0u) {
      decoded.flags |= kDecodedFlagSf;
    }
    if ((insn & 0x40000000u) != 0u) {
      decoded.flags |= kDecodedFlagSub;
    }
    if ((insn & 0x20000000u) != 0u) {
      decoded.flags |= kDecodedFlagSetFlags;
    }
    return decoded;
  }

  if ((insn & 0xFFC00000u) == 0xF9400000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 3u;
    decoded.aux = 8u;
    decoded.flags = kDecodedFlagLoad | kDecodedFlagResult64;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0xF9000000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 3u;
    decoded.aux = 8u;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0xB9400000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 2u;
    decoded.aux = 4u;
    decoded.flags = kDecodedFlagLoad;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0xB9000000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 2u;
    decoded.aux = 4u;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0xB9800000u) {
    decoded.kind = DecodedKind::LoadStore;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 2u;
    decoded.aux = 4u;
    decoded.flags = kDecodedFlagLoad | kDecodedFlagSigned | kDecodedFlagResult64;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x39C00000u) {
    decoded.kind = DecodedKind::LoadStore;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = (insn >> 10) & 0xFFFu;
    decoded.aux = 1u;
    decoded.flags = kDecodedFlagLoad | kDecodedFlagSigned;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x39800000u) {
    decoded.kind = DecodedKind::LoadStore;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = (insn >> 10) & 0xFFFu;
    decoded.aux = 1u;
    decoded.flags = kDecodedFlagLoad | kDecodedFlagSigned | kDecodedFlagResult64;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x79C00000u) {
    decoded.kind = DecodedKind::LoadStore;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 1u;
    decoded.aux = 2u;
    decoded.flags = kDecodedFlagLoad | kDecodedFlagSigned;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x79800000u) {
    decoded.kind = DecodedKind::LoadStore;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 1u;
    decoded.aux = 2u;
    decoded.flags = kDecodedFlagLoad | kDecodedFlagSigned | kDecodedFlagResult64;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x39400000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = (insn >> 10) & 0xFFFu;
    decoded.aux = 1u;
    decoded.flags = kDecodedFlagLoad;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x39000000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = (insn >> 10) & 0xFFFu;
    decoded.aux = 1u;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x79400000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 1u;
    decoded.aux = 2u;
    decoded.flags = kDecodedFlagLoad;
    return decoded;
  }
  if ((insn & 0xFFC00000u) == 0x79000000u) {
    decoded.kind = DecodedKind::LoadStoreUImm;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.imm = ((insn >> 10) & 0xFFFu) << 1u;
    decoded.aux = 2u;
    return decoded;
  }

  // LDRAA/LDRAB share the generic X post/pre-index load/store bit-patterns.
  // Leave them undecoded here so the slow path can apply the architected
  // FEAT_PAuth-absent behavior instead of caching them as plain LDR/STR.
  if (is_ldraa_ldrab_encoding(insn)) {
    return decoded;
  }

  if ((insn & 0xFFC00C00u) == 0x38400400u || (insn & 0xFFC00C00u) == 0x38000400u ||
      (insn & 0xFFC00C00u) == 0x38400C00u || (insn & 0xFFC00C00u) == 0x38000C00u ||
      (insn & 0xFFC00C00u) == 0x78400400u || (insn & 0xFFC00C00u) == 0x78000400u ||
      (insn & 0xFFC00C00u) == 0x78400C00u || (insn & 0xFFC00C00u) == 0x78000C00u ||
      (insn & 0xFFC00C00u) == 0xB8400400u || (insn & 0xFFC00C00u) == 0xB8000400u ||
      (insn & 0xFFC00C00u) == 0xB8400C00u || (insn & 0xFFC00C00u) == 0xB8000C00u ||
      (insn & 0xFFC00C00u) == 0xF8400400u || (insn & 0xFFC00C00u) == 0xF8000400u ||
      (insn & 0xFFC00C00u) == 0xF8400C00u || (insn & 0xFFC00C00u) == 0xF8000C00u) {
    const std::uint32_t mode = insn & 0xFFC00C00u;
    decoded.kind = DecodedKind::LoadStore;
    decoded.rd = static_cast<std::uint8_t>(insn & 0x1Fu);
    decoded.rn = static_cast<std::uint8_t>((insn >> 5) & 0x1Fu);
    decoded.simm = static_cast<std::int32_t>(sign_extend((insn >> 12) & 0x1FFu, 9));
    if (mode == 0x38400400u || mode == 0x38400C00u || mode == 0x78400400u || mode == 0x78400C00u ||
        mode == 0xB8400400u || mode == 0xB8400C00u || mode == 0xF8400400u || mode == 0xF8400C00u) {
      decoded.flags |= kDecodedFlagLoad;
    }
    if (mode == 0x38400400u || mode == 0x38000400u || mode == 0x38400C00u || mode == 0x38000C00u) {
      decoded.aux = 1u;
    } else if (mode == 0x78400400u || mode == 0x78000400u || mode == 0x78400C00u || mode == 0x78000C00u) {
      decoded.aux = 2u;
    } else if (mode == 0xB8400400u || mode == 0xB8000400u || mode == 0xB8400C00u || mode == 0xB8000C00u) {
      decoded.aux = 4u;
    } else {
      decoded.aux = 8u;
      if ((decoded.flags & kDecodedFlagLoad) != 0u) {
        decoded.flags |= kDecodedFlagResult64;
      }
    }
    decoded.aux2 = (mode == 0x38400C00u || mode == 0x38000C00u || mode == 0x78400C00u || mode == 0x78000C00u ||
                    mode == 0xB8400C00u || mode == 0xB8000C00u || mode == 0xF8400C00u || mode == 0xF8000C00u)
        ? 1u
        : 2u;
    return decoded;
  }

  return decoded;
}

bool Cpu::exec_decoded_add_sub_imm(const DecodedInsn& decoded) {
  const bool sf = (decoded.flags & kDecodedFlagSf) != 0u;
  const bool set_flags = (decoded.flags & kDecodedFlagSetFlags) != 0u;
  const bool is_sub = (decoded.flags & kDecodedFlagSub) != 0u;
  const std::uint64_t lhs = sp_or_reg(decoded.rn);
  const std::uint64_t rhs = static_cast<std::uint64_t>(decoded.imm) << decoded.aux;
  const std::uint64_t value = is_sub
      ? (sf ? (lhs - rhs) : static_cast<std::uint32_t>(lhs - rhs))
      : (sf ? (lhs + rhs) : static_cast<std::uint32_t>(lhs + rhs));
  if (set_flags) {
    if (sf) {
      set_reg(decoded.rd, value);
      if (is_sub) {
        set_flags_sub(lhs, rhs, value, false);
      } else {
        set_flags_add(lhs, rhs, value, false);
      }
    } else {
      set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
      if (is_sub) {
        set_flags_sub(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      } else {
        set_flags_add(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      }
    }
  } else {
    set_sp_or_reg(decoded.rd, value, !sf);
  }
  return true;
}

bool Cpu::exec_decoded_add_sub_shifted(const DecodedInsn& decoded) {
  const bool sf = (decoded.flags & kDecodedFlagSf) != 0u;
  const bool set_flags = (decoded.flags & kDecodedFlagSetFlags) != 0u;
  const bool is_sub = (decoded.flags & kDecodedFlagSub) != 0u;
  std::uint64_t rhs = 0;
  if (sf) {
    const std::uint64_t v = reg(decoded.rm);
    const std::uint32_t sh = decoded.aux2 & 63u;
    if (decoded.aux == 0u) {
      rhs = (sh >= 64u) ? 0u : (v << sh);
    } else if (decoded.aux == 1u) {
      rhs = (sh >= 64u) ? 0u : (v >> sh);
    } else {
      rhs = static_cast<std::uint64_t>(static_cast<std::int64_t>(v) >> sh);
    }
  } else {
    const std::uint32_t v = reg32(decoded.rm);
    const std::uint32_t sh = decoded.aux2 & 31u;
    if (decoded.aux == 0u) {
      rhs = static_cast<std::uint32_t>(v << sh);
    } else if (decoded.aux == 1u) {
      rhs = static_cast<std::uint32_t>(v >> sh);
    } else {
      rhs = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> sh);
    }
  }
  const std::uint64_t lhs = sf ? reg(decoded.rn) : reg32(decoded.rn);
  const std::uint64_t value = is_sub
      ? (sf ? (lhs - rhs) : static_cast<std::uint32_t>(lhs - rhs))
      : (sf ? (lhs + rhs) : static_cast<std::uint32_t>(lhs + rhs));
  if (set_flags) {
    if (sf) {
      set_reg(decoded.rd, value);
      if (is_sub) {
        set_flags_sub(lhs, rhs, value, false);
      } else {
        set_flags_add(lhs, rhs, value, false);
      }
    } else {
      set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
      if (is_sub) {
        set_flags_sub(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      } else {
        set_flags_add(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      }
    }
  } else {
    set_sp_or_reg(decoded.rd, value, !sf);
  }
  return true;
}

bool Cpu::exec_decoded_logical_shifted(const DecodedInsn& decoded) {
  const bool sf = (decoded.flags & kDecodedFlagSf) != 0u;
  const bool invert = (decoded.flags & kDecodedFlagInvert) != 0u;
  const std::uint64_t width_mask = sf ? ~0ull : 0xFFFFFFFFull;
  const std::uint32_t sh = sf ? (decoded.imm & 63u) : (decoded.imm & 31u);
  const std::uint64_t rhs_src = sf ? reg(decoded.rm) : static_cast<std::uint64_t>(reg32(decoded.rm));
  std::uint64_t rhs = 0;
  if (decoded.aux == 0u) {
    rhs = (rhs_src << sh);
  } else if (decoded.aux == 1u) {
    rhs = (rhs_src & width_mask) >> sh;
  } else if (decoded.aux == 2u) {
    rhs = sf ? static_cast<std::uint64_t>(static_cast<std::int64_t>(rhs_src) >> sh)
             : static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(rhs_src)) >> sh);
  } else {
    rhs = ror(rhs_src, sh, sf ? 64u : 32u);
  }
  rhs &= width_mask;
  if (invert) {
    rhs = (~rhs) & width_mask;
  }
  const std::uint64_t lhs = sf ? reg(decoded.rn) : static_cast<std::uint64_t>(reg32(decoded.rn));
  std::uint64_t value = 0;
  if (decoded.aux2 == 0u) {
    value = lhs & rhs;
  } else if (decoded.aux2 == 1u) {
    value = lhs | rhs;
  } else if (decoded.aux2 == 2u) {
    value = lhs ^ rhs;
  } else {
    value = lhs & rhs;
  }
  if (decoded.aux2 == 3u) {
    set_flags_logic(value, !sf);
    if (decoded.rd != 31u) {
      if (sf) {
        set_reg(decoded.rd, value);
      } else {
        set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
      }
    }
  } else if (sf) {
    set_reg(decoded.rd, value);
  } else {
    set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
  }
  return true;
}

bool Cpu::exec_decoded_logical_imm(const DecodedInsn& decoded) {
  const bool sf = (decoded.flags & kDecodedFlagSf) != 0u;
  const std::uint32_t n = decoded.aux2 & 1u;
  const std::uint32_t immr = decoded.imm & 0x3Fu;
  const std::uint32_t imms = (decoded.imm >> 6) & 0x3Fu;
  const std::uint32_t datasize = sf ? 64u : 32u;
  std::uint64_t wmask = 0;
  std::uint64_t tmask = 0;
  if (!decode_bit_masks(n, imms, immr, datasize, wmask, tmask)) {
    return false;
  }
  (void)tmask;
  const std::uint64_t lhs = sf ? reg(decoded.rn) : static_cast<std::uint64_t>(reg32(decoded.rn));
  std::uint64_t value = 0;
  if (decoded.aux == 0u) {
    value = lhs & wmask;
  } else if (decoded.aux == 1u) {
    value = lhs | wmask;
  } else if (decoded.aux == 2u) {
    value = lhs ^ wmask;
  } else {
    value = lhs & wmask;
    set_flags_logic(value, !sf);
    if (decoded.rd == 31u) {
      return true;
    }
  }
  if (sf) {
    set_reg(decoded.rd, value);
  } else {
    set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
  }
  return true;
}

bool Cpu::exec_decoded_bitfield(const DecodedInsn& decoded) {
  const bool sf = (decoded.flags & kDecodedFlagSf) != 0u;
  const std::uint32_t n = decoded.aux2 & 1u;
  const std::uint32_t immr = decoded.imm & 0x3Fu;
  const std::uint32_t imms = (decoded.imm >> 6) & 0x3Fu;
  const std::uint32_t datasize = sf ? 64u : 32u;
  std::uint64_t wmask = 0;
  std::uint64_t tmask = 0;
  if (!decode_bit_masks(n, imms, immr, datasize, wmask, tmask)) {
    return false;
  }
  const std::uint64_t src = sf ? reg(decoded.rn) : static_cast<std::uint64_t>(reg32(decoded.rn));
  const std::uint64_t dst = sf ? reg(decoded.rd) : static_cast<std::uint64_t>(reg32(decoded.rd));
  const std::uint64_t bot = ror(src, immr, datasize) & wmask;
  std::uint64_t result = 0;
  if (decoded.aux == 0u) {
    const bool sign = ((src >> (imms & (datasize - 1u))) & 1u) != 0;
    const std::uint64_t top = sign ? ones(datasize) : 0;
    result = (top & ~tmask) | (bot & tmask);
  } else if (decoded.aux == 1u) {
    const std::uint64_t merged = (dst & ~wmask) | bot;
    result = (dst & ~tmask) | (merged & tmask);
  } else if (decoded.aux == 2u) {
    result = bot & tmask;
  } else {
    return false;
  }
  if (sf) {
    set_reg(decoded.rd, result);
  } else {
    set_reg32(decoded.rd, static_cast<std::uint32_t>(result));
  }
  return true;
}

bool Cpu::exec_decoded_load_store_uimm(const DecodedInsn& decoded) {
  if (maybe_take_sp_alignment_fault(decoded.rn)) {
    return true;
  }
  const std::uint64_t addr = sp_or_reg(decoded.rn) + static_cast<std::uint64_t>(decoded.imm);
  const bool is_load = (decoded.flags & kDecodedFlagLoad) != 0u;
  const bool result64 = (decoded.flags & kDecodedFlagResult64) != 0u;
  if (is_load) {
    std::uint64_t value = 0;
    if (maybe_take_data_alignment_fault(addr, decoded.aux, AccessType::Read)) {
      return true;
    }
    if (!mmu_read_value(addr, decoded.aux, &value)) {
      if (exception_taken_this_step_) {
        return true;
      }
      data_abort(addr);
      return true;
    }
    if (result64) {
      set_reg(decoded.rd, value);
    } else if (decoded.aux == 1u) {
      set_reg32(decoded.rd, static_cast<std::uint8_t>(value));
    } else if (decoded.aux == 2u) {
      set_reg32(decoded.rd, static_cast<std::uint16_t>(value));
    } else {
      set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
    }
    return true;
  }

  const std::uint64_t value = (decoded.aux == 8u) ? reg(decoded.rd) : static_cast<std::uint64_t>(reg32(decoded.rd));
  if (maybe_take_data_alignment_fault(addr, decoded.aux, AccessType::Write)) {
    return true;
  }
  if (!mmu_write_value(addr, value, decoded.aux)) {
    if (exception_taken_this_step_) {
      return true;
    }
    data_abort(addr);
    return true;
  }
  return true;
}

const Cpu::DecodedInsn* Cpu::lookup_decoded(std::uint64_t va, std::uint64_t pa, std::uint32_t insn) {
  if (!predecode_enabled_) {
    return nullptr;
  }
  const std::uint64_t va_page = va & ~0xFFFull;
  const std::uint64_t pa_page = pa & ~0xFFFull;
  const std::size_t slot = static_cast<std::size_t>((va >> 2) & (kDecodePageSlots - 1u));

  DecodePage* page = decode_last_page_;
  if (page == nullptr || !page->valid || page->context_epoch != decode_context_epoch_ ||
      page->va_page != va_page || page->pa_page != pa_page) {
    page = &decode_pages_[static_cast<std::size_t>((va_page >> 12) & (kDecodeCachePages - 1u))];
    if (!page->valid || page->context_epoch != decode_context_epoch_ || page->va_page != va_page || page->pa_page != pa_page) {
      page->valid = true;
      page->context_epoch = decode_context_epoch_;
      page->va_page = va_page;
      page->pa_page = pa_page;
      page->valid_words.fill(0);
    }
    decode_last_page_ = page;
  }

  const std::size_t valid_idx = slot >> 6;
  const std::uint64_t valid_bit = 1ull << (slot & 63u);
  if ((page->valid_words[valid_idx] & valid_bit) == 0u || page->raws[slot] != insn) {
    page->insns[slot] = decode_insn(insn);
    page->raws[slot] = insn;
    page->valid_words[valid_idx] |= valid_bit;
  }
  return &page->insns[slot];
}

bool Cpu::exec_decoded(const DecodedInsn& decoded) {
  switch (decoded.kind) {
    case DecodedKind::Fallback:
      return false;
    case DecodedKind::BImm: {
      if ((decoded.flags & kDecodedFlagLink) != 0u) {
        set_reg(30, pc_);
      }
      pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + decoded.simm - 4);
      return true;
    }
    case DecodedKind::BCond: {
      if (condition_holds(decoded.aux)) {
        pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + decoded.simm - 4);
      }
      return true;
    }
    case DecodedKind::Cbz: {
      const bool sf = (decoded.flags & kDecodedFlagSf) != 0u;
      const bool nonzero = (decoded.flags & kDecodedFlagNonZero) != 0u;
      const std::uint64_t value = sf ? reg(decoded.rd) : static_cast<std::uint64_t>(reg32(decoded.rd));
      const bool take = nonzero ? (value != 0u) : (value == 0u);
      if (take) {
        pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + decoded.simm - 4);
      }
      return true;
    }
    case DecodedKind::Tbz: {
      const bool nonzero = (decoded.flags & kDecodedFlagNonZero) != 0u;
      const bool bit_set = ((reg(decoded.rd) >> decoded.aux) & 1u) != 0u;
      const bool take = nonzero ? bit_set : !bit_set;
      if (take) {
        pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + decoded.simm - 4);
      }
      return true;
    }
    case DecodedKind::MovWide: {
      const bool sf = (decoded.flags & kDecodedFlagSf) != 0u;
      const std::uint64_t value = static_cast<std::uint64_t>(decoded.imm) << decoded.aux2;
      if (decoded.aux == 2u) {
        if (sf) {
          set_reg(decoded.rd, value);
        } else {
          set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
        }
      } else if (decoded.aux == 3u) {
        if (sf) {
          const std::uint64_t mask = ~(0xFFFFull << decoded.aux2);
          set_reg(decoded.rd, (reg(decoded.rd) & mask) | value);
        } else {
          if (decoded.aux2 > 16u) {
            return false;
          }
          const std::uint32_t mask = ~(static_cast<std::uint32_t>(0xFFFFu) << decoded.aux2);
          set_reg32(decoded.rd, (reg32(decoded.rd) & mask) | static_cast<std::uint32_t>(value));
        }
      } else if (decoded.aux == 0u) {
        const std::uint64_t neg = ~value;
        if (sf) {
          set_reg(decoded.rd, neg);
        } else {
          if (decoded.aux2 > 16u) {
            return false;
          }
          set_reg32(decoded.rd, static_cast<std::uint32_t>(neg));
        }
      } else {
        return false;
      }
      return true;
    }
    case DecodedKind::AddSubImm:
      return exec_decoded_add_sub_imm(decoded);
    case DecodedKind::AddSubShifted:
      return exec_decoded_add_sub_shifted(decoded);
    case DecodedKind::LogicalShifted:
      return exec_decoded_logical_shifted(decoded);
    case DecodedKind::LogicalImm:
      return exec_decoded_logical_imm(decoded);
    case DecodedKind::Bitfield:
      return exec_decoded_bitfield(decoded);

    case DecodedKind::LoadStoreUImm:
      return exec_decoded_load_store_uimm(decoded);
    case DecodedKind::LoadStore: {
      const bool is_load = (decoded.flags & kDecodedFlagLoad) != 0u;
      const bool is_signed = (decoded.flags & kDecodedFlagSigned) != 0u;
      const bool result64 = (decoded.flags & kDecodedFlagResult64) != 0u;
      if (maybe_take_sp_alignment_fault(decoded.rn)) {
        return true;
      }
      const std::uint64_t base = sp_or_reg(decoded.rn);
      std::uint64_t addr = 0;
      std::uint64_t wb_addr = 0;
      if (decoded.aux2 == 1u) {
        addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + decoded.simm);
        wb_addr = addr;
      } else if (decoded.aux2 == 2u) {
        addr = base;
        wb_addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + decoded.simm);
      } else {
        addr = base + static_cast<std::uint64_t>(decoded.imm);
      }
      if (is_load) {
        std::uint64_t value = 0;
        if (maybe_take_data_alignment_fault(addr, decoded.aux, AccessType::Read)) {
          return true;
        }
        if (!mmu_read_value(addr, decoded.aux, &value)) {
          if (exception_taken_this_step_) {
            return true;
          }
          data_abort(addr);
          return true;
        }
        if (is_signed) {
          const std::uint64_t bits = static_cast<std::uint64_t>(decoded.aux) * 8u;
          const std::uint64_t mask = bits >= 64u ? ~0ull : ((1ull << bits) - 1ull);
          const std::uint64_t signed_value = static_cast<std::uint64_t>(sign_extend(value & mask, static_cast<std::uint32_t>(bits)));
          if (result64) {
            set_reg(decoded.rd, signed_value);
          } else {
            set_reg32(decoded.rd, static_cast<std::uint32_t>(signed_value));
          }
        } else if (result64) {
          set_reg(decoded.rd, value);
        } else {
          set_reg32(decoded.rd, static_cast<std::uint32_t>(value));
        }
      } else {
        std::uint64_t value = 0;
        if (decoded.aux == 8u) {
          value = reg(decoded.rd);
        } else if (decoded.aux == 4u) {
          value = reg32(decoded.rd);
        } else if (decoded.aux == 2u) {
          value = reg32(decoded.rd) & 0xFFFFu;
        } else {
          value = reg32(decoded.rd) & 0xFFu;
        }
        if (maybe_take_data_alignment_fault(addr, decoded.aux, AccessType::Write)) {
          return true;
        }
        if (!mmu_write_value(addr, value, decoded.aux)) {
          if (exception_taken_this_step_) {
            return true;
          }
          data_abort(addr);
          return true;
        }
      }
      if (decoded.aux2 != 0u) {
        set_sp_or_reg(decoded.rn, wb_addr, false);
      }
      return true;
    }
    default:
      return false;
  }
}

void Cpu::set_sp(std::uint64_t value) {
  regs_[31] = value;
  if (sysregs_.current_uses_sp_el0()) {
    sysregs_.set_sp_el0(value);
  } else {
    sysregs_.set_sp_el1(value);
  }
}

void Cpu::save_current_sp_to_bank() {
  if (sysregs_.current_uses_sp_el0()) {
    sysregs_.set_sp_el0(regs_[31]);
  } else {
    sysregs_.set_sp_el1(regs_[31]);
  }
}

void Cpu::load_current_sp_from_bank() {
  regs_[31] = sysregs_.current_uses_sp_el0() ? sysregs_.sp_el0() : sysregs_.sp_el1();
}

bool Cpu::dcc_el0_access_allowed() const {
  return (sysregs_.mdscr_el1() & kMdscrEl1Tdcc) == 0u;
}

std::uint64_t Cpu::read_mdccsr_el0() const {
  return (dcc_rx_full_ ? (1ull << 30) : 0ull) |
         (dcc_tx_full_ ? (1ull << 29) : 0ull);
}

std::uint64_t Cpu::read_mdccint_el1() const {
  return static_cast<std::uint64_t>(dcc_int_enable_ & ((1u << 30) | (1u << 29)));
}

std::uint64_t Cpu::read_dbgdtr_el0() {
  const std::uint64_t value =
      dcc_rx_full_ ? ((static_cast<std::uint64_t>(dcc_tx_data_) << 32) | dcc_rx_data_) : 0u;
  dcc_rx_full_ = false;
  return value;
}

std::uint32_t Cpu::read_dbgdtrrx_el0() {
  const std::uint32_t value = dcc_rx_full_ ? dcc_rx_data_ : 0u;
  dcc_rx_full_ = false;
  return value;
}

std::uint64_t Cpu::read_osdtrrx_el1() const {
  return dcc_rx_data_;
}

std::uint64_t Cpu::read_osdtrtx_el1() const {
  return dcc_tx_data_;
}

void Cpu::write_mdccint_el1(std::uint64_t value) {
  dcc_int_enable_ = static_cast<std::uint32_t>(value) & ((1u << 30) | (1u << 29));
}

void Cpu::write_dbgdtr_el0(std::uint64_t value) {
  dcc_rx_data_ = static_cast<std::uint32_t>(value >> 32);
  dcc_tx_data_ = static_cast<std::uint32_t>(value);
  dcc_tx_full_ = true;
}

void Cpu::write_dbgdtrtx_el0(std::uint32_t value) {
  dcc_tx_data_ = value;
  dcc_tx_full_ = true;
}

void Cpu::write_osdtrrx_el1(std::uint64_t value) {
  dcc_rx_data_ = static_cast<std::uint32_t>(value);
}

void Cpu::write_osdtrtx_el1(std::uint64_t value) {
  dcc_tx_data_ = static_cast<std::uint32_t>(value);
}

std::uint64_t Cpu::read_icc_iar1_el1() {
  if (exception_depth_ != 0 && exception_is_irq_stack_[exception_depth_ - 1]) {
    return exception_intid_stack_[exception_depth_ - 1];
  }
  if (icc_igrpen1_el1_ == 0 || manual_irq_depth_ >= manual_irq_intid_stack_.size()) {
    return 1023u;
  }

  std::uint32_t intid = 1023u;
  if (!gic_.acknowledge(cpu_index_, static_cast<std::uint8_t>(irq_delivery_threshold_ & 0xFFu), intid)) {
    irq_query_epoch_ = gic_.state_epoch();
    irq_query_threshold_ = irq_delivery_threshold_;
    irq_query_negative_valid_ = true;
    return 1023u;
  }
  irq_query_negative_valid_ = false;
  manual_irq_intid_stack_[manual_irq_depth_] = intid;
  manual_irq_prev_prio_stack_[manual_irq_depth_] = running_priority_;
  manual_irq_prio_dropped_stack_[manual_irq_depth_] = false;
  ++manual_irq_depth_;
  running_priority_ = gic_.priority(cpu_index_, intid);
  refresh_irq_threshold_cache();
  return intid;
}

std::uint64_t Cpu::read_icc_hppir1_el1() const {
  if (icc_igrpen1_el1_ == 0) {
    return 1023u;
  }
  std::uint32_t intid = 1023u;
  if (!gic_.highest_pending(cpu_index_, static_cast<std::uint8_t>(irq_delivery_threshold_ & 0xFFu), intid)) {
    return 1023u;
  }
  return intid;
}

void Cpu::icc_priority_drop(std::uint32_t intid) {
  if (exception_depth_ != 0) {
    const std::uint32_t idx = exception_depth_ - 1;
    if (exception_is_irq_stack_[idx] && exception_intid_stack_[idx] == intid) {
      running_priority_ = exception_prev_prio_stack_[idx];
      refresh_irq_threshold_cache();
      exception_prio_dropped_stack_[idx] = true;
      const bool eoi_drop_dir = ((icc_ctlr_el1_ >> 1) & 0x1u) != 0u;
      if (!eoi_drop_dir) {
        gic_.eoi(cpu_index_, intid);
      }
      return;
    }
  }

  if (manual_irq_depth_ == 0) {
    return;
  }
  const std::uint32_t idx = manual_irq_depth_ - 1;
  if (manual_irq_intid_stack_[idx] != intid) {
    return;
  }
  running_priority_ = manual_irq_prev_prio_stack_[idx];
  refresh_irq_threshold_cache();
  manual_irq_prio_dropped_stack_[idx] = true;
  const bool eoi_drop_dir = ((icc_ctlr_el1_ >> 1) & 0x1u) != 0u;
  if (!eoi_drop_dir) {
    gic_.eoi(cpu_index_, intid);
    --manual_irq_depth_;
  }
}

void Cpu::icc_deactivate(std::uint32_t intid) {
  gic_.eoi(cpu_index_, intid);
  if (manual_irq_depth_ == 0) {
    return;
  }
  const std::uint32_t idx = manual_irq_depth_ - 1;
  if (manual_irq_intid_stack_[idx] == intid && manual_irq_prio_dropped_stack_[idx]) {
    --manual_irq_depth_;
  }
}

std::uint64_t Cpu::shared_timer_steps() const {
  return callbacks_.time_steps ? callbacks_.time_steps() : steps_;
}

void Cpu::refresh_local_timer_irq_lines() {
  const bool timer_virt_level = timer_.irq_pending() || timer_.irq_pending_virtual(cpu_index_);
  const bool timer_phys_level = timer_.irq_pending_physical(cpu_index_);
  gic_.set_level(cpu_index_, kTimerVirtPpiIntId, timer_virt_level);
  gic_.set_level(cpu_index_, kTimerPhysPpiIntId, timer_phys_level);
}

void Cpu::parse_pc_watch_list() {
  pc_watch_hits_.clear();
  pc_watch_enabled_ = false;
  const char* env = std::getenv("AARCHVM_TRACE_PC_LIST");
  if (env == nullptr || *env == '\0') {
    return;
  }
  std::stringstream ss{std::string(env)};
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty()) {
      continue;
    }
    try {
      const auto pc = static_cast<std::uint64_t>(std::stoull(item, nullptr, 0));
      pc_watch_hits_.emplace(pc, false);
      pc_watch_enabled_ = true;
    } catch (...) {
    }
  }
}

static std::optional<std::uint64_t> parse_trace_va_env() {
  const char* env = std::getenv("AARCHVM_TRACE_VA");
  if (env == nullptr || *env == '\0') {
    return std::nullopt;
  }
  try {
    return static_cast<std::uint64_t>(std::stoull(env, nullptr, 0));
  } catch (...) {
    return std::nullopt;
  }
}

static std::uint64_t parse_trace_limit_env(const char* name) {
  const char* env = std::getenv(name);
  if (env == nullptr || *env == '\0') {
    return 0;
  }
  try {
    return static_cast<std::uint64_t>(std::stoull(env, nullptr, 0));
  } catch (...) {
    return 0;
  }
}

void Cpu::reset(std::uint64_t pc) {
  const std::uint64_t sp_saved = regs_[31];
  regs_.fill(0);
  for (auto& qreg : qregs_) {
    qreg = {0, 0};
  }
  regs_[31] = sp_saved;
  sysregs_.reset();
  sysregs_.set_sp_el1(sp_saved);
  exception_depth_ = 0;
  exception_is_irq_stack_.fill(false);
  exception_intid_stack_.fill(0);
  exception_prev_prio_stack_.fill(0x100);
  exception_prio_dropped_stack_.fill(false);
  manual_irq_depth_ = 0;
  manual_irq_intid_stack_.fill(0);
  manual_irq_prev_prio_stack_.fill(0x100);
  manual_irq_prio_dropped_stack_.fill(false);
  sync_reported_ = false;
  trace_exceptions_ = (std::getenv("AARCHVM_TRACE_EXC") != nullptr);
  trace_all_exceptions_ = (std::getenv("AARCHVM_TRACE_ALL_EXC") != nullptr);
  trace_brk_ = (std::getenv("AARCHVM_TRACE_BRK") != nullptr);
  parse_pc_watch_list();
  trace_va_ = parse_trace_va_env();
  trace_va_hit_ = false;
  trace_svc_limit_ = parse_trace_limit_env("AARCHVM_TRACE_SVC_LIMIT");
  trace_svc_count_ = 0;
  trace_eret_lower_limit_ = parse_trace_limit_env("AARCHVM_TRACE_ERET_LOWER_LIMIT");
  trace_eret_lower_count_ = 0;
  trace_lower_sync_limit_ = parse_trace_limit_env("AARCHVM_TRACE_LOWER_SYNC_LIMIT");
  trace_lower_sync_count_ = 0;
  waiting_for_interrupt_ = false;
  waiting_for_event_ = false;
  irq_query_epoch_ = 0;
  irq_query_threshold_ = 0;
  irq_query_negative_valid_ = false;
  irq_delivery_threshold_ = 0xFF;
  event_register_ = false;
  icc_pmr_el1_ = 0xFF;
  running_priority_ = 0x100;
  refresh_irq_threshold_cache();
  icc_ctlr_el1_ = 0;
  icc_sre_el1_ = 0;
  icc_bpr1_el1_ = 0;
  icc_igrpen1_el1_ = 0;
  icc_ap0r_el1_.fill(0);
  icc_ap1r_el1_.fill(0);
  dcc_rx_data_ = 0;
  dcc_tx_data_ = 0;
  dcc_int_enable_ = 0;
  dcc_rx_full_ = false;
  dcc_tx_full_ = false;
  clear_exclusive_monitor();
  tlb_flush_all();
  invalidate_decode_all();
  invalidate_ram_page_caches();
  last_translation_fault_.reset();
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();
  pc_ = pc;
  steps_ = 0;
  halted_ = false;
}

void Cpu::set_cntvct(std::uint64_t value) {
  sysregs_.set_cntvct(value);
}

void Cpu::signal_event() {
  event_register_ = true;
}

void Cpu::notify_external_memory_write(std::uint64_t pa, std::size_t size) {
  invalidate_decode_pa(pa, size);
  if (exclusive_monitor_overlaps(pa, size)) {
    event_register_ = true;
    clear_exclusive_monitor();
  }
}

void Cpu::notify_tlbi_vmalle1() {
  tlb_flush_all();
  invalidate_decode_all();
}

void Cpu::notify_tlbi_vae1(std::uint64_t operand, bool all_asids) {
  const std::uint64_t va_page = tlbi_operand_va_page(operand);
  if (all_asids) {
    ++perf_counters_.tlb_flush_va;
    tlb_invalidate_page(va_page, 0u, false);
  } else {
    tlb_flush_va(operand);
  }
  invalidate_decode_va_page(canonicalize_tlbi_operand_va_base(operand));
}

void Cpu::notify_tlbi_aside1(std::uint16_t asid) {
  tlb_flush_asid(asid);
  invalidate_decode_all();
}

void Cpu::notify_ic_ivau() {
  invalidate_decode_all();
}


bool Cpu::fp_asimd_traps_enabled_for_current_el() const {
  const std::uint32_t fpen = static_cast<std::uint32_t>((sysregs_.cpacr_el1() >> 20) & 0x3u);
  if (sysregs_.in_el0()) {
    return fpen != 0x3u;
  }
  return fpen == 0x0u || fpen == 0x2u;
}

bool Cpu::insn_uses_fp_asimd(std::uint32_t insn) const {
  // CPACR_EL1 FP traps apply only to instructions that are architecturally
  // valid and actually use SIMD&FP state. A number of optional encodings live
  // inside the generic 0x0E/0x1E SIMD&FP decode space, but when the
  // corresponding feature is absent they must remain UNDEFINED rather than
  // being misreported as EC=0x07 FP access traps.
  const std::uint64_t pfr0 = sysregs_.id_aa64pfr0_el1();
  const std::uint64_t isar0 = sysregs_.id_aa64isar0_el1();
  const std::uint64_t isar1 = sysregs_.id_aa64isar1_el1();
  if (insn_undef_due_to_absent_fp_asimd_feature(insn, pfr0, isar0, isar1)) {
    return false;
  }

  if ((insn & 0x0E000000u) == 0x0E000000u ||
      (insn & 0x1E000000u) == 0x1E000000u) {
    return true;
  }

  const bool fp_ls_literal =
      (insn & 0x3F000000u) == 0x1C000000u &&
      ((insn >> 30) & 0x3u) != 0x3u;
  if (fp_ls_literal) {
    return true;
  }

  if ((insn & 0xFFE00000u) == 0xD5200000u || (insn & 0xFFE00000u) == 0xD5000000u) {
    const std::uint32_t op0 = (insn >> 19) & 0x3u;
    const std::uint32_t op1 = (insn >> 16) & 0x7u;
    const std::uint32_t crn = (insn >> 12) & 0xFu;
    const std::uint32_t crm = (insn >> 8) & 0xFu;
    const std::uint32_t op2 = (insn >> 5) & 0x7u;
    const std::uint32_t key = sysreg_key(op0, op1, crn, crm, op2);
    return key == sysreg_key(3u, 3u, 4u, 4u, 0u) || // FPCR
           key == sysreg_key(3u, 3u, 4u, 4u, 1u);   // FPSR
  }

  const bool fp_ls_unsigned_imm =
      (insn & 0xFFC00000u) == 0x3D800000u || (insn & 0xFFC00000u) == 0x3DC00000u ||
      (insn & 0xFFC00000u) == 0x3D000000u || (insn & 0xFFC00000u) == 0x3D400000u ||
      (insn & 0xFFC00000u) == 0x7D000000u || (insn & 0xFFC00000u) == 0x7D400000u ||
      (insn & 0xFFC00000u) == 0xBD000000u || (insn & 0xFFC00000u) == 0xBD400000u ||
      (insn & 0xFFC00000u) == 0xFD000000u || (insn & 0xFFC00000u) == 0xFD400000u;
  const bool fp_ls_pre_post =
      (insn & 0xFFC00C00u) == 0x3C800C00u || (insn & 0xFFC00C00u) == 0x3CC00C00u ||
      (insn & 0xFFC00C00u) == 0x3C800400u || (insn & 0xFFC00C00u) == 0x3CC00400u ||
      (insn & 0xFFC00C00u) == 0x3C400C00u || (insn & 0xFFC00C00u) == 0x3C000C00u ||
      (insn & 0xFFC00C00u) == 0x3C400400u || (insn & 0xFFC00C00u) == 0x3C000400u ||
      (insn & 0xFFC00C00u) == 0x7C400C00u || (insn & 0xFFC00C00u) == 0x7C000C00u ||
      (insn & 0xFFC00C00u) == 0x7C400400u || (insn & 0xFFC00C00u) == 0x7C000400u ||
      (insn & 0xFFC00C00u) == 0xBC400C00u || (insn & 0xFFC00C00u) == 0xBC000C00u ||
      (insn & 0xFFC00C00u) == 0xBC400400u || (insn & 0xFFC00C00u) == 0xBC000400u ||
      (insn & 0xFFC00C00u) == 0xFC400C00u || (insn & 0xFFC00C00u) == 0xFC000C00u ||
      (insn & 0xFFC00C00u) == 0xFC400400u || (insn & 0xFFC00C00u) == 0xFC000400u;
  const bool fp_ls_regoffset =
      (insn & 0xFFE00C00u) == 0x3CA00800u || (insn & 0xFFE00C00u) == 0x3CE00800u ||
      (insn & 0xFFE00C00u) == 0x3C200800u || (insn & 0xFFE00C00u) == 0x3C600800u ||
      (insn & 0xFFE00C00u) == 0x7C200800u || (insn & 0xFFE00C00u) == 0x7C600800u ||
      (insn & 0xFFE00C00u) == 0xBC200800u || (insn & 0xFFE00C00u) == 0xBC600800u ||
      (insn & 0xFFE00C00u) == 0xFC200800u || (insn & 0xFFE00C00u) == 0xFC600800u;
  const bool fp_ls_unscaled =
      (insn & 0xFFC00C00u) == 0x3C800000u || (insn & 0xFFC00C00u) == 0x3CC00000u ||
      (insn & 0xFFC00C00u) == 0x3C000000u || (insn & 0xFFC00C00u) == 0x3C400000u ||
      (insn & 0xFFC00C00u) == 0x7C000000u || (insn & 0xFFC00C00u) == 0x7C400000u ||
      (insn & 0xFFC00C00u) == 0xBC000000u || (insn & 0xFFC00C00u) == 0xBC400000u ||
      (insn & 0xFFC00C00u) == 0xFC000000u || (insn & 0xFFC00C00u) == 0xFC400000u;
  const bool fp_ls_pair =
      (insn & 0xFFC00000u) == 0x2D800000u || (insn & 0xFFC00000u) == 0x2C800000u ||
      (insn & 0xFFC00000u) == 0x2D000000u || (insn & 0xFFC00000u) == 0x2C000000u ||
      (insn & 0xFFC00000u) == 0x2DC00000u || (insn & 0xFFC00000u) == 0x2CC00000u ||
      (insn & 0xFFC00000u) == 0x2D400000u || (insn & 0xFFC00000u) == 0x2C400000u ||
      (insn & 0xFFC00000u) == 0x6D800000u || (insn & 0xFFC00000u) == 0x6C800000u ||
      (insn & 0xFFC00000u) == 0x6D000000u || (insn & 0xFFC00000u) == 0x6C000000u ||
      (insn & 0xFFC00000u) == 0x6DC00000u || (insn & 0xFFC00000u) == 0x6CC00000u ||
      (insn & 0xFFC00000u) == 0x6D400000u || (insn & 0xFFC00000u) == 0x6C400000u ||
      (insn & 0xFFC00000u) == 0xAD800000u || (insn & 0xFFC00000u) == 0xAC800000u ||
      (insn & 0xFFC00000u) == 0xAD000000u || (insn & 0xFFC00000u) == 0xAC000000u ||
      (insn & 0xFFC00000u) == 0xADC00000u || (insn & 0xFFC00000u) == 0xACC00000u ||
      (insn & 0xFFC00000u) == 0xAD400000u || (insn & 0xFFC00000u) == 0xAC400000u;
  const bool asimd_structured_ls =
      (insn & 0xBFFFFC00u) == 0x0C407000u || (insn & 0xBFFFFC00u) == 0x0C007000u ||
      (insn & 0xBFE0FC00u) == 0x0CC07000u || (insn & 0xBFE0FC00u) == 0x0C807000u ||
      (insn & 0xBFFFFC00u) == 0x0C40A000u || (insn & 0xBFFFFC00u) == 0x0C00A000u ||
      (insn & 0xBFE0FC00u) == 0x0CC0A000u || (insn & 0xBFE0FC00u) == 0x0C80A000u ||
      (insn & 0xBFFFFC00u) == 0x0C406000u || (insn & 0xBFFFFC00u) == 0x0C006000u ||
      (insn & 0xBFE0FC00u) == 0x0CC06000u || (insn & 0xBFE0FC00u) == 0x0C806000u ||
      (insn & 0xBFFFFC00u) == 0x0C402000u || (insn & 0xBFFFFC00u) == 0x0C002000u ||
      (insn & 0xBFE0FC00u) == 0x0CC02000u || (insn & 0xBFE0FC00u) == 0x0C802000u ||
      (insn & 0xBFFFFC00u) == 0x0C408000u || (insn & 0xBFFFFC00u) == 0x0C008000u ||
      (insn & 0xBFE0FC00u) == 0x0CC08000u || (insn & 0xBFE0FC00u) == 0x0C808000u ||
      (insn & 0xBFFFFC00u) == 0x0C404000u || (insn & 0xBFFFFC00u) == 0x0C004000u ||
      (insn & 0xBFE0FC00u) == 0x0CC04000u || (insn & 0xBFE0FC00u) == 0x0C804000u ||
      (insn & 0xBFFFFC00u) == 0x0C400000u || (insn & 0xBFFFFC00u) == 0x0C000000u ||
      (insn & 0xBFE0FC00u) == 0x0CC00000u || (insn & 0xBFE0FC00u) == 0x0C800000u ||
      (insn & 0xBF9F0000u) == 0x0D000000u || (insn & 0xBF800000u) == 0x0D800000u;
  if (fp_ls_unsigned_imm || fp_ls_pre_post || fp_ls_regoffset || fp_ls_unscaled ||
      fp_ls_pair || asimd_structured_ls) {
    return true;
  }

  return false;
}

void Cpu::trap_fp_asimd_access() {
  enter_sync_exception(pc_ - 4u, 0x07u, 0u, false, 0u);
}

void Cpu::perf_flush_tlb_all() {
  tlb_flush_all();
  invalidate_decode_all();
}

bool Cpu::irq_wakeup_ready() {
  if (sysregs_.irq_masked() || icc_igrpen1_el1_ == 0) {
    return false;
  }
  const std::uint16_t irq_threshold = irq_delivery_threshold_;
  const std::uint64_t irq_epoch = gic_.state_epoch();
  if (irq_query_negative_valid_ &&
      irq_query_epoch_ == irq_epoch &&
      irq_query_threshold_ == irq_threshold) {
    return false;
  }
  if (!gic_.has_pending(cpu_index_, static_cast<std::uint8_t>(irq_threshold & 0xFFu))) {
    irq_query_epoch_ = irq_epoch;
    irq_query_threshold_ = irq_threshold;
    irq_query_negative_valid_ = true;
    return false;
  }
  return true;
}

bool Cpu::ready_to_run() {
  if (halted_) {
    return false;
  }
  if (!waiting_for_interrupt_ && !waiting_for_event_) {
    return true;
  }
  if (waiting_for_interrupt_) {
    return gic_.has_pending(cpu_index_);
  }
  if (event_register_) {
    return true;
  }
  return irq_wakeup_ready();
}

bool Cpu::debug_privilege_matches(std::uint64_t ctrl) const {
  const std::uint32_t privilege = static_cast<std::uint32_t>((ctrl & kDebugCtrlPrivilegeMask) >> 1);
  if (sysregs_.in_el0()) {
    return (privilege & kDebugCtrlPrivilegeEl0) != 0u;
  }
  if (sysregs_.current_el() == 1u) {
    return (privilege & kDebugCtrlPrivilegeEl1) != 0u;
  }
  return false;
}

bool Cpu::maybe_take_breakpoint_exception(std::uint64_t fault_pc) {
  if (!sysregs_.breakpoint_watchpoint_enabled_current()) {
    return false;
  }
  for (std::uint32_t i = 0; i < sysregs_.breakpoint_resource_count(); ++i) {
    const std::uint64_t ctrl = sysregs_.dbgbcr_el1(i);
    if ((ctrl & kDebugCtrlEnable) == 0u) {
      continue;
    }
    if (((ctrl & kDebugCtrlTypeMask) >> 3) != kDebugCtrlTypeExecute) {
      continue;
    }
    if ((ctrl & kDebugCtrlBreakpointBasMask) == 0u || !debug_privilege_matches(ctrl)) {
      continue;
    }
    if ((fault_pc & ~0x3ull) != (sysregs_.dbgbvr_el1(i) & ~0x3ull)) {
      continue;
    }
    enter_sync_exception(fault_pc,
                         sysregs_.in_el0() ? 0x30u : 0x31u,
                         kDebugExceptionIfsc,
                         false,
                         0u);
    return true;
  }
  return false;
}

bool Cpu::maybe_take_watchpoint_exception(std::uint64_t va,
                                          std::size_t size,
                                          AccessType access,
                                          bool cache_maintenance) {
  if (access == AccessType::Fetch || size == 0u || !sysregs_.breakpoint_watchpoint_enabled_current()) {
    return false;
  }
  const bool write = access_is_write(access);
  for (std::uint32_t i = 0; i < sysregs_.watchpoint_resource_count(); ++i) {
    const std::uint64_t ctrl = sysregs_.dbgwcr_el1(i);
    if ((ctrl & kDebugCtrlEnable) == 0u || !debug_privilege_matches(ctrl)) {
      continue;
    }
    const std::uint32_t type = static_cast<std::uint32_t>((ctrl & kDebugCtrlTypeMask) >> 3);
    const std::uint32_t required = write ? kDebugCtrlTypeStore : kDebugCtrlTypeLoad;
    if ((type & required) == 0u) {
      continue;
    }
    const std::uint64_t bas = (ctrl & kDebugCtrlWatchpointBasMask) >> 5;
    if (bas == 0u) {
      continue;
    }
    const std::uint64_t base = sysregs_.dbgwvr_el1(i) & ~0x7ull;
    for (std::size_t offset = 0; offset < size; ++offset) {
      const std::uint64_t byte_va = va + offset;
      if (byte_va < base || byte_va >= (base + 8u)) {
        continue;
      }
      const std::uint32_t bit = static_cast<std::uint32_t>(byte_va - base);
      if (((bas >> bit) & 1u) == 0u) {
        continue;
      }
      enter_sync_exception(pc_ - 4u,
                           sysregs_.in_el0() ? 0x34u : 0x35u,
                           kDebugExceptionIfsc | (write ? (1u << 6) : 0u) |
                               (cache_maintenance ? (1u << 8) : 0u),
                           true,
                           byte_va);
      return true;
    }
  }
  return false;
}

bool Cpu::maybe_take_software_step_before_instruction() {
  if (!sysregs_.software_step_active_pending()) {
    return false;
  }
  enter_sync_exception(pc_, sysregs_.in_el0() ? 0x32u : 0x33u, kDebugExceptionIfsc, false, 0u);
  return true;
}

bool Cpu::is_load_exclusive_instruction(std::uint32_t insn) {
  const std::uint32_t tag = insn & 0xFFE0FC00u;
  if (tag == 0x08407C00u || tag == 0x0840FC00u ||
      tag == 0x48407C00u || tag == 0x4840FC00u ||
      tag == 0x88407C00u || tag == 0x8840FC00u ||
      tag == 0xC8407C00u || tag == 0xC840FC00u ||
      tag == 0x08C0FC00u || tag == 0x48C0FC00u ||
      tag == 0x88C0FC00u || tag == 0xC8C0FC00u) {
    return true;
  }

  const std::uint32_t pair_tag = insn & 0xFFE08000u;
  return pair_tag == 0x88600000u || pair_tag == 0x88608000u ||
         pair_tag == 0xC8600000u || pair_tag == 0xC8608000u;
}

void Cpu::complete_software_step_after_instruction(std::uint64_t next_pc,
                                                   std::uint32_t insn) {
  sysregs_.set_software_step_state(false);
  if (!sysregs_.software_step_active_pending()) {
    return;
  }

  std::uint32_t iss = kDebugExceptionIfsc;
  if (is_load_exclusive_instruction(insn)) {
    iss |= (1u << 24) | (1u << 6);
  } else {
    iss |= (1u << 24);
  }
  enter_sync_exception(next_pc, sysregs_.in_el0() ? 0x32u : 0x33u, iss, false, 0u);
}

std::uint64_t Cpu::captured_pstate_for_sync_exception(std::uint32_t ec) const {
  std::uint64_t saved = sysregs_.pstate_bits();
  if (!stepped_instruction_in_flight_) {
    return saved;
  }

  constexpr std::uint64_t kSpsrSsBit = 1ull << 21;
  if (ec == 0x15u) {
    return saved & ~kSpsrSsBit;
  }
  return saved | kSpsrSsBit;
}

bool Cpu::step() {
  if (halted_) {
    return false;
  }
  exception_taken_this_step_ = false;
  stepped_instruction_in_flight_ = false;
  pc_ = normalize_stage1_address(pc_, true);

  if (waiting_for_interrupt_) {
    if (gic_.has_pending(cpu_index_)) {
      waiting_for_interrupt_ = false;
      if (should_try_irq() && try_take_irq()) {
        ++steps_;
        return true;
      }
      ++steps_;
      return true;
    }
    ++steps_;
    return true;
  }

  if (waiting_for_event_) {
    if (event_register_) {
      event_register_ = false;
      waiting_for_event_ = false;
    } else if (irq_wakeup_ready() && should_try_irq() && try_take_irq()) {
      waiting_for_event_ = false;
      ++steps_;
      return true;
    }
    ++steps_;
    return true;
  }

  if (should_try_irq() && try_take_irq()) {
    ++steps_;
    return true;
  }

  if (maybe_take_software_step_before_instruction()) {
    return true;
  }

  // In AArch64 state the PC must remain word-aligned. This fault has higher
  // priority than Illegal Execution state, so check it before consulting
  // PSTATE.IL.
  if ((pc_ & 0x3u) != 0u) {
    enter_sync_exception(pc_, 0x22u, 0u, true, pc_);
    return true;
  }

  if (sysregs_.illegal_execution_state()) {
    enter_sync_exception(pc_, 0x0Eu, 0u, false, 0u);
    return true;
  }

  if (pc_watch_enabled_) {
    auto pc_watch = pc_watch_hits_.find(pc_);
    if (pc_watch != pc_watch_hits_.end() && !pc_watch->second) {
      std::cerr << "PC-HIT pc=0x" << std::hex << pc_
                << " steps=" << std::dec << steps_
                << " cpu=" << cpu_index_
                << " x0=0x" << std::hex << reg(0)
                << " x1=0x" << reg(1)
                << " x2=0x" << reg(2)
                << " x3=0x" << reg(3)
                << " x29=0x" << reg(29)
                << " x30=0x" << reg(30)
                << " sp=0x" << regs_[31]
                << " pstate=0x" << sysregs_.pstate_bits()
                << " esr=0x" << sysregs_.esr_el1()
                << " far=0x" << sysregs_.far_el1()
                << std::dec << "\n";
      pc_watch->second = true;
    }
  }

  const bool stepped_instruction = sysregs_.software_step_active_not_pending();
  stepped_instruction_in_flight_ = stepped_instruction;
  TranslationResult fetch_result{};
  if (!translate_address(pc_, AccessType::Fetch, &fetch_result, true)) {
    const std::uint32_t iss = last_translation_fault_.has_value() ? fault_status_code(*last_translation_fault_) : 0u;
    enter_sync_exception(pc_, sysregs_.in_el0() ? 0x20u : 0x21u, iss, true, pc_);
    stepped_instruction_in_flight_ = false;
    return true;
  }

  std::uint64_t fetch = 0;
  if (!bus_.read(fetch_result.pa, 4, fetch)) {
    enter_sync_exception(pc_, sysregs_.in_el0() ? 0x20u : 0x21u, sync_external_abort_fsc(), true, pc_);
    stepped_instruction_in_flight_ = false;
    return true;
  }

  const std::uint32_t insn = static_cast<std::uint32_t>(fetch);
  const std::uint64_t this_pc = pc_;
  if (maybe_take_breakpoint_exception(this_pc)) {
    stepped_instruction_in_flight_ = false;
    return true;
  }
  pc_ += 4;
  ++steps_;
  const auto finish_instruction = [&](bool result) {
    stepped_instruction_in_flight_ = false;
    pc_ = normalize_stage1_address(pc_, true);
    if (stepped_instruction && !exception_taken_this_step_ && !halted_) {
      complete_software_step_after_instruction(pc_, insn);
    }
    return result;
  };

  if (insn == 0xD503201Fu) {
    return finish_instruction(true); // NOP
  }
  if ((insn & 0xFFFFFC1Fu) == 0xD65F0000u) {
    const std::uint32_t rn = (insn >> 5) & 0x1Fu;
    const std::uint64_t target = reg(rn);
    if (branch_zero_trace_enabled() && target == 0) {
      std::cerr << std::dec << "BRANCH-ZERO kind=RET rn=" << rn
                << " from=0x" << std::hex << this_pc
                << " el=" << std::dec << static_cast<std::uint32_t>(sysregs_.current_el())
                << " depth=" << exception_depth_
                << " x16=0x" << std::hex << reg(16)
                << " x17=0x" << reg(17)
                << " x30=0x" << reg(30)
                << " sp=0x" << regs_[31];
      TranslationResult br_trace{};
      if (translate_address(reg(16), AccessType::Read, &br_trace, false, false)) {
        std::uint64_t br_mem = 0;
        std::cerr << " x16_pa=0x" << br_trace.pa;
        if (bus_.read(br_trace.pa, 8, br_mem)) {
          std::cerr << " mem[x16]=0x" << br_mem;
        }
      }
      std::cerr << " steps=" << std::dec << steps_ << '\n';
    }
    pc_ = target; // RET Xn
    return finish_instruction(true);
  }
  if ((insn & 0xFFFFFC1Fu) == 0xD61F0000u) {
    const std::uint32_t rn = (insn >> 5) & 0x1Fu;
    const std::uint64_t target = reg(rn);
    if (branch_zero_trace_enabled() && target == 0) {
      std::cerr << std::dec << "BRANCH-ZERO kind=BR rn=" << rn
                << " from=0x" << std::hex << this_pc
                << " el=" << std::dec << static_cast<std::uint32_t>(sysregs_.current_el())
                << " depth=" << exception_depth_
                << " x16=0x" << std::hex << reg(16)
                << " x17=0x" << reg(17)
                << " x30=0x" << reg(30)
                << " sp=0x" << regs_[31];
      TranslationResult br_trace{};
      if (translate_address(reg(16), AccessType::Read, &br_trace, false, false)) {
        std::uint64_t br_mem = 0;
        std::cerr << " x16_pa=0x" << br_trace.pa;
        if (bus_.read(br_trace.pa, 8, br_mem)) {
          std::cerr << " mem[x16]=0x" << br_mem;
        }
      }
      std::cerr << " steps=" << std::dec << steps_ << '\n';
    }
    pc_ = target; // BR Xn
    return finish_instruction(true);
  }
  if ((insn & 0xFFFFFC1Fu) == 0xD63F0000u) {
    const std::uint32_t rn = (insn >> 5) & 0x1Fu;
    const std::uint64_t target = reg(rn);
    set_reg(30, pc_);               // BLR Xn
    if (branch_zero_trace_enabled() && target == 0) {
      std::cerr << std::dec << "BRANCH-ZERO kind=BLR rn=" << rn
                << " from=0x" << std::hex << this_pc
                << " el=" << std::dec << static_cast<std::uint32_t>(sysregs_.current_el())
                << " depth=" << exception_depth_
                << " x16=0x" << std::hex << reg(16)
                << " x17=0x" << reg(17)
                << " x30=0x" << reg(30)
                << " sp=0x" << regs_[31];
      TranslationResult br_trace{};
      if (translate_address(reg(16), AccessType::Read, &br_trace, false, false)) {
        std::uint64_t br_mem = 0;
        std::cerr << " x16_pa=0x" << br_trace.pa;
        if (bus_.read(br_trace.pa, 8, br_mem)) {
          std::cerr << " mem[x16]=0x" << br_mem;
        }
      }
      std::cerr << " steps=" << std::dec << steps_ << '\n';
    }
    pc_ = target;
    return finish_instruction(true);
  }
  if (insn == 0xD69F03E0u) { // ERET
    if (sysregs_.in_el0()) {
      enter_sync_exception(this_pc, 0x00u, 0u, false, 0);
      return finish_instruction(true);
    }
    save_current_sp_to_bank();
    clear_exclusive_monitor();
    const bool illegal_return = sysregs_.illegal_exception_return();
    pc_ = sysregs_.exception_return(illegal_return);
    load_current_sp_from_bank();
    if (trace_eret_lower_count_ < trace_eret_lower_limit_ && sysregs_.in_el0()) {
      ++trace_eret_lower_count_;
      std::cerr << "ERET-EL0: pc=0x" << std::hex << pc_
                << " x0=0x" << reg(0)
                << " x1=0x" << reg(1)
                << " x8=0x" << reg(8)
                << " sp=0x" << regs_[31]
                << " steps=" << std::dec << steps_ << '\n';
    }
    if (exception_depth_ != 0) {
      const std::uint32_t idx = exception_depth_ - 1;
      if (exception_is_irq_stack_[idx] && !exception_prio_dropped_stack_[idx]) {
        running_priority_ = exception_prev_prio_stack_[idx];
        refresh_irq_threshold_cache();
        gic_.eoi(cpu_index_, exception_intid_stack_[idx]);
      }
      --exception_depth_;
    }
    return finish_instruction(true);
  }
  if ((insn & 0xFFE0001Fu) == 0xD4A00001u ||  // DCPS1 #imm16
      (insn & 0xFFE0001Fu) == 0xD4A00002u ||  // DCPS2 #imm16
      (insn & 0xFFE0001Fu) == 0xD4A00003u) {  // DCPS3 #imm16
    // The current model does not implement Debug state, so DCPS<n> is always
    // UNDEFINED in the only architecturally reachable state (Non-debug).
    enter_sync_exception(this_pc, 0x00u, 0u, false, 0u);
    return finish_instruction(true);
  }
  if (insn == 0xD6BF03E0u) { // DRPS
    // DRPS is a Debug-state-only return path. Without Debug state support,
    // Non-debug execution must observe it as UNDEFINED.
    enter_sync_exception(this_pc, 0x00u, 0u, false, 0u);
    return finish_instruction(true);
  }
  if ((insn & 0xFFE0001Fu) == 0xD4200000u) {
    if (trace_brk_) {
      std::cerr << "BRK: pc=0x" << std::hex << this_pc;
      for (std::uint32_t i = 0; i < 31; ++i) {
        std::cerr << " x" << i << "=0x" << regs_[i];
      }
      std::cerr << " sp=0x" << regs_[31];
      TranslationResult x6_pa{};
      if (translate_address(regs_[6], AccessType::Read, &x6_pa, false, false)) {
        std::cerr << " x6_pa=0x" << x6_pa.pa;
      }
      TranslationResult x12_pa{};
      if (translate_address(regs_[12], AccessType::Read, &x12_pa, false, false)) {
        std::cerr << " x12_pa=0x" << x12_pa.pa;
      }
      std::cerr << std::dec << '\n';
    }
    if (brk_halt_mode_enabled()) {
      halted_ = true;
      return finish_instruction(false);
    }
    enter_sync_exception(this_pc, 0x3Cu, (insn >> 5) & 0xFFFFu, false, 0);
    return finish_instruction(true);
  }
  if ((insn & 0xFFE0001Fu) == 0xD4400000u) { // HLT #imm16
    enter_sync_exception(this_pc, 0x00u, 0u, false, 0u);
    return finish_instruction(true);
  }
  if ((insn & 0xFFE0001Fu) == 0xD4000002u) { // HVC #imm16
    if (sysregs_.in_el0()) {
      enter_sync_exception(this_pc, 0x00u, 0u, false, 0);
      return finish_instruction(true);
    }
    if (callbacks_.smccc_call && callbacks_.smccc_call(*this, true, static_cast<std::uint16_t>((insn >> 5) & 0xFFFFu))) {
      return finish_instruction(true);
    }
    enter_sync_exception(this_pc, 0x00u, 0u, false, 0);
    return finish_instruction(true);
  }
  if ((insn & 0xFFE0001Fu) == 0xD4000003u) { // SMC #imm16
    if (sysregs_.in_el0()) {
      enter_sync_exception(this_pc, 0x00u, 0u, false, 0);
      return finish_instruction(true);
    }
    if (callbacks_.smccc_call && callbacks_.smccc_call(*this, false, static_cast<std::uint16_t>((insn >> 5) & 0xFFFFu))) {
      return finish_instruction(true);
    }
    enter_sync_exception(this_pc, 0x00u, 0u, false, 0);
    return finish_instruction(true);
  }
  if ((insn & 0xFFE0001Fu) == 0xD4000001u) { // SVC #imm16
    enter_sync_exception(this_pc, 0x15u, (insn >> 5) & 0xFFFFu, false, 0);
    return finish_instruction(true);
  }

  if (fp_asimd_traps_enabled_for_current_el() && insn_uses_fp_asimd(insn)) {
    trap_fp_asimd_access();
    return finish_instruction(true);
  }

  if (insn_undef_due_to_absent_fp_asimd_feature(
          insn,
          sysregs_.id_aa64pfr0_el1(),
          sysregs_.id_aa64isar0_el1(),
          sysregs_.id_aa64isar1_el1())) {
    enter_sync_exception(this_pc, 0x00u, 0u, false, 0u);
    return finish_instruction(true);
  }

  if (const DecodedInsn* decoded = lookup_decoded(this_pc, fetch_result.pa, insn); decoded != nullptr) {
      switch (decoded->kind) {
        case DecodedKind::AddSubImm:
          if (exec_decoded_add_sub_imm(*decoded)) {
            return finish_instruction(true);
          }
          break;
        case DecodedKind::AddSubShifted:
          if (exec_decoded_add_sub_shifted(*decoded)) {
            return finish_instruction(true);
          }
          break;
        case DecodedKind::LogicalShifted:
          if (exec_decoded_logical_shifted(*decoded)) {
            return finish_instruction(true);
          }
          break;
        case DecodedKind::LogicalImm:
          if (exec_decoded_logical_imm(*decoded)) {
            return finish_instruction(true);
          }
          break;
        case DecodedKind::Bitfield:
          if (exec_decoded_bitfield(*decoded)) {
            return finish_instruction(true);
          }
          break;
        case DecodedKind::LoadStoreUImm:
          if (exec_decoded_load_store_uimm(*decoded)) {
            return finish_instruction(true);
          }
          break;
        default:
          break;
      }
      if (exec_decoded(*decoded)) {
        return finish_instruction(true);
      }
  }
  if (exec_branch(insn)) {
    return finish_instruction(true);
  }
  if (exec_system(insn)) {
    return finish_instruction(true);
  }
  if (exec_data_processing(insn)) {
    return finish_instruction(true);
  }
  if (exec_load_store(insn)) {
    return finish_instruction(true);
  }

  if (trace_exceptions_) {
    std::cerr << "UNIMPL: pc=0x" << std::hex << this_pc
              << " insn=0x" << insn
              << " pstate=0x" << sysregs_.pstate_bits()
              << " sp=0x" << regs_[31]
              << std::dec << '\n';
  }
  enter_sync_exception(this_pc, 0x00u, 0u, false, 0);
  return finish_instruction(true);
}

bool Cpu::try_take_irq() {
  exception_taken_this_step_ = true;
  const std::uint16_t irq_threshold = irq_delivery_threshold_;
  const std::uint64_t irq_epoch = gic_.state_epoch();
  std::uint32_t intid = 1023u;
  if (!gic_.acknowledge(cpu_index_, static_cast<std::uint8_t>(irq_threshold & 0xFFu), intid)) {
    irq_query_epoch_ = irq_epoch;
    irq_query_threshold_ = irq_threshold;
    irq_query_negative_valid_ = true;
    return false;
  }
  irq_query_negative_valid_ = false;

  const bool from_lower_el = sysregs_.in_el0();
  const bool from_spx = sysregs_.use_sp_elx();
  if (exception_depth_ >= exception_is_irq_stack_.size()) {
    halted_ = true;
    return false;
  }
  save_current_sp_to_bank();
  exception_is_irq_stack_[exception_depth_] = true;
  exception_intid_stack_[exception_depth_] = intid;
  exception_prev_prio_stack_[exception_depth_] = running_priority_;
  exception_prio_dropped_stack_[exception_depth_] = false;
  const std::uint8_t new_prio = gic_.priority(cpu_index_, intid);
  running_priority_ = new_prio;
  refresh_irq_threshold_cache();
  if (irq_take_trace_enabled()) {
    std::cerr << std::dec << "IRQ-TAKE intid=" << intid
              << " prio=0x" << std::hex << static_cast<std::uint32_t>(new_prio)
              << " pmr=0x" << static_cast<std::uint32_t>(icc_pmr_el1_ & 0xFFu)
              << " depth=" << std::dec << (exception_depth_ + 1)
              << " pc=0x" << std::hex << pc_ << '\n';
  }
  ++exception_depth_;
  const std::uint64_t saved_pstate = sysregs_.pstate_bits();
  if (sysregs_.software_step_active_not_pending()) {
    sysregs_.set_software_step_state(false);
  }
  sysregs_.exception_enter_irq(pc_, saved_pstate);
  load_current_sp_from_bank();
  pc_ = normalize_stage1_address(sysregs_.vbar_el1() + (from_lower_el ? 0x480u : (from_spx ? 0x280u : 0x80u)),
                                 true);
  return true;
}

void Cpu::enter_sync_exception(std::uint64_t fault_pc,
                               std::uint32_t ec,
                               std::uint32_t iss,
                               bool far_valid,
                               std::uint64_t far) {
  exception_taken_this_step_ = true;
  std::uint64_t return_pc = fault_pc;
  if (ec == 0x15u) {
    // Exception-generating instructions like SVC return to the next instruction.
    return_pc = fault_pc + 4u;
  }
  if (exception_depth_ >= exception_is_irq_stack_.size()) {
    if (trace_exceptions_) {
      std::cerr << "FATAL: exception nesting overflow at PC=0x" << std::hex << pc_
                << " fault_pc=0x" << fault_pc
                << " ec=0x" << ec
                << " iss=0x" << iss
                << " far=0x" << far << std::dec << '\n';
    }
    halted_ = true;
    return;
  }
  const bool nested = exception_depth_ != 0;
  const bool from_lower_el = sysregs_.in_el0();
  const bool from_spx = sysregs_.use_sp_elx();
  const std::uint64_t saved_pstate = captured_pstate_for_sync_exception(ec);
  if (stepped_instruction_in_flight_ || sysregs_.software_step_active_not_pending()) {
    sysregs_.set_software_step_state(false);
  }
  stepped_instruction_in_flight_ = false;
  save_current_sp_to_bank();
  exception_is_irq_stack_[exception_depth_] = false;
  exception_intid_stack_[exception_depth_] = 0;
  exception_prev_prio_stack_[exception_depth_] = running_priority_;
  exception_prio_dropped_stack_[exception_depth_] = true;
  ++exception_depth_;
  if (trace_exceptions_ && nested) {
    std::cerr << "NESTED-SYNC: pc=0x" << std::hex << fault_pc
              << " ec=0x" << ec
              << " iss=0x" << iss
              << " far=0x" << far
              << " depth=" << std::dec << exception_depth_ << '\n';
  }
  if (from_lower_el && trace_lower_sync_count_ < trace_lower_sync_limit_) {
    ++trace_lower_sync_count_;
    std::cerr << "SYNC-EL0: pc=0x" << std::hex << fault_pc
              << " ec=0x" << ec
              << " iss=0x" << iss
              << " far_valid=" << (far_valid ? 1 : 0)
              << " far=0x" << far
              << " steps=" << std::dec << steps_ << '\n';
  }
  const bool log_sync = trace_exceptions_ && (trace_all_exceptions_ || !sync_reported_ || ec == 0u || from_lower_el);
  if (log_sync) {
    std::cerr << "SYNC: pc=0x" << std::hex << fault_pc
              << " ec=0x" << ec
              << " iss=0x" << iss
              << " far_valid=" << (far_valid ? 1 : 0)
              << " far=0x" << far
              << " from_lower=" << (from_lower_el ? 1 : 0)
              << " pstate=0x" << sysregs_.pstate_bits()
              << " sp=0x" << regs_[31]
              << " sp_el0=0x" << sysregs_.sp_el0()
              << " sp_el1=0x" << sysregs_.sp_el1()
              << " tpidr_el1=0x" << sysregs_.tpidr_el1()
              << " ttbr0=0x" << sysregs_.ttbr0_el1()
              << " ttbr1=0x" << sysregs_.ttbr1_el1()
              << " tcr=0x" << sysregs_.tcr_el1()
              << " sctlr=0x" << sysregs_.sctlr_el1();
    TranslationResult trace_fetch{};
    if (translate_address(fault_pc, AccessType::Fetch, &trace_fetch, false, false)) {
      std::uint64_t trace_insn = 0;
      if (bus_.read(trace_fetch.pa, 4, trace_insn)) {
        std::cerr << " insn=0x" << (trace_insn & 0xFFFFFFFFu);
      }
    }
    std::cerr << std::dec << '\n';
    sync_reported_ = true;
  }
  sysregs_.exception_enter_sync(return_pc, saved_pstate, ec, iss, far_valid, far);
  load_current_sp_from_bank();
  pc_ = normalize_stage1_address(sysregs_.vbar_el1() + (from_lower_el ? 0x400u : (from_spx ? 0x200u : 0x0u)),
                                 true);
}

bool Cpu::translate_address(std::uint64_t va,
                            AccessType access,
                            TranslationResult* out_result,
                            bool allow_tlb_fill,
                            bool use_tlb,
                            bool apply_pan) {
  ++perf_counters_.translate_calls;
  last_translation_fault_.reset();
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();

  const std::uint64_t effective_va = normalize_stage1_address(va, access == AccessType::Fetch);

  if (!sysregs_.mmu_enabled()) {
    fill_stage1_disabled_output(effective_va, access, out_result);
    return true;
  }

  const bool va_upper = (effective_va >> 63) != 0;
  const std::uint16_t asid = current_translation_asid(va_upper);
  const std::uint64_t page = (effective_va >> 12) & tlb_page_mask();
  const std::uint64_t off = effective_va & 0xFFFull;

  if (use_tlb) {
    const TlbEntry* hit = nullptr;
    TlbEntry* hot = (access == AccessType::Fetch) ? &tlb_last_fetch_ : &tlb_last_data_;
    if (hot->valid && hot->va_page == page && tlb_entry_matches_asid(*hot, asid)) {
      hit = hot;
    } else {
      ++perf_counters_.tlb_lookups;
      hit = tlb_lookup(page, asid);
      if (hit != nullptr) {
        *hot = *hit;
      }
    }
    if (hit != nullptr) {
      ++perf_counters_.tlb_hits;
      out_result->pa = (hit->pa_page << 12) | off;
      out_result->asid = hit->asid;
      out_result->level = hit->level;
      out_result->attr_index = hit->attr_index;
      out_result->mair_attr = hit->mair_attr;
      out_result->global_entry = hit->global_entry;
      out_result->writable = hit->writable;
      out_result->user_accessible = hit->user_accessible;
      out_result->executable = hit->executable;
      out_result->pxn = hit->pxn;
      out_result->uxn = hit->uxn;
      out_result->memory_type = hit->memory_type;
      out_result->leaf_shareability = hit->leaf_shareability;
      out_result->walk_attrs = hit->walk_attrs;
      TranslationFault fault{};
      if (!access_permitted(*out_result, access, out_result->level, &fault, apply_pan)) {
        last_translation_fault_ = fault;
        return false;
      }
      return true;
    }
    ++perf_counters_.tlb_misses;
  }

  TranslationFault fault{};
  if (!walk_page_tables(effective_va, access, out_result, &fault, true, apply_pan)) {
    last_translation_fault_ = fault;
    return false;
  }

  if (allow_tlb_fill) {
    tlb_insert(page, *out_result);
  }
  return true;
}

bool Cpu::translate_cache_maintenance_address(std::uint64_t va,
                                              TranslationResult* out_result,
                                              bool fault_on_el0_no_read_permission,
                                              bool allow_tlb_fill,
                                              bool use_tlb) {
  ++perf_counters_.translate_calls;
  last_translation_fault_.reset();
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();

  const std::uint64_t effective_va = normalize_stage1_address(va, false);

  if (!sysregs_.mmu_enabled()) {
    fill_stage1_disabled_output(effective_va, AccessType::Read, out_result);
    return true;
  }

  const bool va_upper = (effective_va >> 63) != 0;
  const std::uint16_t asid = current_translation_asid(va_upper);
  const std::uint64_t page = (effective_va >> 12) & tlb_page_mask();
  const std::uint64_t off = effective_va & 0xFFFull;

  if (use_tlb) {
    const TlbEntry* hit = nullptr;
    TlbEntry* hot = &tlb_last_data_;
    if (hot->valid && hot->va_page == page && tlb_entry_matches_asid(*hot, asid)) {
      hit = hot;
    } else {
      ++perf_counters_.tlb_lookups;
      hit = tlb_lookup(page, asid);
      if (hit != nullptr) {
        *hot = *hit;
      }
    }
    if (hit != nullptr) {
      ++perf_counters_.tlb_hits;
      out_result->pa = (hit->pa_page << 12) | off;
      out_result->asid = hit->asid;
      out_result->level = hit->level;
      out_result->attr_index = hit->attr_index;
      out_result->mair_attr = hit->mair_attr;
      out_result->global_entry = hit->global_entry;
      out_result->writable = hit->writable;
      out_result->user_accessible = hit->user_accessible;
      out_result->executable = hit->executable;
      out_result->pxn = hit->pxn;
      out_result->uxn = hit->uxn;
      out_result->memory_type = hit->memory_type;
      out_result->leaf_shareability = hit->leaf_shareability;
      out_result->walk_attrs = hit->walk_attrs;
      if (sysregs_.in_el0() && fault_on_el0_no_read_permission && !out_result->user_accessible) {
        last_translation_fault_ = TranslationFault{
            .kind = TranslationFault::Kind::Permission,
            .level = hit->level,
            .write = false,
        };
        return false;
      }
      return true;
    }
    ++perf_counters_.tlb_misses;
  }

  TranslationFault fault{};
  if (!walk_page_tables(effective_va, AccessType::Read, out_result, &fault, false)) {
    last_translation_fault_ = fault;
    return false;
  }

  if (sysregs_.in_el0() && fault_on_el0_no_read_permission && !out_result->user_accessible) {
    last_translation_fault_ = TranslationFault{
        .kind = TranslationFault::Kind::Permission,
        .level = out_result->level,
        .write = false,
    };
    return false;
  }

  if (allow_tlb_fill) {
    tlb_insert(page, *out_result);
  }
  return true;
}

bool Cpu::walk_page_tables(std::uint64_t va,
                           AccessType access,
                           TranslationResult* out_result,
                           TranslationFault* fault,
                           bool check_permissions,
                           bool apply_pan,
                           bool check_access_flag) {
  ++perf_counters_.page_walks;
  const std::uint64_t effective_va = normalize_stage1_address(va, access == AccessType::Fetch);
  const bool write = access_is_write(access);
  const bool trace_va = trace_va_.has_value() && *trace_va_ == va && !trace_va_hit_;
  const auto trace_va_log = [&](const std::string& msg) {
    if (trace_va && trace_exceptions_) {
      std::cerr << msg << std::dec << std::endl;
    }
  };
  const std::uint64_t tcr = sysregs_.tcr_el1();
  const bool va_upper = (effective_va >> 63) != 0;
  const WalkAttributes walk_attrs = decode_walk_attributes(va_upper);

  const std::uint32_t txsz = va_upper
      ? static_cast<std::uint32_t>((tcr >> 16) & 0x3Fu)
      : static_cast<std::uint32_t>(tcr & 0x3Fu);
  if (txsz > 39) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = write};
    }
    return false;
  }

  const std::uint32_t va_bits = 64u - txsz;
  if (va_bits < 12u || va_bits > 48u) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = write};
    }
    return false;
  }

  const std::uint32_t tg = va_upper
      ? static_cast<std::uint32_t>((tcr >> 30) & 0x3u)
      : static_cast<std::uint32_t>((tcr >> 14) & 0x3u);
  // 4KB granule only: TG0==00, TG1==10.
  if ((!va_upper && tg != 0u) || (va_upper && tg != 2u)) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::Translation, .level = 0, .write = write};
    }
    return false;
  }

  const bool epd = va_upper ? (((tcr >> 23) & 0x1u) != 0) : (((tcr >> 7) & 0x1u) != 0);
  if (epd) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::Translation, .level = 0, .write = write};
    }
    return false;
  }

  const std::uint64_t low_limit = (va_bits == 64u) ? ~0ull : ((1ull << va_bits) - 1ull);
  if (!va_upper && effective_va > low_limit) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = write};
    }
    return false;
  }
  if (va_upper && va_bits < 64u) {
    const std::uint64_t upper_tag_mask = ~low_limit;
    if ((effective_va & upper_tag_mask) != upper_tag_mask) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = write};
      }
      return false;
    }
  }

  std::uint64_t table_base = va_upper ? sysregs_.ttbr1_el1() : sysregs_.ttbr0_el1();
  table_base &= 0x0000FFFFFFFFF000ull;
  if (!pa_within_ips(table_base, walk_attrs.ips_bits)) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = write};
    }
    return false;
  }

  const std::uint32_t levels = (va_bits <= 12u) ? 1u : ((va_bits - 12u + 8u) / 9u);
  const std::uint32_t start_level = 4u - levels;
  if (trace_va) {
    trace_va_hit_ = true;
    std::ostringstream oss;
    oss << "TRACE-VA: va=0x" << std::hex << va
        << " effective_va=0x" << effective_va
        << " access=" << static_cast<unsigned>(access)
        << " txsz=0x" << txsz
        << " va_bits=0x" << va_bits
        << " tg=0x" << tg
        << " epd=" << (epd ? 1 : 0)
        << " ttbr_base=0x" << table_base
        << " levels=" << std::dec << levels
        << " start_level=" << start_level;
    trace_va_log(oss.str());
  }
  if (start_level > 3u) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::Translation, .level = 0, .write = write};
    }
    return false;
  }

  const std::uint64_t idx[4] = {
      (effective_va >> 39) & 0x1FFu,
      (effective_va >> 30) & 0x1FFu,
      (effective_va >> 21) & 0x1FFu,
      (effective_va >> 12) & 0x1FFu,
  };

  TableAttrs inherited{};
  for (std::uint32_t level = start_level; level < 4u; ++level) {
    const std::uint64_t desc_addr = table_base + idx[level] * 8u;
    ++perf_counters_.page_walk_desc_reads;
    std::uint64_t desc = 0;
    const bool desc_ok = bus_.read(desc_addr, 8, desc);
    if (trace_va) {
      std::ostringstream oss;
      oss << "TRACE-VA-L" << level
          << ": idx=0x" << std::hex << idx[level]
          << " table_base=0x" << table_base
          << " desc_addr=0x" << desc_addr;
      if (desc_ok) {
        oss << " desc=0x" << desc;
      } else {
        oss << " desc=<bus-fail>";
      }
      trace_va_log(oss.str());
    }
    if (!desc_ok) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::ExternalAbortOnWalk,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = write};
      }
      return false;
    }

    const bool valid = (desc & 1u) != 0;
    const bool bit1 = (desc & 2u) != 0;
    if (!valid) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::Translation,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = write};
      }
      return false;
    }

    if (level == 3u) {
      if (!bit1) {
        if (fault != nullptr) {
          *fault = TranslationFault{.kind = TranslationFault::Kind::Translation,
                                    .level = static_cast<std::uint8_t>(level),
                                    .write = write};
        }
        return false;
      }

      const bool af = ((desc >> 10) & 0x1u) != 0;
      const bool global_entry = ((desc >> 11) & 0x1u) == 0;
      const std::uint8_t ap = static_cast<std::uint8_t>((desc >> 6) & 0x3u);
      const bool leaf_ro = (ap & 0x2u) != 0;
      const bool user_accessible = !inherited.user_no_access && ((ap & 0x1u) != 0);
      const bool dbm = ((desc >> 51) & 0x1u) != 0;
      const bool pxn = ((desc >> 53) & 0x1u) != 0;
      const bool uxn = ((desc >> 54) & 0x1u) != 0;
      const std::uint8_t attr_index = static_cast<std::uint8_t>((desc >> 2) & 0x7u);
      const std::uint8_t mair_attr = static_cast<std::uint8_t>((sysregs_.mair_el1() >> (attr_index * 8u)) & 0xFFu);
      const std::uint64_t page_base = desc & 0x0000FFFFFFFFF000ull;
      if (!pa_within_ips(page_base, walk_attrs.ips_bits)) {
        if (fault != nullptr) {
          *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize,
                                    .level = static_cast<std::uint8_t>(level),
                                    .write = write};
        }
        return false;
      }

      out_result->pa = page_base | (effective_va & 0xFFFull);
      out_result->asid = current_translation_asid(va_upper);
      out_result->level = static_cast<std::uint8_t>(level);
      out_result->attr_index = attr_index;
      out_result->mair_attr = mair_attr;
      out_result->global_entry = global_entry;
      const bool dbm_writeable = leaf_ro && dbm && dirty_state_hw_update_enabled();
      out_result->writable = !(inherited.write_protect || (leaf_ro && !dbm_writeable));
      out_result->user_accessible = user_accessible;
      out_result->pxn = pxn || inherited.pxn;
      out_result->uxn = uxn || inherited.uxn;
      out_result->executable = !out_result->pxn;
      out_result->memory_type = decode_memory_type(mair_attr);
      out_result->leaf_shareability = static_cast<Shareability>((desc >> 8) & 0x3u);
      out_result->walk_attrs = walk_attrs;
      if (check_access_flag && !af) {
        if (fault != nullptr) {
          *fault = TranslationFault{.kind = TranslationFault::Kind::AccessFlag,
                                    .level = static_cast<std::uint8_t>(level),
                                    .write = write};
        }
        return false;
      }
      if (check_permissions &&
          !access_permitted(*out_result, access, static_cast<std::uint8_t>(level), fault, apply_pan)) {
        return false;
      }
      return true;
    }

    if (bit1) {
      inherited.write_protect = inherited.write_protect || (((desc >> 62) & 0x1u) != 0);
      inherited.user_no_access = inherited.user_no_access || (((desc >> 61) & 0x1u) != 0);
      inherited.uxn = inherited.uxn || (((desc >> 60) & 0x1u) != 0);
      inherited.pxn = inherited.pxn || (((desc >> 59) & 0x1u) != 0);
      table_base = desc & 0x0000FFFFFFFFF000ull;
      if (!pa_within_ips(table_base, walk_attrs.ips_bits)) {
        if (fault != nullptr) {
          *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize,
                                    .level = static_cast<std::uint8_t>(level),
                                    .write = write};
        }
        return false;
      }
      continue;
    }

    if (level == 0u) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::Translation,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = write};
      }
      return false;
    }

    const bool af = ((desc >> 10) & 0x1u) != 0;
    const bool global_entry = ((desc >> 11) & 0x1u) == 0;
    const std::uint8_t ap = static_cast<std::uint8_t>((desc >> 6) & 0x3u);
    const bool leaf_ro = (ap & 0x2u) != 0;
    const bool user_accessible = !inherited.user_no_access && ((ap & 0x1u) != 0);
    const bool dbm = ((desc >> 51) & 0x1u) != 0;
    const bool pxn = ((desc >> 53) & 0x1u) != 0;
    const bool uxn = ((desc >> 54) & 0x1u) != 0;
    const std::uint8_t attr_index = static_cast<std::uint8_t>((desc >> 2) & 0x7u);
    const std::uint8_t mair_attr = static_cast<std::uint8_t>((sysregs_.mair_el1() >> (attr_index * 8u)) & 0xFFu);
    const std::uint32_t page_off_bits = 12u + 9u * (3u - level);
    const std::uint64_t block_mask = (1ull << page_off_bits) - 1ull;
    std::uint64_t pa_base = desc & 0x0000FFFFFFFFF000ull;
    pa_base &= ~block_mask;
    if (!pa_within_ips(pa_base, walk_attrs.ips_bits)) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = write};
      }
      return false;
    }

    out_result->pa = pa_base | (effective_va & block_mask);
    out_result->asid = current_translation_asid(va_upper);
    out_result->level = static_cast<std::uint8_t>(level);
    out_result->attr_index = attr_index;
    out_result->mair_attr = mair_attr;
    out_result->global_entry = global_entry;
    const bool dbm_writeable = leaf_ro && dbm && dirty_state_hw_update_enabled();
    out_result->writable = !(inherited.write_protect || (leaf_ro && !dbm_writeable));
    out_result->user_accessible = user_accessible;
    out_result->pxn = pxn || inherited.pxn;
    out_result->uxn = uxn || inherited.uxn;
    out_result->executable = !out_result->pxn;
    out_result->memory_type = decode_memory_type(mair_attr);
    out_result->leaf_shareability = static_cast<Shareability>((desc >> 8) & 0x3u);
    out_result->walk_attrs = walk_attrs;
    if (check_access_flag && !af) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::AccessFlag,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = write};
      }
      return false;
    }
    if (check_permissions &&
        !access_permitted(*out_result, access, static_cast<std::uint8_t>(level), fault, apply_pan)) {
      if (trace_va) {
        trace_va_hit_ = true;
      }
      return false;
    }
    if (trace_va) {
      trace_va_hit_ = true;
    }
    return true;
  }

  if (trace_va) {
    trace_va_hit_ = true;
  }
  return false;
}

std::uint32_t Cpu::fault_status_code(const TranslationFault& fault) const {
  const std::uint32_t level = fault.level & 0x3u;
  std::uint32_t fsc = 0;
  switch (fault.kind) {
  case TranslationFault::Kind::AddressSize:
    fsc = level;
    break;
  case TranslationFault::Kind::Translation:
    fsc = 0x4u + level;
    break;
  case TranslationFault::Kind::AccessFlag:
    fsc = 0x8u + level;
    break;
  case TranslationFault::Kind::Permission:
    fsc = 0xCu + level;
    break;
  case TranslationFault::Kind::ExternalAbortOnWalk:
    fsc = 0x14u + level;
    break;
  }
  return fsc | (fault.write ? (1u << 6) : 0u);
}

std::uint32_t Cpu::data_abort_iss(const TranslationFault& fault, bool cache_maintenance_or_translation) const {
  std::uint32_t iss = fault_status_code(fault);
  if (cache_maintenance_or_translation) {
    iss |= (1u << 8); // CM
    iss |= (1u << 6); // WnR is architecturally 1 for cache maintenance / AT system instructions.
  }
  return iss;
}

void Cpu::set_par_el1_for_fault(const TranslationFault& fault) {
  const std::uint64_t fst = static_cast<std::uint64_t>(fault_status_code(fault) & 0x3Fu);
  sysregs_.set_par_el1(1ull | (fst << 1) | (1ull << 11));
}

Cpu::WalkAttributes Cpu::decode_walk_attributes(bool va_upper) const {
  const std::uint64_t tcr = sysregs_.tcr_el1();
  const std::uint32_t irgn = va_upper ? static_cast<std::uint32_t>((tcr >> 24) & 0x3u)
                                      : static_cast<std::uint32_t>((tcr >> 8) & 0x3u);
  const std::uint32_t orgn = va_upper ? static_cast<std::uint32_t>((tcr >> 26) & 0x3u)
                                      : static_cast<std::uint32_t>((tcr >> 10) & 0x3u);
  const std::uint32_t sh = va_upper ? static_cast<std::uint32_t>((tcr >> 28) & 0x3u)
                                    : static_cast<std::uint32_t>((tcr >> 12) & 0x3u);
  WalkAttributes attrs{};
  attrs.inner = static_cast<Cacheability>(irgn);
  attrs.outer = static_cast<Cacheability>(orgn);
  attrs.shareability = static_cast<Shareability>(sh);
  attrs.ips_bits = decode_ips_bits();
  return attrs;
}

Cpu::MemoryType Cpu::decode_memory_type(std::uint8_t mair_attr) const {
  if ((mair_attr & 0xF0u) == 0x00u) {
    return MemoryType::Device;
  }
  if ((mair_attr & 0xF0u) != 0x00u) {
    return MemoryType::Normal;
  }
  return MemoryType::Unknown;
}

std::uint8_t Cpu::decode_ips_bits() const {
  switch ((sysregs_.tcr_el1() >> 32) & 0x7u) {
  case 0u: return 32;
  case 1u: return 36;
  case 2u: return 40;
  case 3u: return 42;
  case 4u: return 44;
  case 5u: return 48;
  case 6u: return 52;
  default: return 48;
  }
}

bool Cpu::pa_within_ips(std::uint64_t pa, std::uint8_t ips_bits) const {
  if (ips_bits >= 64u) {
    return true;
  }
  return (pa >> ips_bits) == 0;
}

bool Cpu::dirty_state_hw_update_enabled() const {
  return ((sysregs_.tcr_el1() >> 40) & 0x1u) != 0u;
}

void Cpu::tlb_flush_all() {
  ++perf_counters_.tlb_flush_all;
  tlb_last_fetch_.valid = false;
  tlb_last_data_.valid = false;
  for (auto& set : tlb_entries_) {
    for (auto& entry : set) {
      entry.valid = false;
    }
  }
  tlb_next_replace_.fill(0);
}

std::uint16_t Cpu::mmu_asid_mask() const {
  const std::uint8_t asid_bits_field = static_cast<std::uint8_t>((sysregs_.id_aa64mmfr0_el1() >> 4) & 0xFu);
  return asid_bits_field == 2u ? 0xFFFFu : 0x00FFu;
}

std::uint16_t Cpu::ttbr_asid(std::uint64_t ttbr) const {
  return static_cast<std::uint16_t>((ttbr >> 48) & mmu_asid_mask());
}

std::uint16_t Cpu::current_translation_asid(bool va_upper) const {
  (void)va_upper;
  const bool a1 = ((sysregs_.tcr_el1() >> 22) & 0x1u) != 0;
  const std::uint64_t ttbr = a1 ? sysregs_.ttbr1_el1() : sysregs_.ttbr0_el1();
  return ttbr_asid(ttbr);
}

std::uint16_t Cpu::tlbi_operand_asid(std::uint64_t operand) const {
  return static_cast<std::uint16_t>((operand >> 48) & mmu_asid_mask());
}

void Cpu::tlb_flush_asid(std::uint16_t asid) {
  asid &= mmu_asid_mask();
  if (tlb_last_fetch_.valid && !tlb_last_fetch_.global_entry && tlb_last_fetch_.asid == asid) {
    tlb_last_fetch_.valid = false;
  }
  if (tlb_last_data_.valid && !tlb_last_data_.global_entry && tlb_last_data_.asid == asid) {
    tlb_last_data_.valid = false;
  }
  for (auto& set : tlb_entries_) {
    for (auto& entry : set) {
      if (entry.valid && !entry.global_entry && entry.asid == asid) {
        entry.valid = false;
      }
    }
  }
}

void Cpu::tlb_insert(std::uint64_t va_page, const TranslationResult& result) {
  ++perf_counters_.tlb_inserts;
  TlbEntry entry{};
  entry.valid = true;
  entry.va_page = va_page;
  entry.pa_page = result.pa >> 12;
  entry.asid = static_cast<std::uint16_t>(result.asid & mmu_asid_mask());
  entry.level = result.level;
  entry.attr_index = result.attr_index;
  entry.mair_attr = result.mair_attr;
  entry.global_entry = result.global_entry;
  entry.writable = result.writable;
  entry.user_accessible = result.user_accessible;
  entry.executable = result.executable;
  entry.pxn = result.pxn;
  entry.uxn = result.uxn;
  entry.memory_type = result.memory_type;
  entry.leaf_shareability = result.leaf_shareability;
  entry.walk_attrs = result.walk_attrs;
  tlb_insert_entry(va_page, entry);
}

void Cpu::tlb_insert_entry(std::uint64_t va_page, const TlbEntry& entry) {
  if (entry.valid) {
    tlb_last_data_ = entry;
    tlb_last_fetch_.valid = false;
  }
  auto& set = tlb_entries_[tlb_set_index(va_page)];
  for (TlbEntry& slot : set) {
    if (slot.valid &&
        slot.va_page == va_page &&
        slot.global_entry == entry.global_entry &&
        (slot.global_entry || slot.asid == entry.asid)) {
      TlbEntry updated = entry;
      updated.valid = true;
      updated.va_page = va_page;
      slot = updated;
      return;
    }
  }
  for (TlbEntry& slot : set) {
    if (!slot.valid) {
      TlbEntry updated = entry;
      updated.valid = true;
      updated.va_page = va_page;
      slot = updated;
      return;
    }
  }
  std::uint8_t& next = tlb_next_replace_[tlb_set_index(va_page)];
  TlbEntry updated = entry;
  updated.valid = true;
  updated.va_page = va_page;
  set[next % kTlbWays] = updated;
  next = static_cast<std::uint8_t>((next + 1u) % kTlbWays);
}

void Cpu::tlb_invalidate_page(std::uint64_t va_page, std::uint16_t asid, bool match_asid) {
  asid &= mmu_asid_mask();
  if (tlb_last_fetch_.valid &&
      tlb_entry_covers_va_page(tlb_last_fetch_, va_page) &&
      (!match_asid || tlb_entry_matches_asid(tlb_last_fetch_, asid))) {
    tlb_last_fetch_.valid = false;
  }
  if (tlb_last_data_.valid &&
      tlb_entry_covers_va_page(tlb_last_data_, va_page) &&
      (!match_asid || tlb_entry_matches_asid(tlb_last_data_, asid))) {
    tlb_last_data_.valid = false;
  }
  for (auto& set : tlb_entries_) {
    for (TlbEntry& entry : set) {
      if (entry.valid &&
          tlb_entry_covers_va_page(entry, va_page) &&
          (!match_asid || tlb_entry_matches_asid(entry, asid))) {
        entry.valid = false;
      }
    }
  }
}

void Cpu::tlb_flush_va(std::uint64_t operand) {
  ++perf_counters_.tlb_flush_va;
  const std::uint16_t asid = tlbi_operand_asid(operand);
  tlb_invalidate_page(tlbi_operand_va_page(operand), asid, true);
}

std::uint64_t Cpu::q_reg_lane(std::uint32_t idx, std::size_t lane) const {
  if (idx >= qregs_.size() || lane >= 2u) {
    return 0;
  }
  return qregs_[idx][lane];
}

void Cpu::set_q_reg_lane(std::uint32_t idx, std::size_t lane, std::uint64_t value) {
  if (idx >= qregs_.size() || lane >= 2u) {
    return;
  }
  qregs_[idx][lane] = value;
}

bool Cpu::exec_branch(std::uint32_t insn) {
  // B/BL imm26
  if ((insn & 0x7C000000u) == 0x14000000u) {
    const std::int64_t off = sign_extend((insn & 0x03FFFFFFu) << 2u, 28);
    if ((insn & 0x80000000u) != 0) {
      set_reg(30, pc_);
    }
    pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + off - 4);
    return true;
  }

  // B.cond
  if ((insn & 0xFF000010u) == 0x54000000u) {
    const std::uint32_t cond = insn & 0xFu;
    const std::int64_t off = sign_extend(((insn >> 5) & 0x7FFFFu) << 2u, 21);
    if (condition_holds(cond)) {
      pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + off - 4);
    }
    return true;
  }

  // CBZ/CBNZ (32/64-bit)
  if ((insn & 0x7F000000u) == 0x34000000u || (insn & 0x7F000000u) == 0x35000000u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint32_t rt = insn & 0x1Fu;
    const bool nonzero = (insn & 0x01000000u) != 0;
    const std::int64_t off = sign_extend(((insn >> 5) & 0x7FFFFu) << 2u, 21);
    const std::uint64_t val = sf ? reg(rt) : static_cast<std::uint64_t>(reg32(rt));
    const bool take = nonzero ? (val != 0) : (val == 0);
    if (take) {
      pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + off - 4);
    }
    return true;
  }

  // TBZ/TBNZ
  if ((insn & 0x7F000000u) == 0x36000000u || (insn & 0x7F000000u) == 0x37000000u) {
    const bool test_nonzero = (insn & 0x01000000u) != 0;
    const std::uint32_t b5 = (insn >> 31) & 0x1u;
    const std::uint32_t b40 = (insn >> 19) & 0x1Fu;
    const std::uint32_t bitpos = (b5 << 5) | b40;
    const std::uint32_t rt = insn & 0x1Fu;
    const std::int64_t off = sign_extend(((insn >> 5) & 0x3FFFu) << 2u, 16);
    const std::uint64_t value = reg(rt);
    const bool bit_set = ((value >> bitpos) & 1u) != 0;
    const bool take = test_nonzero ? bit_set : !bit_set;
    if (take) {
      pc_ = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_) + off - 4);
    }
    return true;
  }

  return false;
}

bool Cpu::exec_system(std::uint32_t insn) {
  // HINT instructions. Keep the architecturally visible wait/event semantics,
  // but otherwise keep the current model aligned with the architected
  // absent-feature behavior for each allocated hint encoding.
  if ((insn & 0xFFFFF01Fu) == 0xD503201Fu) {
    const auto trap_wfx = [&](std::uint32_t iss) {
      enter_sync_exception(pc_ - 4u, 0x01u, iss, false, 0u);
      return true;
    };
    switch ((insn >> 5) & 0x7Fu) {
    case 0x02u: // WFE
      if (sysregs_.in_el0() && (sysregs_.sctlr_el1() & kSctlrEl1NTwe) == 0u) {
        return trap_wfx(0x1u);
      }
      if (event_register_) {
        event_register_ = false;
      } else {
        waiting_for_event_ = true;
      }
      return true;
    case 0x03u: // WFI
      if (sysregs_.in_el0() && (sysregs_.sctlr_el1() & kSctlrEl1NTwi) == 0u) {
        return trap_wfx(0x0u);
      }
      waiting_for_interrupt_ = true;
      return true;
    case 0x04u: // SEV
      if (callbacks_.sev_broadcast) {
        callbacks_.sev_broadcast(*this);
      } else {
        event_register_ = true;
      }
      return true;
    case 0x05u: // SEVL
      event_register_ = true;
      return true;
    case 0x27u: // PACM
      // PACM lives in the architectural hint space. When FEAT_PAuth_LR is
      // absent, Arm specifies it as Decode_NOP rather than UNDEFINED.
      return true;
    default:
      return true;
    }
  }

  // DMB <option>
  // The SoC executes guest instructions one step at a time and commits each
  // memory effect synchronously into a single global order, so the current
  // program-visible model is already stronger than the ordering guaranteed by
  // DMB. Keep it as a semantic no-op in this model.
  if ((insn & 0xFFFFF0FFu) == 0xD50330BFu) {
    return true;
  }

  // DSB <option>
  // As with DMB, all prior guest-visible memory effects are already completed
  // before later instructions run, so DSB does not need extra machinery in the
  // current execution model.
  if ((insn & 0xFFFFF0FFu) == 0xD503309Fu) {
    return true;
  }

  // ISB <option>
  // Context-changing operations in this model update CPU-visible state
  // immediately, and decode/TLB invalidation is performed at the point where
  // the architected state changes. That makes ISB a program-visible no-op.
  if ((insn & 0xFFFFF0FFu) == 0xD50330DFu) {
    return true;
  }

  // CLREX <imm4>
  if ((insn & 0xFFFFF0FFu) == 0xD503305Fu) {
    clear_exclusive_monitor();
    return true;
  }

  const auto undefined_current_instruction = [&]() {
    enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
    return true;
  };

  const auto trap_current_system_instruction = [&](std::uint32_t trapped_insn) {
    const std::uint32_t rt = trapped_insn & 0x1Fu;
    const std::uint32_t op0 = (trapped_insn >> 19) & 0x3u;
    const std::uint32_t op1 = (trapped_insn >> 16) & 0x7u;
    const std::uint32_t crn = (trapped_insn >> 12) & 0xFu;
    const std::uint32_t crm = (trapped_insn >> 8) & 0xFu;
    const std::uint32_t op2 = (trapped_insn >> 5) & 0x7u;
    enter_sync_exception(pc_ - 4u,
                         0x18u,
                         sysreg_trap_iss(false, op0, op1, crn, crm, op2, rt),
                         false,
                         0u);
    return true;
  };

  // FEAT_FlagM / FEAT_FlagM2 are absent in the current model, so these
  // system-encoding flag-manipulation instructions must be UNDEFINED rather
  // than turning into EL0 system-access traps via the generic MSR/sysreg path.
  if (insn == 0xD500401Fu ||  // CFINV
      insn == 0xD500405Fu ||  // AXFLAG
      insn == 0xD500403Fu) {  // XAFLAG
    return undefined_current_instruction();
  }

  // Several optional extensions add architected system-encoding instructions
  // that are UNDEFINED when the corresponding feature is absent. If we let
  // them fall through to the generic MSR/sysreg path then EL0 execution is
  // misreported as EC=0x18 system-access trap instead of EC=0 undefined.
  if (insn == 0xD50330FFu ||                        // SB (FEAT_SB)
      (insn & 0xFFFFFFE0u) == 0xD5087900u ||        // AT S1E1RP, Xt (FEAT_PAN2)
      (insn & 0xFFFFFFE0u) == 0xD5087920u ||        // AT S1E1WP, Xt (FEAT_PAN2)
      (insn & 0xFFFFFFE0u) == 0xD5031000u ||        // WFET Xt (FEAT_WFxT)
      (insn & 0xFFFFFFE0u) == 0xD5031020u ||        // WFIT Xt (FEAT_WFxT)
      (insn & 0xFFFFFFE0u) == 0xD50B7380u ||        // CFP RCTX, Xt (FEAT_SPECRES)
      (insn & 0xFFFFFFE0u) == 0xD50B73A0u ||        // DVP RCTX, Xt (FEAT_SPECRES)
      (insn & 0xFFFFFFE0u) == 0xD50B73C0u ||        // COSP RCTX, Xt (FEAT_SPECRES2)
      (insn & 0xFFFFFFE0u) == 0xD50B73E0u ||        // CPP RCTX, Xt (FEAT_SPECRES)
      (insn & 0xFFFFFFE0u) == 0xD50B7700u ||        // GCSPUSHM Xt (FEAT_GCS)
      (insn & 0xFFFFFFE0u) == 0xD50B7740u ||        // GCSSS1 Xt (FEAT_GCS)
      (insn & 0xFFFFFFE0u) == 0xD52B7760u ||        // GCSSS2 Xt (FEAT_GCS)
      (insn & 0xFFFFFFE0u) == 0xD52B7720u ||        // GCSPOPM Xt (FEAT_GCS)
      insn == 0xD508779Fu ||                        // GCSPUSHX (FEAT_GCS)
      insn == 0xD50877DFu ||                        // GCSPOPX (FEAT_GCS)
      insn == 0xD50877BFu) {                        // GCSPOPCX (FEAT_GCS)
    return undefined_current_instruction();
  }

  const auto el0_uci_enabled = [&]() {
    return !sysregs_.in_el0() || (sysregs_.sctlr_el1() & kSctlrEl1Uci) != 0u;
  };

  const auto el0_uct_enabled = [&]() {
    return !sysregs_.in_el0() || (sysregs_.sctlr_el1() & kSctlrEl1Uct) != 0u;
  };

  const auto el0_dze_enabled = [&]() {
    return !sysregs_.in_el0() || (sysregs_.sctlr_el1() & kSctlrEl1Dze) != 0u;
  };

  const auto el0_uma_enabled = [&]() {
    return !sysregs_.in_el0() || (sysregs_.sctlr_el1() & kSctlrEl1Uma) != 0u;
  };

  const auto par_reported_shareability = [&](const TranslationResult& result) -> std::uint64_t {
    if (result.memory_type == MemoryType::Device || result.mair_attr == 0x44u) {
      return 0x2u; // Device and Normal Non-cacheable are reported as Outer Shareable.
    }
    switch (result.leaf_shareability) {
      case Shareability::NonShareable:
        return 0x0u;
      case Shareability::OuterShareable:
        return 0x2u;
      case Shareability::InnerShareable:
        return 0x3u;
      case Shareability::Reserved:
      default:
        return 0x0u;
    }
  };

  const auto par_success_value = [&](const TranslationResult& result) -> std::uint64_t {
    std::uint64_t value = result.pa & 0x0000FFFFFFFFF000ull;
    value |= 1ull << 11; // RES1 in 64-bit PAR format.
    value |= par_reported_shareability(result) << 7;
    value |= static_cast<std::uint64_t>(result.mair_attr) << 56;
    return value;
  };

  const auto update_par_from_translation = [&](std::uint64_t va, bool ok, const TranslationResult& result) {
    if (ok) {
      sysregs_.set_par_el1(par_success_value(result));
    } else if (last_translation_fault_.has_value() &&
               last_translation_fault_->kind == TranslationFault::Kind::ExternalAbortOnWalk) {
      data_abort(va, true);
    } else if (last_translation_fault_.has_value()) {
      set_par_el1_for_fault(*last_translation_fault_);
    } else {
      sysregs_.set_par_el1(1ull);
    }
  };

  const auto el0_timer_sysreg_allowed = [&](std::uint32_t key, bool write) -> bool {
    if (!sysregs_.in_el0()) {
      return true;
    }
    std::uint64_t cntkctl = 0;
    (void)sysregs_.read(3u, 0u, 14u, 1u, 0u, cntkctl);
    switch (key) {
      case sysreg_key(3u, 3u, 14u, 0u, 2u): // CNTVCT_EL0
      case sysreg_key(3u, 3u, 14u, 0u, 1u): // CNTPCT_EL0
        if (write) {
          return false;
        }
        return (cntkctl & (1ull << (key == sysreg_key(3u, 3u, 14u, 0u, 2u) ? 1u : 0u))) != 0u;
      case sysreg_key(3u, 3u, 14u, 3u, 0u): // CNTV_TVAL_EL0
      case sysreg_key(3u, 3u, 14u, 3u, 1u): // CNTV_CTL_EL0
      case sysreg_key(3u, 3u, 14u, 3u, 2u): // CNTV_CVAL_EL0
        return (cntkctl & (1ull << 8u)) != 0u;
      case sysreg_key(3u, 3u, 14u, 2u, 0u): // CNTP_TVAL_EL0
      case sysreg_key(3u, 3u, 14u, 2u, 1u): // CNTP_CTL_EL0
      case sysreg_key(3u, 3u, 14u, 2u, 2u): // CNTP_CVAL_EL0
        return (cntkctl & (1ull << 9u)) != 0u;
      default:
        return false;
    }
  };

  const auto el0_sysreg_access_allowed = [&](std::uint32_t key, bool write) -> bool {
    if (!sysregs_.in_el0()) {
      return true;
    }
    if (el0_timer_sysreg_allowed(key, write)) {
      return true;
    }
    switch (key) {
      case sysreg_key(2u, 3u, 0u, 1u, 0u):   // MDCCSR_EL0
      case sysreg_key(2u, 3u, 0u, 4u, 0u):   // DBGDTR_EL0
      case sysreg_key(2u, 3u, 0u, 5u, 0u):   // DBGDTRRX_EL0 / DBGDTRTX_EL0
        return dcc_el0_access_allowed();
      case sysreg_key(3u, 3u, 4u, 2u, 0u):   // NZCV
      case sysreg_key(3u, 3u, 4u, 4u, 0u):   // FPCR
      case sysreg_key(3u, 3u, 4u, 4u, 1u):   // FPSR
      case sysreg_key(3u, 3u, 13u, 0u, 2u):  // TPIDR_EL0
        return true;
      case sysreg_key(3u, 3u, 4u, 2u, 1u):   // DAIF
        return el0_uma_enabled();
      case sysreg_key(3u, 3u, 0u, 0u, 7u):   // DCZID_EL0
      case sysreg_key(3u, 3u, 13u, 0u, 3u):  // TPIDRRO_EL0
      case sysreg_key(3u, 3u, 14u, 0u, 0u):  // CNTFRQ_EL0
        return !write;
      case sysreg_key(3u, 3u, 0u, 0u, 1u):   // CTR_EL0
        return !write && el0_uct_enabled();
      default:
        return false;
    }
  };

  const auto amu_feature_level =
      static_cast<std::uint32_t>((sysregs_.id_aa64pfr0_el1() >> 44) & 0xFu);
  const bool have_amuv1 = amu_feature_level >= 1u;
  const bool have_amuv1p1 = amu_feature_level >= 2u;

  const auto is_break_watch_sysreg = [&](std::uint32_t key) -> bool {
    const std::uint32_t op0 = (key >> 14) & 0x3u;
    const std::uint32_t op1 = (key >> 11) & 0x7u;
    const std::uint32_t crn = (key >> 7) & 0xFu;
    const std::uint32_t crm = (key >> 3) & 0xFu;
    const std::uint32_t op2 = key & 0x7u;
    return op0 == 2u && op1 == 0u && crn == 0u && crm < 16u &&
           (op2 == 4u || op2 == 5u || op2 == 6u || op2 == 7u);
  };

  const auto software_access_debug_event_should_halt = [&](std::uint32_t key) -> bool {
    if (!is_break_watch_sysreg(key) || (sysregs_.mdscr_el1() & kMdscrEl1Tda) == 0u) {
      return false;
    }
    std::uint64_t oslsr = 0;
    if (!sysregs_.read(2u, 0u, 1u, 1u, 4u, oslsr)) {
      return false;
    }
    return (oslsr & (1ull << 1)) == 0u;
  };

  const auto read_mdscr_el1 = [&]() -> std::uint64_t {
    std::uint64_t value = sysregs_.mdscr_el1();
    value &= ~(kMdscrEl1Rxfull | kMdscrEl1Txfull);
    if (dcc_rx_full_) {
      value |= kMdscrEl1Rxfull;
    }
    if (dcc_tx_full_) {
      value |= kMdscrEl1Txfull;
    }
    return value;
  };

  const auto write_mdscr_el1 = [&](std::uint64_t value) {
    std::uint64_t oslsr = 0u;
    const bool os_lock_active =
        sysregs_.read(2u, 0u, 1u, 1u, 4u, oslsr) && ((oslsr & (1ull << 1)) != 0u);
    (void)sysregs_.write(2u, 0u, 0u, 2u, 2u, value);
    if (os_lock_active) {
      dcc_rx_full_ = (value & kMdscrEl1Rxfull) != 0u;
      dcc_tx_full_ = (value & kMdscrEl1Txfull) != 0u;
    }
  };

  const auto is_amu_v1_sysreg = [&](std::uint32_t key) -> bool {
    const std::uint32_t op0 = (key >> 14) & 0x3u;
    const std::uint32_t op1 = (key >> 11) & 0x7u;
    const std::uint32_t crn = (key >> 7) & 0xFu;
    const std::uint32_t crm = (key >> 3) & 0xFu;
    const std::uint32_t op2 = key & 0x7u;
    if (op0 != 3u || op1 != 3u || crn != 13u) {
      return false;
    }
    if (crm == 2u) {
      return op2 <= 5u; // AMCR/AMCFGR/AMCGCR/AMUSERENR/AMCNTENCLR0/AMCNTENSET0
    }
    if (crm == 3u) {
      return op2 <= 1u; // AMCNTENCLR1/AMCNTENSET1
    }
    switch (crm) {
      case 4u:  // AMEVCNTR0<m>_EL0, m[3]=0
      case 5u:  // AMEVCNTR0<m>_EL0, m[3]=1
      case 6u:  // AMEVTYPER0<m>_EL0, m[3]=0
      case 7u:  // AMEVTYPER0<m>_EL0, m[3]=1
      case 12u: // AMEVCNTR1<m>_EL0, m[3]=0
      case 13u: // AMEVCNTR1<m>_EL0, m[3]=1
      case 14u: // AMEVTYPER1<m>_EL0, m[3]=0
      case 15u: // AMEVTYPER1<m>_EL0, m[3]=1
        return true;
      default:
        return false;
    }
  };

  const auto is_amu_v1p1_sysreg = [&](std::uint32_t key) -> bool {
    const std::uint32_t op0 = (key >> 14) & 0x3u;
    const std::uint32_t op1 = (key >> 11) & 0x7u;
    const std::uint32_t crn = (key >> 7) & 0xFu;
    const std::uint32_t crm = (key >> 3) & 0xFu;
    const std::uint32_t op2 = key & 0x7u;
    if (op0 == 3u && op1 == 3u && crn == 13u && crm == 2u && op2 == 6u) {
      return true; // AMCG1IDR_EL0
    }
    if (op0 != 3u || op1 != 4u || crn != 13u) {
      return false;
    }
    switch (crm) {
      case 8u:  // AMEVCNTVOFF0<m>_EL2, m[3]=0
      case 9u:  // AMEVCNTVOFF0<m>_EL2, m[3]=1
      case 10u: // AMEVCNTVOFF1<m>_EL2, m[3]=0
      case 11u: // AMEVCNTVOFF1<m>_EL2, m[3]=1
        return true;
      default:
        return false;
    }
  };

  const auto sysreg_present = [&](std::uint32_t key) -> bool {
    if (!have_amuv1 && is_amu_v1_sysreg(key)) {
      return false;
    }
    if (!have_amuv1p1 && is_amu_v1p1_sysreg(key)) {
      return false;
    }
    {
      const std::uint32_t op0 = (key >> 14) & 0x3u;
      const std::uint32_t op1 = (key >> 11) & 0x7u;
      const std::uint32_t crn = (key >> 7) & 0xFu;
      const std::uint32_t crm = (key >> 3) & 0xFu;
      const std::uint32_t op2 = key & 0x7u;
      if (op0 == 2u && op1 == 0u && crn == 0u && crm < 16u) {
        if (op2 == 4u || op2 == 5u) {
          return crm < sysregs_.breakpoint_resource_count();
        }
        if (op2 == 6u || op2 == 7u) {
          return crm < sysregs_.watchpoint_resource_count();
        }
      }
    }
    switch (key) {
      case sysreg_key(3u, 3u, 13u, 0u, 5u):  // TPIDR2_EL0
      case sysreg_key(3u, 3u, 2u, 5u, 1u):   // GCSPR_EL0
      case sysreg_key(3u, 3u, 2u, 4u, 0u):   // RNDR
      case sysreg_key(3u, 3u, 2u, 4u, 1u):   // RNDRRS
      case sysreg_key(3u, 3u, 14u, 15u, 7u): // PMCCFILTR_EL0
      case sysreg_key(3u, 3u, 9u, 13u, 0u):  // PMCCNTR_EL0
      case sysreg_key(3u, 3u, 9u, 12u, 6u):  // PMCEID0_EL0
      case sysreg_key(3u, 3u, 9u, 12u, 7u):  // PMCEID1_EL0
      case sysreg_key(3u, 3u, 9u, 12u, 2u):  // PMCNTENCLR_EL0
      case sysreg_key(3u, 3u, 9u, 12u, 1u):  // PMCNTENSET_EL0
      case sysreg_key(3u, 3u, 9u, 12u, 0u):  // PMCR_EL0
      case sysreg_key(3u, 0u, 9u, 14u, 2u):  // PMINTENCLR_EL1
      case sysreg_key(3u, 0u, 9u, 14u, 1u):  // PMINTENSET_EL1
      case sysreg_key(3u, 0u, 9u, 14u, 6u):  // PMMIR_EL1
      case sysreg_key(3u, 0u, 9u, 14u, 5u):  // PMECR_EL1
      case sysreg_key(3u, 0u, 9u, 14u, 7u):  // PMIAR_EL1
      case sysreg_key(2u, 0u, 14u, 11u, 7u): // PMCCNTSVR_EL1
      case sysreg_key(3u, 3u, 9u, 6u, 0u):   // PMICFILTR_EL0
      case sysreg_key(3u, 3u, 9u, 4u, 0u):   // PMICNTR_EL0
      case sysreg_key(2u, 0u, 14u, 12u, 0u): // PMICNTSVR_EL1
      case sysreg_key(3u, 0u, 9u, 14u, 4u):  // PMUACR_EL1
      case sysreg_key(3u, 3u, 9u, 12u, 3u):  // PMOVSCLR_EL0
      case sysreg_key(3u, 3u, 9u, 14u, 3u):  // PMOVSSET_EL0
      case sysreg_key(3u, 3u, 9u, 12u, 5u):  // PMSELR_EL0
      case sysreg_key(3u, 3u, 9u, 12u, 4u):  // PMSWINC_EL0
      case sysreg_key(3u, 3u, 9u, 13u, 4u):  // PMZR_EL0
      case sysreg_key(3u, 0u, 9u, 9u, 0u):   // PMSCR_EL1
      case sysreg_key(3u, 4u, 9u, 9u, 0u):   // PMSCR_EL2
      case sysreg_key(3u, 0u, 9u, 10u, 4u):  // PMSDSFR_EL1
      case sysreg_key(3u, 0u, 9u, 9u, 5u):   // PMSEVFR_EL1
      case sysreg_key(3u, 0u, 9u, 9u, 4u):   // PMSFCR_EL1
      case sysreg_key(3u, 0u, 9u, 9u, 2u):   // PMSICR_EL1
      case sysreg_key(3u, 0u, 9u, 9u, 7u):   // PMSIDR_EL1
      case sysreg_key(3u, 0u, 9u, 9u, 3u):   // PMSIRR_EL1
      case sysreg_key(3u, 0u, 9u, 9u, 6u):   // PMSLATFR_EL1
      case sysreg_key(3u, 0u, 9u, 9u, 1u):   // PMSNEVFR_EL1
      case sysreg_key(3u, 0u, 9u, 10u, 7u):  // PMBIDR_EL1
      case sysreg_key(3u, 0u, 9u, 10u, 0u):  // PMBLIMITR_EL1
      case sysreg_key(3u, 0u, 9u, 10u, 5u):  // PMBMAR_EL1
      case sysreg_key(3u, 0u, 9u, 10u, 1u):  // PMBPTR_EL1
      case sysreg_key(3u, 0u, 9u, 10u, 3u):  // PMBSR_EL1
      case sysreg_key(3u, 4u, 9u, 10u, 3u):  // PMBSR_EL2
      case sysreg_key(3u, 6u, 9u, 10u, 3u):  // PMBSR_EL3
      case sysreg_key(3u, 3u, 13u, 0u, 7u):  // SCXTNUM_EL0
      case sysreg_key(3u, 0u, 13u, 0u, 7u):  // SCXTNUM_EL1
      case sysreg_key(3u, 0u, 13u, 0u, 6u):  // RCWMASK_EL1
      case sysreg_key(3u, 0u, 13u, 0u, 3u):  // RCWSMASK_EL1
      case sysreg_key(3u, 3u, 10u, 2u, 4u):  // POR_EL0
      case sysreg_key(3u, 0u, 1u, 2u, 1u):   // TRFCR_EL1
      case sysreg_key(3u, 3u, 4u, 2u, 2u):   // SVCR
      case sysreg_key(3u, 0u, 1u, 2u, 6u):   // SMCR_EL1
      case sysreg_key(3u, 4u, 1u, 2u, 6u):   // SMCR_EL2
      case sysreg_key(3u, 6u, 1u, 2u, 6u):   // SMCR_EL3
      case sysreg_key(3u, 3u, 9u, 13u, 2u):  // PMXEVCNTR_EL0
      case sysreg_key(3u, 3u, 9u, 13u, 1u):  // PMXEVTYPER_EL0
      case sysreg_key(3u, 3u, 9u, 14u, 0u):  // PMUSERENR_EL0
      case sysreg_key(3u, 0u, 4u, 3u, 0u):   // ALLINT (FEAT_NMI absent)
      case sysreg_key(3u, 0u, 4u, 3u, 1u):   // PM (FEAT_EBEP absent)
      case sysreg_key(3u, 0u, 4u, 2u, 4u):   // UAO
      case sysreg_key(3u, 3u, 4u, 2u, 5u):   // DIT
      case sysreg_key(3u, 3u, 4u, 2u, 6u):   // SSBS
      case sysreg_key(3u, 3u, 4u, 2u, 7u):   // TCO
        return false;
      default:
        return true;
    }
  };

  const auto linux_el0_cpuid_trap_sysreg = [&](std::uint32_t key) -> bool {
    const std::uint32_t op0 = (key >> 14) & 0x3u;
    const std::uint32_t op1 = (key >> 11) & 0x7u;
    const std::uint32_t crn = (key >> 7) & 0xFu;
    const std::uint32_t crm = (key >> 3) & 0xFu;
    const std::uint32_t op2 = key & 0x7u;

    // Match Linux arm64's EL0 CPUID emulation space in do_emulate_mrs():
    // Op0=3, Op1=0, CRn=0, CRm={0,2..7}. Only MIDR/MPIDR/REVIDR are defined
    // in CRm==0; the remaining CRm==2..7 space is sanitized/RAZ by the guest.
    if (op0 != 3u || op1 != 0u || crn != 0u) {
      return false;
    }
    if (crm == 0u) {
      return op2 == 0u || op2 == 5u || op2 == 6u;
    }
    return crm >= 2u && crm <= 7u;
  };

  const auto el0_sysreg_undefined = [&](std::uint32_t key, bool write) -> bool {
    if (!sysregs_.in_el0()) {
      return false;
    }
    if (key == sysreg_key(3u, 0u, 4u, 2u, 0u) ||   // SPSel
        key == sysreg_key(3u, 0u, 4u, 1u, 0u) ||   // SP_EL0
        key == sysreg_key(3u, 4u, 4u, 1u, 0u) ||   // SP_EL1
        key == sysreg_key(3u, 0u, 4u, 2u, 2u) ||   // CurrentEL
        key == sysreg_key(3u, 0u, 4u, 2u, 3u) ||   // PAN
        key == sysreg_key(2u, 0u, 0u, 0u, 2u) ||   // OSDTRRX_EL1
        key == sysreg_key(2u, 0u, 0u, 2u, 0u) ||   // MDCCINT_EL1
        key == sysreg_key(2u, 0u, 0u, 2u, 2u) ||   // MDSCR_EL1
        key == sysreg_key(2u, 0u, 0u, 3u, 2u) ||   // OSDTRTX_EL1
        key == sysreg_key(2u, 0u, 0u, 6u, 2u) ||   // OSECCR_EL1
        key == sysreg_key(2u, 0u, 1u, 0u, 0u) ||   // MDRAR_EL1
        key == sysreg_key(2u, 0u, 1u, 4u, 4u) ||   // DBGPRCR_EL1
        key == sysreg_key(2u, 0u, 1u, 0u, 4u) ||   // OSLAR_EL1
        key == sysreg_key(2u, 0u, 1u, 1u, 4u) ||   // OSLSR_EL1
        key == sysreg_key(2u, 0u, 1u, 3u, 4u) ||   // OSDLR_EL1
        key == sysreg_key(2u, 0u, 7u, 8u, 6u) ||   // DBGCLAIMSET_EL1
        key == sysreg_key(2u, 0u, 7u, 9u, 6u) ||   // DBGCLAIMCLR_EL1
        key == sysreg_key(2u, 0u, 7u, 14u, 6u) ||  // DBGAUTHSTATUS_EL1
        key == sysreg_key(3u, 3u, 4u, 5u, 0u) ||   // DSPSR_EL0
        key == sysreg_key(3u, 3u, 4u, 5u, 1u)) {   // DLR_EL0
      return true;
    }
    // Linux arm64 exposes HWCAP_CPUID by trapping a sanitized subset of MRS
    // ID-space reads from EL0 and emulating them in the kernel. Keep that
    // space as a system-access trap instead of collapsing it to UNDEFINED so
    // guest userspace observes the expected Linux behaviour.
    return !write && !linux_el0_cpuid_trap_sysreg(key) &&
           (key == sysreg_key(3u, 0u, 0u, 0u, 0u) ||  // MIDR_EL1
            key == sysreg_key(3u, 0u, 0u, 0u, 5u) ||  // MPIDR_EL1
            key == sysreg_key(3u, 0u, 0u, 0u, 6u) ||  // REVIDR_EL1
            key == sysreg_key(3u, 1u, 0u, 0u, 0u) ||  // CCSIDR_EL1
            key == sysreg_key(3u, 1u, 0u, 0u, 1u) ||  // CLIDR_EL1
            key == sysreg_key(3u, 0u, 0u, 4u, 0u) ||  // ID_AA64PFR0_EL1
            key == sysreg_key(3u, 0u, 0u, 4u, 1u) ||  // ID_AA64PFR1_EL1
            key == sysreg_key(3u, 0u, 0u, 4u, 2u) ||  // ID_AA64PFR2_EL1
            key == sysreg_key(3u, 0u, 0u, 4u, 4u) ||  // ID_AA64ZFR0_EL1
            key == sysreg_key(3u, 0u, 0u, 4u, 5u) ||  // ID_AA64SMFR0_EL1
            key == sysreg_key(3u, 0u, 0u, 4u, 7u) ||  // reserved ID space
            key == sysreg_key(3u, 0u, 0u, 5u, 0u) ||  // ID_AA64DFR0_EL1
            key == sysreg_key(3u, 0u, 0u, 5u, 1u) ||  // ID_AA64DFR1_EL1
            key == sysreg_key(3u, 0u, 0u, 6u, 0u) ||  // ID_AA64ISAR0_EL1
            key == sysreg_key(3u, 0u, 0u, 6u, 1u) ||  // ID_AA64ISAR1_EL1
            key == sysreg_key(3u, 0u, 0u, 6u, 2u) ||  // ID_AA64ISAR2_EL1
            key == sysreg_key(3u, 0u, 0u, 6u, 3u) ||  // ID_AA64ISAR3_EL1
            key == sysreg_key(3u, 0u, 0u, 7u, 0u) ||  // ID_AA64MMFR0_EL1
            key == sysreg_key(3u, 0u, 0u, 7u, 1u) ||  // ID_AA64MMFR1_EL1
            key == sysreg_key(3u, 0u, 0u, 7u, 2u) ||  // ID_AA64MMFR2_EL1
            key == sysreg_key(3u, 0u, 0u, 7u, 3u) ||  // ID_AA64MMFR3_EL1
            key == sysreg_key(3u, 0u, 0u, 7u, 4u));   // ID_AA64MMFR4_EL1
  };

  // TLBI VMALLE1 / VMALLE1IS
  if (insn == 0xD508871Fu || insn == 0xD508831Fu) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    tlb_flush_all();
    invalidate_decode_all();
    if (callbacks_.tlbi_vmalle1_broadcast) {
      callbacks_.tlbi_vmalle1_broadcast(*this);
    }
    return true;
  }

  // TLBI VAE1/VAE1IS/VALE1/VALE1IS/VAAE1/VAAE1IS/VAALE1/VAALE1IS, Xt.
  if ((insn & 0xFFFFFFE0u) == 0xD5088720u ||
      (insn & 0xFFFFFFE0u) == 0xD5088320u ||
      (insn & 0xFFFFFFE0u) == 0xD50887A0u ||
      (insn & 0xFFFFFFE0u) == 0xD50883A0u ||
      (insn & 0xFFFFFFE0u) == 0xD5088760u ||
      (insn & 0xFFFFFFE0u) == 0xD5088360u ||
      (insn & 0xFFFFFFE0u) == 0xD50887E0u ||
      (insn & 0xFFFFFFE0u) == 0xD50883E0u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    const std::uint32_t rt = insn & 0x1Fu;
    const std::uint64_t operand = reg(rt);
    // For the VAE1/VALE1/VAAE1/VAALE1 family, bit[6] selects the AA
    // "all ASIDs" variants, while bit[7] selects the LE "last level" forms.
    // This model caches only final leaf translations, so LE shares the same
    // invalidation scope as the non-LE form, but AA must still be decoded
    // correctly.
    const bool all_asids = (insn & 0x40u) != 0;
    const std::uint64_t va_page = tlbi_operand_va_page(operand);
    if (all_asids) {
      ++perf_counters_.tlb_flush_va;
      tlb_invalidate_page(va_page, 0u, false);
    } else {
      tlb_flush_va(operand);
    }
    invalidate_decode_va_page(canonicalize_tlbi_operand_va_base(operand));
    if (callbacks_.tlbi_vae1_broadcast) {
      callbacks_.tlbi_vae1_broadcast(*this, operand, all_asids);
    }
    return true;
  }

  // TLBI ASIDE1 / ASIDE1IS, Xt.
  if ((insn & 0xFFFFFFE0u) == 0xD5088740u ||
      (insn & 0xFFFFFFE0u) == 0xD5088340u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    const std::uint16_t asid = tlbi_operand_asid(reg(insn & 0x1Fu));
    tlb_flush_asid(asid);
    invalidate_decode_all();
    if (callbacks_.tlbi_aside1_broadcast) {
      callbacks_.tlbi_aside1_broadcast(*this, asid);
    }
    return true;
  }

  // IC IALLU
  if (insn == 0xD508751Fu) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    invalidate_decode_all();
    if (callbacks_.ic_ivau_broadcast) {
      callbacks_.ic_ivau_broadcast(*this);
    }
    return true;
  }

  // IC IALLUIS
  if (insn == 0xD508711Fu) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    invalidate_decode_all();
    if (callbacks_.ic_ivau_broadcast) {
      callbacks_.ic_ivau_broadcast(*this);
    }
    return true;
  }

  // IC IVAU, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7520u) {
    if (!el0_uci_enabled()) {
      return trap_current_system_instruction(insn);
    }
    const std::uint32_t rt = insn & 0x1Fu;
    TranslationResult result{};
    if (!translate_cache_maintenance_address(reg(rt), &result, true)) {
      data_abort(reg(rt), true);
      return true;
    }
    invalidate_decode_va(normalize_stage1_address(reg(rt), false), 1u);
    if (callbacks_.ic_ivau_broadcast) {
      callbacks_.ic_ivau_broadcast(*this);
    }
    return true;
  }

  // DC IVAC, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087620u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    const std::uint32_t rt = insn & 0x1Fu;
    const std::uint64_t line_base = reg(rt) & ~(static_cast<std::uint64_t>(kDataCacheLineBytes) - 1u);
    TranslationResult result{};
    // FEAT_PAN does not affect data cache maintenance instructions other than
    // DC ZVA/GVA-family operations, but DC IVAC still requires write
    // permission to the translated VA.
    if (!translate_address(reg(rt), AccessType::Write, &result, true, true, false)) {
      data_abort(reg(rt), true);
      return true;
    }
    if (maybe_take_watchpoint_exception(line_base, kDataCacheLineBytes, AccessType::Write, true)) {
      return true;
    }
    return true;
  }

  // DC CVAC, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7A20u) {
    if (!el0_uci_enabled()) {
      return trap_current_system_instruction(insn);
    }
    TranslationResult result{};
    if (!translate_cache_maintenance_address(reg(insn & 0x1Fu), &result, true)) {
      data_abort(reg(insn & 0x1Fu), true);
      return true;
    }
    return true;
  }

  // DC CVAU, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7B20u) {
    if (!el0_uci_enabled()) {
      return trap_current_system_instruction(insn);
    }
    TranslationResult result{};
    if (!translate_cache_maintenance_address(reg(insn & 0x1Fu), &result, true)) {
      data_abort(reg(insn & 0x1Fu), true);
      return true;
    }
    return true;
  }

  // DC CVAP / CVADP, Xt
  // FEAT_DPB / FEAT_DPB2 are not implemented by this model and
  // ID_AA64ISAR1_EL1.DPB advertises them as absent, so direct access is
  // UNDEFINED at every exception level.
  if ((insn & 0xFFFFFFE0u) == 0xD50B7C20u ||
      (insn & 0xFFFFFFE0u) == 0xD50B7D20u) {
    return undefined_current_instruction();
  }

  // DC ZVA, Xt.
  // Model a 64-byte zeroing block, matching DCZID_EL0 BS=4.
  if ((insn & 0xFFFFFFE0u) == 0xD50B7420u) {
    if (!el0_dze_enabled()) {
      return trap_current_system_instruction(insn);
    }
    const std::uint32_t rt = insn & 0x1Fu;
    const std::uint64_t base = reg(rt) & ~(static_cast<std::uint64_t>(kDataCacheLineBytes) - 1u);
    std::array<TranslationResult, kDataCacheLineBytes / 8u> results{};
    for (std::uint64_t off = 0; off < kDataCacheLineBytes; off += 8u) {
      const std::uint64_t va = base + off;
      TranslationResult& result = results[off / 8u];
      if (!translate_address(va, AccessType::Write, &result, true)) {
        data_abort(va);
        return true;
      }
      if (result.memory_type == MemoryType::Device) {
        constexpr std::uint32_t kAlignmentFaultFsc = 0x21u;
        const std::uint32_t iss = kAlignmentFaultFsc | (1u << 6); // WnR=1, CM=0.
        enter_sync_exception(pc_ - 4, sysregs_.in_el0() ? 0x24u : 0x25u, iss, true, va);
        return true;
      }
    }
    if (maybe_take_watchpoint_exception(base, kDataCacheLineBytes, AccessType::Write)) {
      return true;
    }
    for (std::uint64_t off = 0; off < kDataCacheLineBytes; off += 8u) {
      const std::uint64_t va = base + off;
      const TranslationResult& result = results[off / 8u];
      if (!bus_.write(result.pa, 0, 8)) {
        const std::uint32_t iss = external_abort_iss(true, false);
        enter_sync_exception(pc_ - 4, sysregs_.in_el0() ? 0x24u : 0x25u, iss, true, va);
        return true;
      }
      on_code_write(va, result.pa, 8u);
      if (callbacks_.memory_write) {
        callbacks_.memory_write(*this, result.pa, 8u);
      }
    }
    clear_exclusive_monitor();
    return true;
  }

  // DC CIVAC, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7E20u) {
    if (!el0_uci_enabled()) {
      return trap_current_system_instruction(insn);
    }
    TranslationResult result{};
    if (!translate_cache_maintenance_address(reg(insn & 0x1Fu), &result, true)) {
      data_abort(reg(insn & 0x1Fu), true);
      return true;
    }
    return true;
  }

  // DC ISW, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087640u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    return true;
  }

  // DC CSW, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087A40u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    return true;
  }

  // DC CISW, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087E40u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    return true;
  }

  // AT S1E1R, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087800u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    const std::uint32_t rt = insn & 0x1Fu;
    TranslationResult result{};
    // FEAT_PAN2 is absent in this model, so plain AT S1E1R ignores PSTATE.PAN.
    const std::uint64_t va = reg(rt);
    update_par_from_translation(va, translate_address(va, AccessType::Read, &result, false, false, false), result);
    return true;
  }

  // AT S1E1W, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087820u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    const std::uint32_t rt = insn & 0x1Fu;
    TranslationResult result{};
    // FEAT_PAN2 is absent in this model, so plain AT S1E1W ignores PSTATE.PAN.
    const std::uint64_t va = reg(rt);
    update_par_from_translation(va, translate_address(va, AccessType::Write, &result, false, false, false), result);
    return true;
  }

  // AT S1E0R, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087840u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    const std::uint32_t rt = insn & 0x1Fu;
    TranslationResult result{};
    const std::uint64_t va = reg(rt);
    update_par_from_translation(va, translate_address(va, AccessType::UnprivilegedRead, &result, false, false), result);
    return true;
  }

  // AT S1E0W, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087860u) {
    if (sysregs_.in_el0()) {
      return undefined_current_instruction();
    }
    const std::uint32_t rt = insn & 0x1Fu;
    TranslationResult result{};
    const std::uint64_t va = reg(rt);
    update_par_from_translation(va, translate_address(va, AccessType::UnprivilegedWrite, &result, false, false), result);
    return true;
  }
  // MSR (immediate) to PSTATE fields.
  if ((insn & 0xFFF8E01Fu) == 0xD500401Fu) {
    const std::uint32_t op1 = (insn >> 16) & 0x7u;
    const std::uint32_t crm = (insn >> 8) & 0xFu;
    const std::uint32_t op2 = (insn >> 5) & 0x7u;
    switch ((op1 << 3) | op2) {
      case (0u << 3) | 3u:  // UAO (FEAT_UAO, absent in this model)
        return undefined_current_instruction();
      case (0u << 3) | 4u:  // PAN
        if (sysregs_.in_el0()) {
          return false;
        }
        sysregs_.set_pan((crm & 0x1u) != 0);
        return true;
      case (0u << 3) | 5u:  // SPSel
        if (sysregs_.in_el0()) {
          return false;
        }
        save_current_sp_to_bank();
        sysregs_.set_spsel(crm & 0x1u);
        load_current_sp_from_bank();
        return true;
      case (1u << 3) | 0u:  // ALLINT / PM / reserved
        return undefined_current_instruction();
      case (3u << 3) | 1u:  // SSBS (FEAT_SSBS, absent in this model)
      case (3u << 3) | 2u:  // DIT (FEAT_DIT, absent in this model)
      case (3u << 3) | 4u:  // TCO (FEAT_MTE, absent in this model)
        return undefined_current_instruction();
      case (3u << 3) | 3u:  // SVCRSM / SVCRZA / SVCRSMZA / reserved
        return undefined_current_instruction();
      case (3u << 3) | 6u:  // DAIFSet
        if (!el0_uma_enabled()) {
          return trap_current_system_instruction(insn);
        }
        sysregs_.daif_set(static_cast<std::uint8_t>(crm));
        return true;
      case (3u << 3) | 7u:  // DAIFClr
        if (!el0_uma_enabled()) {
          return trap_current_system_instruction(insn);
        }
        sysregs_.daif_clr(static_cast<std::uint8_t>(crm));
        return true;
      default:
        return undefined_current_instruction();
    }
  }

  // MRS Xt, sysreg
  if ((insn & 0xFFE00000u) == 0xD5200000u) {
    const std::uint32_t rt = insn & 0x1Fu;
    const std::uint32_t op0 = (insn >> 19) & 0x3u;
    const std::uint32_t op1 = (insn >> 16) & 0x7u;
    const std::uint32_t crn = (insn >> 12) & 0xFu;
    const std::uint32_t crm = (insn >> 8) & 0xFu;
    const std::uint32_t op2 = (insn >> 5) & 0x7u;
    const std::uint32_t key = sysreg_key(op0, op1, crn, crm, op2);
    if (!sysreg_present(key)) {
      return false;
    }
    if (el0_sysreg_undefined(key, false)) {
      return false;
    }
    if (!el0_sysreg_access_allowed(key, false)) {
      enter_sync_exception(pc_ - 4, 0x18u, sysreg_trap_iss(true, op0, op1, crn, crm, op2, rt), false, 0);
      return true;
    }
    if (software_access_debug_event_should_halt(key)) {
      halted_ = true;
      return true;
    }

    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 0u)) { // ICC_IAR1_EL1
      set_reg(rt, read_icc_iar1_el1());
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 2u)) { // ICC_HPPIR1_EL1
      set_reg(rt, read_icc_hppir1_el1());
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (11u << 3) | 3u)) { // ICC_RPR_EL1
      set_reg(rt, running_priority_ >= 0x100u ? 0xFFu : static_cast<std::uint64_t>(running_priority_ & 0xFFu));
      return true;
    }
    if (key == ((2u << 14) | (3u << 11) | (0u << 7) | (1u << 3) | 0u)) { // MDCCSR_EL0
      set_reg(rt, read_mdccsr_el0());
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (2u << 3) | 0u)) { // MDCCINT_EL1
      set_reg(rt, read_mdccint_el1());
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (2u << 3) | 2u)) { // MDSCR_EL1
      set_reg(rt, read_mdscr_el1());
      return true;
    }
    if (key == ((2u << 14) | (3u << 11) | (0u << 7) | (4u << 3) | 0u)) { // DBGDTR_EL0
      set_reg(rt, read_dbgdtr_el0());
      return true;
    }
    if (key == ((2u << 14) | (3u << 11) | (0u << 7) | (5u << 3) | 0u)) { // DBGDTRRX_EL0
      set_reg(rt, read_dbgdtrrx_el0());
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (0u << 3) | 2u)) { // OSDTRRX_EL1
      set_reg(rt, read_osdtrrx_el1());
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (3u << 3) | 2u)) { // OSDTRTX_EL1
      set_reg(rt, read_osdtrtx_el1());
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (6u << 3) | 0u)) { // ICC_PMR_EL1
      set_reg(rt, icc_pmr_el1_);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 4u)) { // ICC_CTLR_EL1
      set_reg(rt, icc_ctlr_el1_);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 5u)) { // ICC_SRE_EL1
      set_reg(rt, icc_sre_el1_);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 3u)) { // ICC_BPR1_EL1
      set_reg(rt, icc_bpr1_el1_);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 0u)) { // ICC_AP1R0_EL1
      set_reg(rt, icc_ap1r_el1_[0]);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 1u)) { // ICC_AP1R1_EL1
      set_reg(rt, icc_ap1r_el1_[1]);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 2u)) { // ICC_AP1R2_EL1
      set_reg(rt, icc_ap1r_el1_[2]);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 3u)) { // ICC_AP1R3_EL1
      set_reg(rt, icc_ap1r_el1_[3]);
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (0u << 3) | 1u) ||
        key == ((3u << 14) | (3u << 11) | (14u << 7) | (0u << 3) | 2u)) { // CNTVCT_EL0 / CNTPCT_EL0 share the same global counter in this model
      set_reg(rt, timer_.counter_at_steps(shared_timer_steps()));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 1u)) { // CNTV_CTL_EL0
      set_reg(rt, timer_.read_cntv_ctl_el0(cpu_index_, shared_timer_steps()));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 2u)) { // CNTV_CVAL_EL0
      set_reg(rt, timer_.read_cntv_cval_el0(cpu_index_));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 0u)) { // CNTV_TVAL_EL0
      set_reg(rt, timer_.read_cntv_tval_el0(cpu_index_, shared_timer_steps()));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 1u)) { // CNTP_CTL_EL0
      set_reg(rt, timer_.read_cntp_ctl_el0(cpu_index_, shared_timer_steps()));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 2u)) { // CNTP_CVAL_EL0
      set_reg(rt, timer_.read_cntp_cval_el0(cpu_index_));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 0u)) { // CNTP_TVAL_EL0
      set_reg(rt, timer_.read_cntp_tval_el0(cpu_index_, shared_timer_steps()));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (0u << 7) | (0u << 3) | 7u)) { // DCZID_EL0
      std::uint64_t value = 0;
      if (!sysregs_.read(op0, op1, crn, crm, op2, value)) {
        return false;
      }
      // When SCTLR_EL1.DZE is clear, EL0 still reads DCZID_EL0 but observes
      // DZP=1 to indicate that DC ZVA is not supported from EL0.
      if (sysregs_.in_el0() && (sysregs_.sctlr_el1() & kSctlrEl1Dze) == 0u) {
        value |= 1ull << 4;
      }
      set_reg(rt, value);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL0
      if (sysregs_.current_uses_sp_el0()) {
        return false;
      }
      set_reg(rt, sysregs_.sp_el0());
      return true;
    }
    if (key == ((3u << 14) | (4u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL1
      return false;
    }

    std::uint64_t value = 0;
    if (!sysregs_.read(op0, op1, crn, crm, op2, value)) {
      return false;
    }
    set_reg(rt, value);
    return true;
  }

  // MSR sysreg, Xt
  if ((insn & 0xFFE00000u) == 0xD5000000u) {
    const std::uint32_t rt = insn & 0x1Fu;
    const std::uint32_t op0 = (insn >> 19) & 0x3u;
    const std::uint32_t op1 = (insn >> 16) & 0x7u;
    const std::uint32_t crn = (insn >> 12) & 0xFu;
    const std::uint32_t crm = (insn >> 8) & 0xFu;
    const std::uint32_t op2 = (insn >> 5) & 0x7u;
    const std::uint32_t key = sysreg_key(op0, op1, crn, crm, op2);
    if (!sysreg_present(key)) {
      return false;
    }
    if (el0_sysreg_undefined(key, true)) {
      return false;
    }
    if (!el0_sysreg_access_allowed(key, true)) {
      enter_sync_exception(pc_ - 4, 0x18u, sysreg_trap_iss(false, op0, op1, crn, crm, op2, rt), false, 0);
      return true;
    }
    if (software_access_debug_event_should_halt(key)) {
      halted_ = true;
      return true;
    }

    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 1u)) { // ICC_EOIR1_EL1
      icc_priority_drop(static_cast<std::uint32_t>(reg(rt)));
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (2u << 3) | 0u)) { // SPSel
      save_current_sp_to_bank();
      sysregs_.set_spsel(reg(rt));
      load_current_sp_from_bank();
      return true;
    }
    if (key == ((2u << 14) | (3u << 11) | (0u << 7) | (4u << 3) | 0u)) { // DBGDTR_EL0
      write_dbgdtr_el0(reg(rt));
      return true;
    }
    if (key == ((2u << 14) | (3u << 11) | (0u << 7) | (5u << 3) | 0u)) { // DBGDTRTX_EL0
      write_dbgdtrtx_el0(static_cast<std::uint32_t>(reg(rt)));
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (2u << 3) | 0u)) { // MDCCINT_EL1
      write_mdccint_el1(reg(rt));
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (2u << 3) | 2u)) { // MDSCR_EL1
      write_mdscr_el1(reg(rt));
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (0u << 3) | 2u)) { // OSDTRRX_EL1
      write_osdtrrx_el1(reg(rt));
      return true;
    }
    if (key == ((2u << 14) | (0u << 11) | (0u << 7) | (3u << 3) | 2u)) { // OSDTRTX_EL1
      write_osdtrtx_el1(reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (6u << 3) | 0u)) { // ICC_PMR_EL1
      icc_pmr_el1_ = reg(rt);
      refresh_irq_threshold_cache();
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 4u)) { // ICC_CTLR_EL1
      icc_ctlr_el1_ = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 5u)) { // ICC_SRE_EL1
      icc_sre_el1_ = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 3u)) { // ICC_BPR1_EL1
      icc_bpr1_el1_ = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (8u << 3) | 4u)) { // ICC_AP0R0_EL1
      icc_ap0r_el1_[0] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (8u << 3) | 5u)) { // ICC_AP0R1_EL1
      icc_ap0r_el1_[1] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (8u << 3) | 6u)) { // ICC_AP0R2_EL1
      icc_ap0r_el1_[2] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (8u << 3) | 7u)) { // ICC_AP0R3_EL1
      icc_ap0r_el1_[3] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 0u)) { // ICC_AP1R0_EL1
      icc_ap1r_el1_[0] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 1u)) { // ICC_AP1R1_EL1
      icc_ap1r_el1_[1] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 2u)) { // ICC_AP1R2_EL1
      icc_ap1r_el1_[2] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (9u << 3) | 3u)) { // ICC_AP1R3_EL1
      icc_ap1r_el1_[3] = reg(rt);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 7u)) { // ICC_IGRPEN1_EL1
      icc_igrpen1_el1_ = reg(rt) & 1u;
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (11u << 3) | 5u)) { // ICC_SGI1R_EL1
      gic_.send_sgi(cpu_index_, reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (11u << 3) | 1u)) { // ICC_DIR_EL1
      icc_deactivate(static_cast<std::uint32_t>(reg(rt)));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 1u)) { // CNTV_CTL_EL0
      timer_.write_cntv_ctl_el0(cpu_index_, shared_timer_steps(), reg(rt));
      refresh_local_timer_irq_lines();
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 2u)) { // CNTV_CVAL_EL0
      timer_.write_cntv_cval_el0(cpu_index_, shared_timer_steps(), reg(rt));
      refresh_local_timer_irq_lines();
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 0u)) { // CNTV_TVAL_EL0
      timer_.write_cntv_tval_el0(cpu_index_, shared_timer_steps(), reg(rt));
      refresh_local_timer_irq_lines();
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 1u)) { // CNTP_CTL_EL0
      timer_.write_cntp_ctl_el0(cpu_index_, shared_timer_steps(), reg(rt));
      refresh_local_timer_irq_lines();
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 2u)) { // CNTP_CVAL_EL0
      timer_.write_cntp_cval_el0(cpu_index_, shared_timer_steps(), reg(rt));
      refresh_local_timer_irq_lines();
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 0u)) { // CNTP_TVAL_EL0
      timer_.write_cntp_tval_el0(cpu_index_, shared_timer_steps(), reg(rt));
      refresh_local_timer_irq_lines();
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL0
      if (sysregs_.current_uses_sp_el0()) {
        return false;
      }
      sysregs_.set_sp_el0(reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (4u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL1
      return false;
    }

    const bool ok = sysregs_.write(op0, op1, crn, crm, op2, reg(rt));
    if (!ok) {
      return false;
    }

    // Translation regime changes invalidate decode state. Keep ASID-tagged TLB
    // entries across TTBR writes so software can switch address spaces by ASID.
    if (key == ((3u << 14) | (0u << 11) | (2u << 7) | (0u << 3) | 0u) || // TTBR0_EL1
        key == ((3u << 14) | (0u << 11) | (2u << 7) | (0u << 3) | 1u)) { // TTBR1_EL1
      invalidate_decode_all();
    }
    if (key == ((3u << 14) | (0u << 11) | (1u << 7) | (0u << 3) | 0u) || // SCTLR_EL1
        key == ((3u << 14) | (0u << 11) | (2u << 7) | (0u << 3) | 2u)) { // TCR_EL1
      tlb_flush_all();
      invalidate_decode_all();
    }
    return true;
  }

  return false;
}

bool Cpu::exec_data_processing(std::uint32_t insn) {
  const std::uint32_t rd = insn & 0x1Fu;
  const std::uint32_t rn = (insn >> 5) & 0x1Fu;

  // FEAT_FlagM is absent in the current model. RMIF / SETF8 / SETF16 live in
  // generic integer decode space, so intercept them explicitly before wider
  // arithmetic/select families can mis-handle them as unrelated instructions.
  if ((insn & 0xFFE07C10u) == 0xBA000400u ||   // RMIF Xn, #imm6, #mask
      (insn & 0xFFFFBC1Fu) == 0x3A00080Du) {   // SETF8/SETF16 Wn
    enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
    return true;
  }

  // The current model declares FEAT_PAuth absent in ID_AA64ISAR*.
  // Several direct integer PAuth encodings overlap with generic integer
  // decode space (for example the 1-source RBIT/REV/CLZ/CLS masks). Treat
  // these allocated encodings as UNDEFINED explicitly so they do not alias to
  // unrelated integer instructions.
  if ((insn & 0xFFFFFFE0u) == 0xDAC143E0u ||   // XPACI Xd
      (insn & 0xFFFFFFE0u) == 0xDAC147E0u ||   // XPACD Xd
      (insn & 0xFFFFFC00u) == 0xDAC10000u ||   // PACIA Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC123E0u ||   // PACIZA Xd
      (insn & 0xFFFFFC00u) == 0xDAC10400u ||   // PACIB Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC127E0u ||   // PACIZB Xd
      (insn & 0xFFFFFC00u) == 0xDAC10800u ||   // PACDA Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC12BE0u ||   // PACDZA Xd
      (insn & 0xFFFFFC00u) == 0xDAC10C00u ||   // PACDB Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC12FE0u ||   // PACDZB Xd
      (insn & 0xFFFFFC00u) == 0xDAC11000u ||   // AUTIA Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC133E0u ||   // AUTIZA Xd
      (insn & 0xFFFFFC00u) == 0xDAC11400u ||   // AUTIB Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC137E0u ||   // AUTIZB Xd
      (insn & 0xFFFFFC00u) == 0xDAC11800u ||   // AUTDA Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC13BE0u ||   // AUTDZA Xd
      (insn & 0xFFFFFC00u) == 0xDAC11C00u ||   // AUTDB Xd, Xn|SP
      (insn & 0xFFFFFFE0u) == 0xDAC13FE0u ||   // AUTDZB Xd
      (insn & 0xFFE0FC00u) == 0x9AC03000u) {   // PACGA Xd, Xn, Xm|SP
    enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
    return true;
  }

  if (feat_fp16_absent(sysregs_.id_aa64pfr0_el1()) &&
      insn_requires_fp16_feature(insn)) {
    enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
    return true;
  }

  const auto read_fp32 = [&](std::uint32_t idx) -> float {
    return std::bit_cast<float>(static_cast<std::uint32_t>(qregs_[idx][0] & 0xFFFFFFFFu));
  };
  const auto read_fp64 = [&](std::uint32_t idx) -> double {
    return std::bit_cast<double>(qregs_[idx][0]);
  };
  const auto write_fp32 = [&](std::uint32_t idx, float value) {
    qregs_[idx][0] = static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value));
    qregs_[idx][1] = 0u;
  };
  const auto write_fp64 = [&](std::uint32_t idx, double value) {
    qregs_[idx][0] = std::bit_cast<std::uint64_t>(value);
    qregs_[idx][1] = 0u;
  };
  const auto write_vec_scalar = [&](std::uint32_t idx, std::uint64_t value, std::uint32_t esize_bits) {
    qregs_[idx][0] = value & ones(esize_bits);
    qregs_[idx][1] = 0u;
  };
  const auto write_fp_compare_result = [&](std::uint32_t idx, bool predicate, std::uint32_t esize_bits) {
    qregs_[idx][0] = predicate ? ones(esize_bits) : 0u;
    qregs_[idx][1] = 0u;
  };
  const auto set_fp_compare_flags = [&](bool unordered, bool less, bool equal) {
    auto p = sysregs_.pstate();
    if (unordered) {
      p.n = false;
      p.z = false;
      p.c = true;
      p.v = true;
    } else if (less) {
      p.n = true;
      p.z = false;
      p.c = false;
      p.v = false;
    } else if (equal) {
      p.n = false;
      p.z = true;
      p.c = true;
      p.v = false;
    } else {
      p.n = false;
      p.z = false;
      p.c = true;
      p.v = false;
    }
    sysregs_.set_pstate(p);
  };
  const auto vector_fp_shape = [&](std::uint32_t insn, std::uint32_t& esize_bits, std::uint32_t& lanes) -> bool {
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool sz = ((insn >> 22) & 1u) != 0u;
    esize_bits = sz ? 64u : 32u;
    if (esize_bits == 64u && !q) {
      return false;
    }
    lanes = (q ? 128u : 64u) / esize_bits;
    return true;
  };
  const auto vector_fp_compare_mask = [](bool predicate, std::uint32_t esize_bits) -> std::uint64_t {
    return predicate ? ones(esize_bits) : 0u;
  };
  const auto fp_round_ties_even = [](auto value) {
    using FloatT = decltype(value);
    if (!std::isfinite(value) || value == static_cast<FloatT>(0.0)) {
      return value;
    }
    const FloatT abs_value = std::fabs(value);
    const FloatT floor_value = std::floor(abs_value);
    const FloatT frac = abs_value - floor_value;
    FloatT rounded = floor_value;
    if (frac > static_cast<FloatT>(0.5) ||
        (frac == static_cast<FloatT>(0.5) && std::fmod(floor_value, static_cast<FloatT>(2.0)) != static_cast<FloatT>(0.0))) {
      rounded += static_cast<FloatT>(1.0);
    }
    return std::copysign(rounded, value);
  };
  const auto fp_round_ties_away = [](auto value) {
    using FloatT = decltype(value);
    if (!std::isfinite(value) || value == static_cast<FloatT>(0.0)) {
      return value;
    }
    const FloatT abs_value = std::fabs(value);
    const FloatT floor_value = std::floor(abs_value);
    const FloatT frac = abs_value - floor_value;
    FloatT rounded = floor_value;
    if (frac >= static_cast<FloatT>(0.5)) {
      rounded += static_cast<FloatT>(1.0);
    }
    return std::copysign(rounded, value);
  };
  const auto fp_round_by_mode = [&](auto value, std::uint32_t mode) {
    switch (mode & 0x3u) {
      case 0u: return fp_round_ties_even(value);
      case 1u: return std::ceil(value);
      case 2u: return std::floor(value);
      case 3u: return std::trunc(value);
    }
    return value;
  };
  const auto current_fpcr_ctl = [&]() -> std::uint32_t {
    const std::uint64_t fpcr = sysregs_.fpcr();
    return static_cast<std::uint32_t>(((fpcr >> 22) & 0x3u) |
                                      (((fpcr >> 25) & 0x1u) << 2) |
                                      (((fpcr >> 24) & 0x1u) << 3) |
                                      (((fpcr >> 26) & 0x1u) << 4));
  };
  const auto fp_round_int_bits = [&](auto bits, auto round_fn, bool exact) {
    using UIntT = decltype(bits);
    using FloatT = std::conditional_t<sizeof(UIntT) == 4u, float, double>;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint64_t fpsr_bits = 0u;
    if (const auto nan = fp_process_nan_unary(bits, fpcr_mode, true, fpsr_bits); nan.has_value()) {
      return std::pair<UIntT, std::uint64_t>{*nan, fpsr_bits};
    }
    bits = fp_flush_input_denormal_bits(bits, fpcr_mode, fpsr_bits);
    if (fp_is_inf_bits(bits) || (bits & ~fp_sign_mask_bits<UIntT>()) == 0u) {
      return std::pair<UIntT, std::uint64_t>{bits, fpsr_bits};
    }
    const FloatT value = std::bit_cast<FloatT>(bits);
    FloatT rounded = round_fn(value);
    if (rounded == static_cast<FloatT>(0.0)) {
      rounded = std::copysign(static_cast<FloatT>(0.0), value);
    }
    if (exact && rounded != value) {
      fpsr_bits |= kFpsrIxc;
    }
    return std::pair<UIntT, std::uint64_t>{std::bit_cast<UIntT>(rounded), fpsr_bits};
  };

  // Minimal AdvSIMD/FP support for libc/busybox string routines.
  if ((insn & 0xBF20FC00u) == 0x0E003C00u) { // UMOV/MOV (lane to general)
    const std::uint32_t imm5 = (insn >> 16) & 0x1Fu;
    if (imm5 == 0u) {
      return false;
    }
    const std::uint32_t size_shift = static_cast<std::uint32_t>(__builtin_ctz(imm5));
    const std::uint32_t esize_bits = 8u << size_shift;
    if (esize_bits > 64u) {
      return false;
    }
    const std::uint32_t lane = imm5 >> (size_shift + 1u);
    const std::uint32_t lanes = 128u / esize_bits;
    if (lane >= lanes) {
      return false;
    }
    const std::uint64_t elem = vector_get_elem(qregs_[rn], esize_bits, lane);
    if (esize_bits < 64u) {
      set_reg32(rd, static_cast<std::uint32_t>(elem));
    } else {
      set_reg(rd, elem);
    }
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E000400u) { // DUP (element)
    // Half-precision FMAXNM/FMINNM shares this coarse decode space. Without
    // FEAT_FP16 it must stay UNDEFINED instead of being mis-decoded as DUP,
    // and future FP16-enabled models must not silently execute the wrong
    // integer AdvSIMD operation either.
    if ((insn & 0x9F20FC00u) == 0x0E000400u && ((insn >> 22) & 1u) != 0u) {
      const std::uint32_t fp = static_cast<std::uint32_t>((sysregs_.id_aa64pfr0_el1() >> 16) & 0xFu);
      if (fp == 0u) {
        enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
        return true;
      }
      return false;
    }
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t imm5 = (insn >> 16) & 0x1Fu;
    const std::uint32_t imm4 = (insn >> 11) & 0xFu;
    if (imm5 == 0u || imm4 != 0u) {
      return false;
    }
    const std::uint32_t size_shift = static_cast<std::uint32_t>(__builtin_ctz(imm5));
    const std::uint32_t esize_bits = 8u << size_shift;
    if (esize_bits > 64u || esize_bits > (q ? 128u : 64u)) {
      return false;
    }
    const std::uint32_t lane = imm5 >> (size_shift + 1u);
    const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
    if (lane >= lanes) {
      return false;
    }
    const std::uint64_t elem = vector_get_elem(qregs_[rn], esize_bits, lane);
    auto& dst = qregs_[rd];
    dst = {0, 0};
    for (std::uint32_t i = 0; i < lanes; ++i) {
      vector_set_elem(dst, esize_bits, i, elem);
    }
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E000C00u) { // DUP (general)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t imm5 = (insn >> 16) & 0x1Fu;
    if (imm5 == 0u || (imm5 & (imm5 - 1u)) != 0u) {
      return false;
    }
    const std::uint32_t esize_bits = 8u << __builtin_ctz(imm5);
    if (esize_bits > 64u) {
      return false;
    }
    const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
    const std::uint32_t rm = (insn >> 5) & 0x1Fu;
    const std::uint64_t elem = (esize_bits == 64u) ? reg(rm) : (static_cast<std::uint64_t>(reg32(rm)) & ones(esize_bits));
    auto& dst = qregs_[rd];
    dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      vector_set_elem(dst, esize_bits, lane, elem);
    }
    return true;
  }

  if ((insn & 0xFFE08400u) == 0x6E000400u) { // INS/MOV (element)
    // Half-precision FMULX shares this coarse decode space. Without
    // FEAT_FP16 it must stay UNDEFINED instead of being mis-decoded as
    // INS/MOV, and future FP16-enabled models must not silently execute the
    // wrong integer permutation either.
    if ((insn & 0xBF20FC00u) == 0x0E001C00u && ((insn >> 22) & 1u) != 0u) {
      const std::uint32_t fp = static_cast<std::uint32_t>((sysregs_.id_aa64pfr0_el1() >> 16) & 0xFu);
      if (fp == 0u) {
        enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
        return true;
      }
      return false;
    }
    const std::uint32_t imm5 = (insn >> 16) & 0x1Fu;
    const std::uint32_t imm4 = (insn >> 11) & 0xFu;
    if (imm5 == 0u) {
      return false;
    }
    const std::uint32_t size_shift = static_cast<std::uint32_t>(__builtin_ctz(imm5));
    const std::uint32_t esize_bits = 8u << size_shift;
    if (esize_bits > 64u) {
      return false;
    }
    const std::uint32_t lane_mask = (1u << size_shift) - 1u;
    if ((imm4 & lane_mask) != 0u) {
      return false;
    }
    const std::uint32_t lanes = 128u / esize_bits;
    const std::uint32_t dst_lane = imm5 >> (size_shift + 1u);
    const std::uint32_t src_lane = imm4 >> size_shift;
    if (dst_lane >= lanes || src_lane >= lanes) {
      return false;
    }
    const std::uint64_t elem = vector_get_elem(qregs_[rn], esize_bits, src_lane);
    vector_set_elem(qregs_[rd], esize_bits, dst_lane, elem);
    return true;
  }

  if ((insn & 0xBF80FC00u) == 0x2F00E400u) { // MOVI (scalar D immediate)
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    qregs_[rd][0] = scalar_byte_mask_imm(imm8);
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFE08400u) == 0x4E000400u) { // INS (general)
    const std::uint32_t imm5 = (insn >> 16) & 0x1Fu;
    const std::uint32_t imm4 = (insn >> 11) & 0xFu;
    if (imm5 == 0u || imm4 != 0x3u) {
      return false;
    }
    const std::uint32_t size_shift = static_cast<std::uint32_t>(__builtin_ctz(imm5));
    const std::uint32_t esize_bits = 8u << size_shift;
    if (esize_bits > 64u) {
      return false;
    }
    const std::uint32_t lane = imm5 >> (size_shift + 1u);
    const std::uint32_t lanes = 128u / esize_bits;
    if (lane >= lanes) {
      return false;
    }
    const std::uint64_t elem = (esize_bits == 64u) ? reg(rn) : (static_cast<std::uint64_t>(reg32(rn)) & ones(esize_bits));
    vector_set_elem(qregs_[rd], esize_bits, lane, elem);
    return true;
  }

  if ((insn & 0x9F80FC00u) == 0x0F00A400u) { // SSHLL/USHLL/SSHLL2/USHLL2, including SXTL/UXTL aliases
    const std::uint32_t immh = (insn >> 19) & 0xFu;
    if (immh != 0u) {
      const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
      const bool upper_half = ((insn >> 30) & 1u) != 0u;
      const std::uint32_t immb = (insn >> 16) & 0x7u;
      const std::uint32_t src_esize_bits = 8u << highest_set_bit(immh);
      if (src_esize_bits < 8u || src_esize_bits > 32u) {
        return false;
      }
      const std::uint32_t immhb = (immh << 3u) | immb;
      if (immhb < src_esize_bits || immhb >= (src_esize_bits * 2u)) {
        return false;
      }
      const std::uint32_t shift = immhb - src_esize_bits;
      const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
      const std::uint32_t lanes = 64u / src_esize_bits;
      const std::uint32_t src_base = upper_half ? lanes : 0u;
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0, 0};
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        const std::uint64_t value = vector_get_elem(src, src_esize_bits, lane + src_base);
        std::uint64_t widened = 0;
        if (is_unsigned) {
          widened = (value & ones(src_esize_bits)) << shift;
        } else {
          widened = static_cast<std::uint64_t>(sign_extend(value, src_esize_bits) << shift);
        }
        vector_set_elem(dst, dst_esize_bits, lane, widened);
      }
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xBF80FC00u) == 0x2F000400u || (insn & 0xBF80FC00u) == 0x0F000400u) { // USHR/SSHR
    const std::uint32_t immh = (insn >> 19) & 0xFu;
    if (immh != 0u) {
      const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
      const bool q = ((insn >> 30) & 1u) != 0u;
      const std::uint32_t immb = (insn >> 16) & 0x7u;
      const std::uint32_t esize_bits = 8u << highest_set_bit(immh);
      const std::uint32_t total_bits = q ? 128u : 64u;
      if (esize_bits > total_bits) {
        return false;
      }
      const std::uint32_t immhb = (immh << 3u) | immb;
      const std::uint32_t shift = esize_bits * 2u - immhb;
      if (shift == 0u || shift > esize_bits) {
        return false;
      }
      const std::uint32_t lanes = total_bits / esize_bits;
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0, 0};
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        const std::uint64_t value = vector_get_elem(src, esize_bits, lane);
        const std::uint64_t shifted = is_unsigned ? (value >> shift) : static_cast<std::uint64_t>(sign_extend(value, esize_bits) >> shift);
        vector_set_elem(dst, esize_bits, lane, shifted & ones(esize_bits));
      }
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xBF80FC00u) == 0x0F00E400u) { // MOVI (byte immediate)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool op = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t cmode = (insn >> 12) & 0xFu;
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    const std::uint64_t imm64 = advsimd_expand_imm(op, cmode, imm8);
    auto& dst = qregs_[rd];
    dst[0] = imm64;
    dst[1] = q ? imm64 : 0u;
    return true;
  }

  if ((insn & 0x9FF80400u) == 0x0F000400u) { // MOVI/ORR/MVNI/BIC (vector, modified immediate)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool op = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t cmode = (insn >> 12) & 0xFu;
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    const std::uint64_t imm64 = advsimd_expand_imm(false, cmode, imm8);
    auto& dst = qregs_[rd];
    const auto old = dst;
    if (cmode <= 11u) {
      if (!op) {
        if ((cmode & 1u) == 0u) {
          dst[0] = imm64;
          dst[1] = q ? imm64 : 0u;
        } else {
          dst[0] = old[0] | imm64;
          dst[1] = q ? (old[1] | imm64) : 0u;
        }
      } else {
        if ((cmode & 1u) == 0u) {
          dst[0] = ~imm64;
          dst[1] = q ? ~imm64 : 0u;
        } else {
          dst[0] = old[0] & ~imm64;
          dst[1] = q ? (old[1] & ~imm64) : 0u;
        }
      }
      return true;
    }
    if (cmode == 12u || cmode == 13u) {
      if (!op) {
        dst[0] = imm64;
        dst[1] = q ? imm64 : 0u;
      } else {
        dst[0] = ~imm64;
        dst[1] = q ? ~imm64 : 0u;
      }
      return true;
    }
  }

  if ((insn & 0xBF20FC00u) == 0x0E201800u) { // REV16 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (((insn >> 16) & 0x1Fu) == 0u && size == 0u) {
      const std::uint32_t total_bits = q ? 128u : 64u;
      const std::uint32_t esize_bits = 8u;
      const std::uint32_t container_bits = 16u;
      const std::uint32_t containers = total_bits / container_bits;
      const std::uint32_t elems_per_container = container_bits / esize_bits;
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0, 0};
      for (std::uint32_t c = 0; c < containers; ++c) {
        for (std::uint32_t i = 0; i < elems_per_container; ++i) {
          const std::uint32_t src_lane = c * elems_per_container + (elems_per_container - 1u - i);
          const std::uint32_t dst_lane = c * elems_per_container + i;
          vector_set_elem(dst, esize_bits, dst_lane, vector_get_elem(src, esize_bits, src_lane));
        }
      }
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xBF20FC00u) == 0x0E200800u || (insn & 0xBF20FC00u) == 0x2E200800u) { // REV32/REV64 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool rev32 = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t esize_bits = 8u << size;
    if (((insn >> 16) & 0x1Fu) == 0u &&
        !((rev32 && size > 1u) || (!rev32 && size > 2u) || esize_bits >= (rev32 ? 32u : 64u))) {
      const std::uint32_t total_bits = q ? 128u : 64u;
      const std::uint32_t container_bits = rev32 ? 32u : 64u;
      const std::uint32_t containers = total_bits / container_bits;
      const std::uint32_t elems_per_container = container_bits / esize_bits;
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0, 0};
      for (std::uint32_t c = 0; c < containers; ++c) {
        for (std::uint32_t i = 0; i < elems_per_container; ++i) {
          const std::uint32_t src_lane = c * elems_per_container + (elems_per_container - 1u - i);
          const std::uint32_t dst_lane = c * elems_per_container + i;
          vector_set_elem(dst, esize_bits, dst_lane, vector_get_elem(src, esize_bits, src_lane));
        }
      }
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xBF21FC00u) == 0x0E204800u || (insn & 0xBF21FC00u) == 0x2E204800u) { // CLS/CLZ (vector)
    const bool is_clz = ((insn >> 29) & 1u) != 0u;
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (((insn >> 16) & 0x1Fu) == 0u && size <= 2u) {
      const std::uint32_t esize_bits = 8u << size;
      const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0u, 0u};
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        const std::uint64_t value = vector_get_elem(src, esize_bits, lane);
        vector_set_elem(dst, esize_bits, lane, is_clz ? clz_bits(value, esize_bits) : cls_bits(value, esize_bits));
      }
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xBF3FFC00u) == 0x0E208800u || // CMGT (zero)
      (insn & 0xBF3FFC00u) == 0x2E208800u || // CMGE (zero)
      (insn & 0xBF3FFC00u) == 0x0E209800u || // CMEQ (zero)
      (insn & 0xBF3FFC00u) == 0x2E209800u || // CMLE (zero)
      (insn & 0xBF3FFC00u) == 0x0E20A800u) { // CMLT (zero)
    const std::uint32_t tag = insn & 0xBF3FFC00u;
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t esize_bits = 8u << size;
    if (esize_bits == 0u || (!q && esize_bits == 64u)) {
      return false;
    }
    const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t elem = vector_get_elem(src, esize_bits, lane);
      const std::int64_t signed_elem = sign_extend(elem, esize_bits);
      bool predicate = false;
      switch (tag) {
        case 0x0E208800u:
          predicate = signed_elem > 0;
          break;
        case 0x2E208800u:
          predicate = signed_elem >= 0;
          break;
        case 0x0E209800u:
          predicate = elem == 0u;
          break;
        case 0x2E209800u:
          predicate = signed_elem <= 0;
          break;
        case 0x0E20A800u:
          predicate = signed_elem < 0;
          break;
        default:
          return false;
      }
      vector_set_elem(dst, esize_bits, lane, predicate ? ones(esize_bits) : 0u);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x2E208C00u) { // CMEQ (register)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t esize_bits = 8u << size;
    if (esize_bits == 0u || (!q && esize_bits == 64u)) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const bool equal = vector_get_elem(lhs, esize_bits, lane) == vector_get_elem(rhs, esize_bits, lane);
      vector_set_elem(dst, esize_bits, lane, equal ? ones(esize_bits) : 0u);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E208C00u) { // CMTST (register)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t esize_bits = 8u << size;
    if (esize_bits == 0u || (!q && esize_bits == 64u)) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t value = vector_get_elem(lhs, esize_bits, lane) & vector_get_elem(rhs, esize_bits, lane);
      vector_set_elem(dst, esize_bits, lane, value != 0u ? ones(esize_bits) : 0u);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBFE08C00u) == 0x0E000000u) { // TBL/TBX
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t len = (insn >> 13) & 0x3u;
    const bool is_tbx = ((insn >> 12) & 1u) != 0u;
    const std::uint32_t regs = len + 1u;
    const std::uint32_t datasize = q ? 128u : 64u;
    const std::uint32_t elements = datasize / 8u;
    std::array<std::uint8_t, 64> table{};
    for (std::uint32_t reg_idx = 0; reg_idx < regs; ++reg_idx) {
      const auto& table_reg = qregs_[(rn + reg_idx) & 31u];
      for (std::uint32_t byte = 0; byte < 16u; ++byte) {
        table[reg_idx * 16u + byte] = static_cast<std::uint8_t>(vector_get_elem(table_reg, 8u, byte));
      }
    }
    std::array<std::uint64_t, 2> dst =
        q ? (is_tbx ? qregs_[rd] : std::array<std::uint64_t, 2>{0u, 0u})
          : std::array<std::uint64_t, 2>{is_tbx ? qregs_[rd][0] : 0u, 0u};
    for (std::uint32_t lane = 0; lane < elements; ++lane) {
      const std::uint32_t index = static_cast<std::uint32_t>(vector_get_elem(qregs_[rm], 8u, lane) & 0xFFu);
      if (index < regs * 16u) {
        vector_set_elem(dst, 8u, lane, table[index]);
      }
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBFE08400u) == 0x2E000000u) { // EXT (vector extract)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t total_bytes = q ? 16u : 8u;
    const std::uint32_t imm4 = (insn >> 11) & 0xFu;
    if (imm4 >= total_bytes) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    std::array<std::uint8_t, 32> bytes{};
    for (std::uint32_t i = 0; i < total_bytes; ++i) {
      bytes[i] = static_cast<std::uint8_t>(vector_get_elem(qregs_[rn], 8u, i));
      bytes[total_bytes + i] = static_cast<std::uint8_t>(vector_get_elem(qregs_[rm], 8u, i));
    }
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < total_bytes; ++i) {
      vector_set_elem(dst, 8u, i, bytes[imm4 + i]);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF3FFC00u) == 0x0E205800u) { // CNT (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size != 0u) {
      return false;
    }
    const std::uint32_t lanes = q ? 16u : 8u;
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const auto value = static_cast<std::uint8_t>(vector_get_elem(src, 8u, lane));
      vector_set_elem(dst, 8u, lane, static_cast<std::uint64_t>(std::popcount(value)));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0E201C00u) { // AND (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    qregs_[rd][0] = lhs[0] & rhs[0];
    qregs_[rd][1] = q ? (lhs[1] & rhs[1]) : 0u;
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0E601C00u) { // BIC (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    qregs_[rd][0] = lhs[0] & ~rhs[0];
    qregs_[rd][1] = q ? (lhs[1] & ~rhs[1]) : 0u;
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0EA01C00u) { // ORR/MOV (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    qregs_[rd][0] = lhs[0] | rhs[0];
    qregs_[rd][1] = q ? (lhs[1] | rhs[1]) : 0u;
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x2E201C00u) { // EOR (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    qregs_[rd][0] = lhs[0] ^ rhs[0];
    qregs_[rd][1] = q ? (lhs[1] ^ rhs[1]) : 0u;
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x2EA01C00u) { // BIT (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const auto dst_old = qregs_[rd];
    const auto src = qregs_[rn];
    const auto mask = qregs_[rm];
    qregs_[rd][0] = (dst_old[0] & ~mask[0]) | (src[0] & mask[0]);
    qregs_[rd][1] = q ? ((dst_old[1] & ~mask[1]) | (src[1] & mask[1])) : 0u;
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x2EE01C00u) { // BIF (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const auto dst_old = qregs_[rd];
    const auto src = qregs_[rn];
    const auto mask = qregs_[rm];
    qregs_[rd][0] = (dst_old[0] & mask[0]) | (src[0] & ~mask[0]);
    qregs_[rd][1] = q ? ((dst_old[1] & mask[1]) | (src[1] & ~mask[1])) : 0u;
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x2E601C00u) { // BSL (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const auto mask = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    qregs_[rd][0] = (mask[0] & lhs[0]) | (~mask[0] & rhs[0]);
    qregs_[rd][1] = q ? ((mask[1] & lhs[1]) | (~mask[1] & rhs[1])) : 0u;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x2E203400u || // CMHI (register)
      (insn & 0xBF20FC00u) == 0x0E203400u || // CMGT (register)
      (insn & 0xBF20FC00u) == 0x2E203C00u || // CMHS (register)
      (insn & 0xBF20FC00u) == 0x0E203C00u) { // CMGE (register)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t esize_bits = 8u << size;
    if (esize_bits == 0u || (!q && esize_bits == 64u)) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    const std::uint32_t tag = insn & 0xBF20FC00u;
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t lhs_elem = vector_get_elem(lhs, esize_bits, lane);
      const std::uint64_t rhs_elem = vector_get_elem(rhs, esize_bits, lane);
      bool predicate = false;
      switch (tag) {
        case 0x2E203400u: // CMHI
          predicate = lhs_elem > rhs_elem;
          break;
        case 0x0E203400u: // CMGT
          predicate = sign_extend(lhs_elem, esize_bits) > sign_extend(rhs_elem, esize_bits);
          break;
        case 0x2E203C00u: // CMHS
          predicate = lhs_elem >= rhs_elem;
          break;
        case 0x0E203C00u: // CMGE
          predicate = sign_extend(lhs_elem, esize_bits) >= sign_extend(rhs_elem, esize_bits);
          break;
        default:
          return false;
      }
      vector_set_elem(dst, esize_bits, lane, predicate ? ones(esize_bits) : 0u);
    }
    qregs_[rd] = dst;
    return true;
  }

  switch (insn & 0xFFE0FC00u) {
    case 0x2E20D400u: // FADDP (vector, 2S)
    case 0x6E20D400u: // FADDP (vector, 4S)
    case 0x6E60D400u: // FADDP (vector, 2D)
    case 0x2E20F400u: // FMAXP (vector, 2S)
    case 0x6E20F400u: // FMAXP (vector, 4S)
    case 0x6E60F400u: // FMAXP (vector, 2D)
    case 0x2EA0F400u: // FMINP (vector, 2S)
    case 0x6EA0F400u: // FMINP (vector, 4S)
    case 0x6EE0F400u: // FMINP (vector, 2D)
    case 0x2E20C400u: // FMAXNMP (vector, 2S)
    case 0x6E20C400u: // FMAXNMP (vector, 4S)
    case 0x6E60C400u: // FMAXNMP (vector, 2D)
    case 0x2EA0C400u: // FMINNMP (vector, 2S)
    case 0x6EA0C400u: // FMINNMP (vector, 4S)
    case 0x6EE0C400u: { // FMINNMP (vector, 2D)
      const std::uint32_t opcode = insn & 0xFFE0FC00u;
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      std::uint32_t esize_bits = 0u;
      std::uint32_t lanes = 0u;
      if (!vector_fp_shape(insn, esize_bits, lanes) || (lanes & 1u) != 0u) {
        return false;
      }
      const auto lhs = qregs_[rn];
      const auto rhs = qregs_[rm];
      const std::uint32_t half_lanes = lanes / 2u;
      std::array<std::uint64_t, 2> dst = {0, 0};
      std::uint64_t fpsr_bits = 0u;

      const bool is_addp =
          opcode == 0x2E20D400u || opcode == 0x6E20D400u || opcode == 0x6E60D400u;
      const bool is_min =
          opcode == 0x2EA0F400u || opcode == 0x6EA0F400u || opcode == 0x6EE0F400u ||
          opcode == 0x2EA0C400u || opcode == 0x6EA0C400u || opcode == 0x6EE0C400u;
      const bool numeric_variant =
          opcode == 0x2E20C400u || opcode == 0x6E20C400u || opcode == 0x6E60C400u ||
          opcode == 0x2EA0C400u || opcode == 0x6EA0C400u || opcode == 0x6EE0C400u;
      const std::uint32_t fpcr_mode = current_fpcr_ctl();

      for (std::uint32_t lane = 0; lane < half_lanes; ++lane) {
        const std::uint32_t src_lane = lane * 2u;
        if (esize_bits == 32u) {
          if (is_addp) {
            std::uint64_t lane_fpsr = 0u;
            const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
                static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, src_lane)),
                static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, src_lane + 1u)),
                fpcr_mode,
                HostFpBinaryOp::Add,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 32u, lane, result);
          } else {
            std::uint64_t lane_fpsr = 0u;
            const std::uint32_t result = fp_minmax_result_bits<std::uint32_t, float>(
                static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, src_lane)),
                static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, src_lane + 1u)),
                fpcr_mode,
                is_min,
                numeric_variant,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 32u, lane, result);
          }
        } else {
          if (is_addp) {
            std::uint64_t lane_fpsr = 0u;
            const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
                vector_get_elem(lhs, 64u, src_lane),
                vector_get_elem(lhs, 64u, src_lane + 1u),
                fpcr_mode,
                HostFpBinaryOp::Add,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 64u, lane, result);
          } else {
            std::uint64_t lane_fpsr = 0u;
            const std::uint64_t result = fp_minmax_result_bits<std::uint64_t, double>(
                vector_get_elem(lhs, 64u, src_lane),
                vector_get_elem(lhs, 64u, src_lane + 1u),
                fpcr_mode,
                is_min,
                numeric_variant,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 64u, lane, result);
          }
        }
      }

      for (std::uint32_t lane = 0; lane < half_lanes; ++lane) {
        const std::uint32_t src_lane = lane * 2u;
        const std::uint32_t dst_lane = lane + half_lanes;
        if (esize_bits == 32u) {
          if (is_addp) {
            std::uint64_t lane_fpsr = 0u;
            const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
                static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, src_lane)),
                static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, src_lane + 1u)),
                fpcr_mode,
                HostFpBinaryOp::Add,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 32u, dst_lane, result);
          } else {
            std::uint64_t lane_fpsr = 0u;
            const std::uint32_t result = fp_minmax_result_bits<std::uint32_t, float>(
                static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, src_lane)),
                static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, src_lane + 1u)),
                fpcr_mode,
                is_min,
                numeric_variant,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 32u, dst_lane, result);
          }
        } else {
          if (is_addp) {
            std::uint64_t lane_fpsr = 0u;
            const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
                vector_get_elem(rhs, 64u, src_lane),
                vector_get_elem(rhs, 64u, src_lane + 1u),
                fpcr_mode,
                HostFpBinaryOp::Add,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 64u, dst_lane, result);
          } else {
            std::uint64_t lane_fpsr = 0u;
            const std::uint64_t result = fp_minmax_result_bits<std::uint64_t, double>(
                vector_get_elem(rhs, 64u, src_lane),
                vector_get_elem(rhs, 64u, src_lane + 1u),
                fpcr_mode,
                is_min,
                numeric_variant,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            vector_set_elem(dst, 64u, dst_lane, result);
          }
        }
      }

      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xFFB0FC00u) == 0x6E30F800u || // FMAXV Sd, Vn.4S
      (insn & 0xFFB0FC00u) == 0x6EB0F800u || // FMINV Sd, Vn.4S
      (insn & 0xFFB0FC00u) == 0x6E30C800u || // FMAXNMV Sd, Vn.4S
      (insn & 0xFFB0FC00u) == 0x6EB0C800u) { // FMINNMV Sd, Vn.4S
    const std::uint32_t opcode = insn & 0xFFB0FC00u;
    const bool is_min = opcode == 0x6EB0F800u || opcode == 0x6EB0C800u;
    const bool numeric_variant = opcode == 0x6E30C800u || opcode == 0x6EB0C800u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint64_t fpsr_bits = 0u;
    const auto reduce_pair = [&](std::uint32_t lhs_bits, std::uint32_t rhs_bits) {
      std::uint64_t lane_fpsr = 0u;
      const std::uint32_t result = fp_minmax_result_bits<std::uint32_t, float>(
          lhs_bits,
          rhs_bits,
          fpcr_mode,
          is_min,
          numeric_variant,
          lane_fpsr);
      fpsr_bits |= lane_fpsr;
      return result;
    };
    const std::uint32_t lo = reduce_pair(
        static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 0u)),
        static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 1u)));
    const std::uint32_t hi = reduce_pair(
        static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 2u)),
        static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 3u)));
    const std::uint32_t acc = reduce_pair(lo, hi);
    sysregs_.fp_or_fpsr(fpsr_bits);
    write_fp32(rd, std::bit_cast<float>(acc));
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x7E616800u) { // FCVTXN Sd, Dn
    std::uint64_t fpsr_bits = 0u;
    const std::uint32_t result = fp64_to_fp32_bits_round_to_odd(qregs_[rn][0], current_fpcr_ctl(), fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x5EF1B800u) { // ADDP Dd, Vn.2D
    const auto src = qregs_[rn];
    qregs_[rd][0] = vector_get_elem(src, 64u, 0u) + vector_get_elem(src, 64u, 1u);
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1EE24000u) { // FCVT Sd, Hn
    std::uint64_t fpsr_bits = 0u;
    const std::uint16_t src = static_cast<std::uint16_t>(qregs_[rn][0] & 0xFFFFu);
    const std::uint32_t result = fp16_to_fp_bits<std::uint32_t>(src, current_fpcr_ctl(), fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1EE2C000u) { // FCVT Dd, Hn
    std::uint64_t fpsr_bits = 0u;
    const std::uint16_t src = static_cast<std::uint16_t>(qregs_[rn][0] & 0xFFFFu);
    const std::uint64_t result = fp16_to_fp_bits<std::uint64_t>(src, current_fpcr_ctl(), fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E22C000u) { // FCVT Dd, Sn
    std::uint64_t fpsr_bits = 0u;
    const std::uint64_t result = fp32_to_fp64_bits(
        static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
        current_fpcr_ctl(),
        fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E624000u) { // FCVT Sd, Dn
    std::uint64_t fpsr_bits = 0u;
    const std::uint32_t result = fp64_to_fp32_bits(qregs_[rn][0], current_fpcr_ctl(), fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFF3FFC00u) == 0x1E23C000u) { // FCVT Hd, Sn / Dn
    std::uint64_t fpsr_bits = 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    std::uint16_t result = 0u;
    if (ftype == 0u) {
      result = fp_to_fp16_bits<std::uint32_t>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          current_fpcr_ctl(),
          fpsr_bits);
    } else if (ftype == 1u) {
      result = fp_to_fp16_bits<std::uint64_t>(qregs_[rn][0], current_fpcr_ctl(), fpsr_bits);
    } else {
      return false;
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E20D400u) { // FADD/FSUB (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_sub = ((insn >> 23) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            fpcr_mode,
            is_sub ? HostFpBinaryOp::Sub : HostFpBinaryOp::Add,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            fpcr_mode,
            is_sub ? HostFpBinaryOp::Sub : HostFpBinaryOp::Add,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E20DC00u) { // FMULX (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_mulx_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            fpcr_mode,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_mulx_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            fpcr_mode,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x2E20DC00u) { // FMUL (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            fpcr_mode,
            HostFpBinaryOp::Mul,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            fpcr_mode,
            HostFpBinaryOp::Mul,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF80F400u) == 0x0F809000u) { // FMUL (by element, vector)
    const std::uint32_t sz = (insn >> 22) & 1u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    if (sz != 0u && l != 0u) {
      return false;
    }
    const std::uint32_t index = sz == 0u ? ((h << 1u) | l) : h;
    const std::uint32_t rm = ((insn >> 16) & 0xFu) | (((insn >> 20) & 1u) << 4u);
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0u;
    std::uint32_t lanes = 0u;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    std::uint64_t fpsr_bits = 0u;
    if (esize_bits == 32u) {
      const std::uint32_t elem = static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, index));
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            elem,
            fpcr_mode,
            HostFpBinaryOp::Mul,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      }
    } else {
      const std::uint64_t elem = vector_get_elem(rhs, 64u, index);
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            elem,
            fpcr_mode,
            HostFpBinaryOp::Mul,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF80F400u) == 0x2F809000u) { // FMULX (by element, vector)
    const std::uint32_t sz = (insn >> 22) & 1u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    if (sz != 0u && l != 0u) {
      return false;
    }
    const std::uint32_t index = sz == 0u ? ((h << 1u) | l) : h;
    const std::uint32_t rm = ((insn >> 16) & 0xFu) | (((insn >> 20) & 1u) << 4u);
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0u;
    std::uint32_t lanes = 0u;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    std::uint64_t fpsr_bits = 0u;
    if (esize_bits == 32u) {
      const std::uint32_t elem = static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, index));
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_mulx_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            elem,
            fpcr_mode,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      }
    } else {
      const std::uint64_t elem = vector_get_elem(rhs, 64u, index);
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_mulx_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            elem,
            fpcr_mode,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF00F400u) == 0x0F008000u || // MUL (by element, vector)
      (insn & 0xBF00F400u) == 0x2F000000u || // MLA (by element, vector)
      (insn & 0xBF00F400u) == 0x2F004000u) { // MLS (by element, vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t tag = insn & 0xBF00F400u;
    const bool accumulate = tag != 0x0F008000u;
    const bool subtract = tag == 0x2F004000u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t m = (insn >> 20) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    std::uint32_t index = 0u;
    std::uint32_t rm = 0u;
    switch (size) {
      case 1u:
        index = (h << 2u) | (l << 1u) | m;
        rm = (insn >> 16) & 0xFu;
        break;
      case 2u:
        index = (h << 1u) | l;
        rm = ((insn >> 16) & 0xFu) | (m << 4u);
        break;
      default:
        return false;
    }
    const std::uint32_t esize_bits = 8u << size;
    const std::uint64_t mask = ones(esize_bits);
    const std::uint32_t total_bits = q ? 128u : 64u;
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    const std::uint64_t elem = vector_get_elem(rhs, esize_bits, index);
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t prod =
          (vector_get_elem(lhs, esize_bits, lane) * elem) & mask;
      const std::uint64_t value = accumulate
          ? ((subtract
                  ? (vector_get_elem(acc, esize_bits, lane) - prod)
                  : (vector_get_elem(acc, esize_bits, lane) + prod)) &
             mask)
          : prod;
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF00F400u) == 0x0F00C000u || // SQDMULH (by element, vector)
      (insn & 0xBF00F400u) == 0x0F00D000u) { // SQRDMULH (by element, vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool round = (insn & 0x00001000u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t m = (insn >> 20) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    std::uint32_t index = 0u;
    std::uint32_t rm = 0u;
    switch (size) {
      case 1u:
        index = (h << 2u) | (l << 1u) | m;
        rm = (insn >> 16) & 0xFu;
        break;
      case 2u:
        index = (h << 1u) | l;
        rm = ((insn >> 16) & 0xFu) | (m << 4u);
        break;
      default:
        return false;
    }
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    const std::uint64_t elem = vector_get_elem(rhs, esize_bits, index);
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    bool saturated = false;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      bool lane_saturated = false;
      const std::uint64_t value = sqdmulh_result(
          vector_get_elem(lhs, esize_bits, lane),
          elem,
          esize_bits,
          round,
          lane_saturated);
      saturated |= lane_saturated;
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0xBF00F400u) == 0x0F00B000u || // SQDMULL (by element, vector)
      (insn & 0xBF00F400u) == 0x0F003000u || // SQDMLAL (by element, vector)
      (insn & 0xBF00F400u) == 0x0F007000u) { // SQDMLSL (by element, vector)
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t tag = insn & 0xBF00F400u;
    const bool accumulate = tag != 0x0F00B000u;
    const bool subtract = tag == 0x0F007000u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t m = (insn >> 20) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    std::uint32_t index = 0u;
    std::uint32_t rm = 0u;
    switch (size) {
      case 1u:
        index = (h << 2u) | (l << 1u) | m;
        rm = (insn >> 16) & 0xFu;
        break;
      case 2u:
        index = (h << 1u) | l;
        rm = ((insn >> 16) & 0xFu) | (m << 4u);
        break;
      default:
        return false;
    }
    const std::uint32_t src_esize_bits = 8u << size;
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t lanes = 64u / src_esize_bits;
    const std::uint32_t src_base = upper_half ? lanes : 0u;
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    const std::uint64_t elem = vector_get_elem(rhs, src_esize_bits, index);
    bool saturated = false;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      bool lane_saturated = false;
      const std::uint64_t value = accumulate
          ? sqdml_long_accumulate_result(
                vector_get_elem(acc, dst_esize_bits, lane),
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                elem,
                src_esize_bits,
                subtract,
                lane_saturated)
          : sqdmull_product_result(
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                elem,
                src_esize_bits,
                lane_saturated);
      saturated |= lane_saturated;
      vector_set_elem(dst, dst_esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0xBF00F400u) == 0x0F00A000u || // SMULL/SMULL2 (by element, vector)
      (insn & 0xBF00F400u) == 0x0F002000u || // SMLAL/SMLAL2 (by element, vector)
      (insn & 0xBF00F400u) == 0x0F006000u || // SMLSL/SMLSL2 (by element, vector)
      (insn & 0xBF00F400u) == 0x2F00A000u || // UMULL/UMULL2 (by element, vector)
      (insn & 0xBF00F400u) == 0x2F002000u || // UMLAL/UMLAL2 (by element, vector)
      (insn & 0xBF00F400u) == 0x2F006000u) { // UMLSL/UMLSL2 (by element, vector)
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t tag = insn & 0x9F00F400u;
    const bool accumulate = tag != 0x0F00A000u;
    const bool subtract = tag == 0x0F006000u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t m = (insn >> 20) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    std::uint32_t index = 0u;
    std::uint32_t rm = 0u;
    switch (size) {
      case 1u:
        index = (h << 2u) | (l << 1u) | m;
        rm = (insn >> 16) & 0xFu;
        break;
      case 2u:
        index = (h << 1u) | l;
        rm = ((insn >> 16) & 0xFu) | (m << 4u);
        break;
      default:
        return false;
    }
    const std::uint32_t src_esize_bits = 8u << size;
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t lanes = 64u / src_esize_bits;
    const std::uint32_t src_base = upper_half ? lanes : 0u;
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    const std::uint64_t elem = vector_get_elem(rhs, src_esize_bits, index);
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t value = accumulate
          ? long_multiply_accumulate_result(
                vector_get_elem(acc, dst_esize_bits, lane),
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                elem,
                src_esize_bits,
                subtract,
                is_unsigned)
          : long_multiply_result(
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                elem,
                src_esize_bits,
                is_unsigned);
      vector_set_elem(dst, dst_esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF80F400u) == 0x0F801000u || // FMLA (by element, vector)
      (insn & 0xBF80F400u) == 0x0F805000u) { // FMLS (by element, vector)
    const std::uint32_t sz = (insn >> 22) & 1u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    if (sz != 0u && l != 0u) {
      return false;
    }
    const bool is_sub = (insn & 0x00004000u) != 0u;
    const std::uint32_t index = sz == 0u ? ((h << 1u) | l) : h;
    const std::uint32_t rm = ((insn >> 16) & 0xFu) | (((insn >> 20) & 1u) << 4u);
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0u;
    std::uint32_t lanes = 0u;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    std::uint64_t fpsr_bits = 0u;
    if (esize_bits == 32u) {
      const std::uint32_t elem = static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, index));
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_fma_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            elem,
            static_cast<std::uint32_t>(vector_get_elem(acc, 32u, lane)),
            fpcr_mode,
            is_sub,
            false,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      }
    } else {
      const std::uint64_t elem = vector_get_elem(rhs, 64u, index);
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_fma_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            elem,
            vector_get_elem(acc, 64u, lane),
            fpcr_mode,
            is_sub,
            false,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBFA0FC00u) == 0x0E20FC00u || // FRECPS (vector)
      (insn & 0xBFA0FC00u) == 0x0EA0FC00u) { // FRSQRTS (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_rsqrt = (insn & 0x00800000u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0u;
    std::uint32_t lanes = 0u;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = is_rsqrt
            ? fp_rsqrt_step_fused_bits<std::uint32_t, float>(
                  static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
                  static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
                  fpcr_mode,
                  lane_fpsr)
            : fp_recip_step_fused_bits<std::uint32_t, float>(
                  static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
                  static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
                  fpcr_mode,
                  lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = is_rsqrt
            ? fp_rsqrt_step_fused_bits<std::uint64_t, double>(
                  vector_get_elem(lhs, 64u, lane),
                  vector_get_elem(rhs, 64u, lane),
                  fpcr_mode,
                  lane_fpsr)
            : fp_recip_step_fused_bits<std::uint64_t, double>(
                  vector_get_elem(lhs, 64u, lane),
                  vector_get_elem(rhs, 64u, lane),
                  fpcr_mode,
                  lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E20FC00u) { // FDIV (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            fpcr_mode,
            HostFpBinaryOp::Div,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            fpcr_mode,
            HostFpBinaryOp::Div,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E20F400u) { // FMAX/FMIN (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_min = ((insn >> 23) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_minmax_result_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            fpcr_mode,
            is_min,
            false,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_minmax_result_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            fpcr_mode,
            is_min,
            false,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E20CC00u) { // FMLA/FMLS (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_sub = ((insn >> 23) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_fma_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(acc, 32u, lane)),
            fpcr_mode,
            is_sub,
            false,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_fma_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            vector_get_elem(acc, 64u, lane),
            fpcr_mode,
            is_sub,
            false,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E20C400u) { // FMAXNM/FMINNM (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_min = ((insn >> 23) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        const std::uint32_t result = fp_minmax_result_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            fpcr_mode,
            is_min,
            true,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        const std::uint64_t result = fp_minmax_result_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            fpcr_mode,
            is_min,
            true,
            lane_fpsr);
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x2E20D400u) { // FABD (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      if (esize_bits == 32u) {
        std::uint64_t lane_fpsr = 0u;
        std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane)),
            static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane)),
            fpcr_mode,
            HostFpBinaryOp::Sub,
            lane_fpsr);
        result &= ~fp_sign_mask_bits<std::uint32_t>();
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 32u, lane, result);
      } else {
        std::uint64_t lane_fpsr = 0u;
        std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
            vector_get_elem(lhs, 64u, lane),
            vector_get_elem(rhs, 64u, lane),
            fpcr_mode,
            HostFpBinaryOp::Sub,
            lane_fpsr);
        result &= ~fp_sign_mask_bits<std::uint64_t>();
        fpsr_bits |= lane_fpsr;
        vector_set_elem(dst, 64u, lane, result);
      }
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xDF20FC00u) == 0x4E20EC00u) { // FACGE/FACGT (vector)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_gt = ((insn >> 23) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      bool predicate = false;
      if (esize_bits == 32u) {
        std::uint32_t a_bits = static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane));
        std::uint32_t b_bits = static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane));
        a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
        b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
        const float a = std::bit_cast<float>(a_bits);
        const float b = std::bit_cast<float>(b_bits);
        fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
        if (!std::isnan(a) && !std::isnan(b)) {
          predicate = is_gt ? (std::fabs(a) > std::fabs(b)) : (std::fabs(a) >= std::fabs(b));
        }
      } else {
        std::uint64_t a_bits = vector_get_elem(lhs, 64u, lane);
        std::uint64_t b_bits = vector_get_elem(rhs, 64u, lane);
        a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
        b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
        const double a = std::bit_cast<double>(a_bits);
        const double b = std::bit_cast<double>(b_bits);
        fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
        if (!std::isnan(a) && !std::isnan(b)) {
          predicate = is_gt ? (std::fabs(a) > std::fabs(b)) : (std::fabs(a) >= std::fabs(b));
        }
      }
      vector_set_elem(dst, esize_bits, lane, vector_fp_compare_mask(predicate, esize_bits));
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E20E400u) { // FCMEQ/FCMGE/FCMGT (vector, register)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_eq = ((insn >> 29) & 1u) == 0u;
    const bool is_gt = !is_eq && (((insn >> 23) & 1u) != 0u);
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      bool predicate = false;
      if (esize_bits == 32u) {
        std::uint32_t a_bits = static_cast<std::uint32_t>(vector_get_elem(lhs, 32u, lane));
        std::uint32_t b_bits = static_cast<std::uint32_t>(vector_get_elem(rhs, 32u, lane));
        a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
        b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
        const float a = std::bit_cast<float>(a_bits);
        const float b = std::bit_cast<float>(b_bits);
        fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
        if (!std::isnan(a) && !std::isnan(b)) {
          predicate = is_eq ? (a == b) : (is_gt ? (a > b) : (a >= b));
        }
      } else {
        std::uint64_t a_bits = vector_get_elem(lhs, 64u, lane);
        std::uint64_t b_bits = vector_get_elem(rhs, 64u, lane);
        a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
        b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
        const double a = std::bit_cast<double>(a_bits);
        const double b = std::bit_cast<double>(b_bits);
        fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
        if (!std::isnan(a) && !std::isnan(b)) {
          predicate = is_eq ? (a == b) : (is_gt ? (a > b) : (a >= b));
        }
      }
      vector_set_elem(dst, esize_bits, lane, vector_fp_compare_mask(predicate, esize_bits));
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF3FFC00u) == 0x0E20D800u ||
      (insn & 0xBF3FFC00u) == 0x2E20C800u ||
      (insn & 0xBF3FFC00u) == 0x0E20C800u ||
      (insn & 0xBF3FFC00u) == 0x2E20D800u ||
      (insn & 0xBF3FFC00u) == 0x0E20E800u) { // FCM* (vector, zero)
    const std::uint32_t tag = insn & 0xBF3FFC00u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint32_t esize_bits = 0;
    std::uint32_t lanes = 0;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {0, 0};
    std::uint64_t fpsr_bits = 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      bool predicate = false;
      if (esize_bits == 32u) {
        std::uint32_t a_bits = static_cast<std::uint32_t>(vector_get_elem(src, 32u, lane));
        a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
        const float a = std::bit_cast<float>(a_bits);
        fpsr_bits |= fp_compare_fpsr_bits(a_bits, static_cast<std::uint32_t>(0), false);
        if (!std::isnan(a)) {
          switch (tag) {
            case 0x0E20D800u: predicate = (a == 0.0f); break;
            case 0x2E20C800u: predicate = (a >= 0.0f); break;
            case 0x0E20C800u: predicate = (a > 0.0f); break;
            case 0x2E20D800u: predicate = (a <= 0.0f); break;
            case 0x0E20E800u: predicate = (a < 0.0f); break;
          }
        }
      } else {
        std::uint64_t a_bits = vector_get_elem(src, 64u, lane);
        a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
        const double a = std::bit_cast<double>(a_bits);
        fpsr_bits |= fp_compare_fpsr_bits(a_bits, static_cast<std::uint64_t>(0), false);
        if (!std::isnan(a)) {
          switch (tag) {
            case 0x0E20D800u: predicate = (a == 0.0); break;
            case 0x2E20C800u: predicate = (a >= 0.0); break;
            case 0x0E20C800u: predicate = (a > 0.0); break;
            case 0x2E20D800u: predicate = (a <= 0.0); break;
            case 0x0E20E800u: predicate = (a < 0.0); break;
          }
        }
      }
      vector_set_elem(dst, esize_bits, lane, vector_fp_compare_mask(predicate, esize_bits));
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  switch (insn & 0xFF3FFC00u) {
    case 0x0E216800u: // FCVTN Vd.2S, Vn.2D
    case 0x4E216800u: // FCVTN2 Vd.4S, Vn.2D
    case 0x2E216800u: // FCVTXN Vd.2S, Vn.2D
    case 0x6E216800u: // FCVTXN2 Vd.4S, Vn.2D
    case 0x0E217800u: // FCVTL Vd.2D, Vn.2S
    case 0x4E217800u: { // FCVTL2 Vd.2D, Vn.4S
      const std::uint32_t opcode = insn & 0xFF3FFC00u;
      const std::uint32_t ftype = (insn >> 22) & 0x3u;
      const bool upper = ((insn >> 30) & 1u) != 0u;
      std::uint64_t fpsr_bits = 0u;
      if (opcode == 0x0E216800u || opcode == 0x4E216800u ||
          opcode == 0x2E216800u || opcode == 0x6E216800u) {
        if (ftype == 2u && (opcode == 0x0E216800u || opcode == 0x4E216800u)) {
          const std::uint32_t bf16 = static_cast<std::uint32_t>((sysregs_.id_aa64isar1_el1() >> 44) & 0xFu);
          if (bf16 == 0u) {
            enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
            return true;
          }
          return false;
        }
        const bool round_to_odd = opcode == 0x2E216800u || opcode == 0x6E216800u;
        const std::uint32_t fpcr_mode = current_fpcr_ctl();
        if (ftype == 1u) {
          std::array<std::uint64_t, 2> dst = upper ? qregs_[rd] : std::array<std::uint64_t, 2>{0u, 0u};
          std::uint64_t result64 = 0u;
          for (std::uint32_t lane = 0; lane < 2u; ++lane) {
            std::uint64_t lane_fpsr = 0u;
            const std::uint32_t result = round_to_odd
                ? fp64_to_fp32_bits_round_to_odd(vector_get_elem(qregs_[rn], 64u, lane), fpcr_mode, lane_fpsr)
                : fp64_to_fp32_bits(vector_get_elem(qregs_[rn], 64u, lane), fpcr_mode, lane_fpsr);
            fpsr_bits |= lane_fpsr;
            result64 |= static_cast<std::uint64_t>(result) << (lane * 32u);
          }
          if (!upper) {
            dst[0] = result64;
            dst[1] = 0u;
          } else {
            dst[1] = result64;
          }
          sysregs_.fp_or_fpsr(fpsr_bits);
          qregs_[rd] = dst;
          return true;
        }
        if (ftype == 0u && !round_to_odd) {
          std::array<std::uint64_t, 2> dst = upper ? qregs_[rd] : std::array<std::uint64_t, 2>{0u, 0u};
          std::uint64_t result64 = 0u;
          for (std::uint32_t lane = 0; lane < 4u; ++lane) {
            std::uint64_t lane_fpsr = 0u;
            const std::uint16_t result = fp_to_fp16_bits<std::uint32_t>(
                static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, lane)),
                fpcr_mode,
                lane_fpsr);
            fpsr_bits |= lane_fpsr;
            result64 |= static_cast<std::uint64_t>(result) << (lane * 16u);
          }
          if (!upper) {
            dst[0] = result64;
            dst[1] = 0u;
          } else {
            dst[1] = result64;
          }
          sysregs_.fp_or_fpsr(fpsr_bits);
          qregs_[rd] = dst;
          return true;
        }
        return false;
      }

      if (ftype == 1u) {
        std::array<std::uint64_t, 2> dst = {0u, 0u};
        for (std::uint32_t lane = 0; lane < 2u; ++lane) {
          std::uint64_t lane_fpsr = 0u;
          const std::uint32_t src_lane = upper ? lane + 2u : lane;
          const std::uint64_t result = fp32_to_fp64_bits(
              static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, src_lane)),
              current_fpcr_ctl(),
              lane_fpsr);
          fpsr_bits |= lane_fpsr;
          vector_set_elem(dst, 64u, lane, result);
        }
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd] = dst;
        return true;
      }
      if (ftype == 0u) {
        std::array<std::uint64_t, 2> dst = {0u, 0u};
        for (std::uint32_t lane = 0; lane < 4u; ++lane) {
          std::uint64_t lane_fpsr = 0u;
          const std::uint32_t src_lane = upper ? lane + 4u : lane;
          const std::uint32_t result = fp16_to_fp_bits<std::uint32_t>(
              static_cast<std::uint16_t>(vector_get_elem(qregs_[rn], 16u, src_lane)),
              current_fpcr_ctl(),
              lane_fpsr);
          fpsr_bits |= lane_fpsr;
          vector_set_elem(dst, 32u, lane, result);
        }
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd] = dst;
        return true;
      }
      return false;
    }
  }

  if ((insn & 0xBFF1FC00u) == 0x0EA1B800u || // FCVTZS (vector, S)
      (insn & 0xBFF1FC00u) == 0x2EA1B800u || // FCVTZU (vector, S)
      (insn & 0xBFF1FC00u) == 0x0EE1B800u || // FCVTZS (vector, D)
      (insn & 0xBFF1FC00u) == 0x2EE1B800u) { // FCVTZU (vector, D)
    const std::uint32_t opcode = insn & 0xBFF1FC00u;
    std::uint32_t esize_bits = 0u;
    std::uint32_t lanes = 0u;
    if (!vector_fp_shape(insn, esize_bits, lanes)) {
      return false;
    }
    const bool is_unsigned = (opcode & 0x20000000u) != 0u;
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    std::uint64_t fpsr_bits = 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      std::uint64_t lane_fpsr = 0u;
      if (esize_bits == 32u) {
        const long double value = fp_to_int_input_value<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(src, 32u, lane)),
            fpcr_mode,
            lane_fpsr);
        std::uint64_t convert_fpsr = 0u;
        if (is_unsigned) {
          const std::uint32_t result = fp_to_unsigned_rtz<std::uint32_t>(value, convert_fpsr);
          vector_set_elem(dst, 32u, lane, result);
        } else {
          const std::int32_t result = fp_to_signed_rtz<std::int32_t>(value, convert_fpsr);
          vector_set_elem(dst, 32u, lane, static_cast<std::uint32_t>(result));
        }
        lane_fpsr |= convert_fpsr;
      } else {
        const long double value = fp_to_int_input_value<std::uint64_t, double>(
            vector_get_elem(src, 64u, lane),
            fpcr_mode,
            lane_fpsr);
        std::uint64_t convert_fpsr = 0u;
        if (is_unsigned) {
          const std::uint64_t result = fp_to_unsigned_rtz<std::uint64_t>(value, convert_fpsr);
          vector_set_elem(dst, 64u, lane, result);
        } else {
          const std::int64_t result = fp_to_signed_rtz<std::int64_t>(value, convert_fpsr);
          vector_set_elem(dst, 64u, lane, static_cast<std::uint64_t>(result));
        }
        lane_fpsr |= convert_fpsr;
      }
      fpsr_bits |= lane_fpsr;
    }
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd] = dst;
    return true;
  }

  switch (insn & 0xBFF0FC00u) {
    case 0x0E20A800u: // FCVTNS (vector, S)
    case 0x2E20A800u: // FCVTNU (vector, S)
    case 0x0EA0A800u: // FCVTPS (vector, S)
    case 0x2EA0A800u: // FCVTPU (vector, S)
    case 0x0E20B800u: // FCVTMS (vector, S)
    case 0x2E20B800u: // FCVTMU (vector, S)
    case 0x0E20C800u: // FCVTAS (vector, S)
    case 0x2E20C800u: // FCVTAU (vector, S)
    case 0x0E60A800u: // FCVTNS (vector, D)
    case 0x2E60A800u: // FCVTNU (vector, D)
    case 0x0EE0A800u: // FCVTPS (vector, D)
    case 0x2EE0A800u: // FCVTPU (vector, D)
    case 0x0E60B800u: // FCVTMS (vector, D)
    case 0x2E60B800u: // FCVTMU (vector, D)
    case 0x0E60C800u: // FCVTAS (vector, D)
    case 0x2E60C800u: { // FCVTAU (vector, D)
      const std::uint32_t opcode = insn & 0xBFF0FC00u;
      std::uint32_t esize_bits = 0u;
      std::uint32_t lanes = 0u;
      if (!vector_fp_shape(insn, esize_bits, lanes)) {
        return false;
      }
      const bool is_unsigned = (opcode & 0x20000000u) != 0u;
      FpToIntRoundingMode rounding = FpToIntRoundingMode::TieEven;
      switch (opcode) {
        case 0x0E20A800u:
        case 0x2E20A800u:
        case 0x0E60A800u:
        case 0x2E60A800u:
          rounding = FpToIntRoundingMode::TieEven;
          break;
        case 0x0EA0A800u:
        case 0x2EA0A800u:
        case 0x0EE0A800u:
        case 0x2EE0A800u:
          rounding = FpToIntRoundingMode::PosInf;
          break;
        case 0x0E20B800u:
        case 0x2E20B800u:
        case 0x0E60B800u:
        case 0x2E60B800u:
          rounding = FpToIntRoundingMode::NegInf;
          break;
        case 0x0E20C800u:
        case 0x2E20C800u:
        case 0x0E60C800u:
        case 0x2E60C800u:
          rounding = FpToIntRoundingMode::TieAway;
          break;
      }
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0, 0};
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t lane_fpsr = 0u;
        if (esize_bits == 32u) {
          const long double value = fp_to_int_input_value<std::uint32_t, float>(
              static_cast<std::uint32_t>(vector_get_elem(src, 32u, lane)),
              fpcr_mode,
              lane_fpsr);
          std::uint64_t convert_fpsr = 0u;
          if (is_unsigned) {
            const std::uint32_t result = fp_to_unsigned_round<std::uint32_t>(value, rounding, convert_fpsr);
            vector_set_elem(dst, 32u, lane, result);
          } else {
            const std::int32_t result = fp_to_signed_round<std::int32_t>(value, rounding, convert_fpsr);
            vector_set_elem(dst, 32u, lane, static_cast<std::uint32_t>(result));
          }
          lane_fpsr |= convert_fpsr;
        } else {
          const long double value = fp_to_int_input_value<std::uint64_t, double>(
              vector_get_elem(src, 64u, lane),
              fpcr_mode,
              lane_fpsr);
          std::uint64_t convert_fpsr = 0u;
          if (is_unsigned) {
            const std::uint64_t result = fp_to_unsigned_round<std::uint64_t>(value, rounding, convert_fpsr);
            vector_set_elem(dst, 64u, lane, result);
          } else {
            const std::int64_t result = fp_to_signed_round<std::int64_t>(value, rounding, convert_fpsr);
            vector_set_elem(dst, 64u, lane, static_cast<std::uint64_t>(result));
          }
          lane_fpsr |= convert_fpsr;
        }
        fpsr_bits |= lane_fpsr;
      }
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd] = dst;
      return true;
    }
  }


  switch (insn & 0xBFE1FC00u) {
    case 0x0EA1C800u: // URECPE (vector)
    case 0x2EA1C800u: { // URSQRTE (vector)
      const bool is_rsqrt = ((insn >> 29) & 1u) != 0u;
      const bool q = ((insn >> 30) & 1u) != 0u;
      const std::uint32_t lanes = q ? 4u : 2u;
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0u, 0u};
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        const std::uint32_t operand = static_cast<std::uint32_t>(vector_get_elem(src, 32u, lane));
        const std::uint32_t result = is_rsqrt
            ? advsimd_unsigned_rsqrt_estimate_bits(operand)
            : advsimd_unsigned_recip_estimate_bits(operand);
        vector_set_elem(dst, 32u, lane, result);
      }
      qregs_[rd] = dst;
      return true;
    }
  }

  switch (insn & 0xBFF1FC00u) {
    case 0x0EA1D800u: // FRECPE (vector, S)
    case 0x2EA1D800u: // FRSQRTE (vector, S)
    case 0x0EE1D800u: // FRECPE (vector, D)
    case 0x2EE1D800u: // FRSQRTE (vector, D)
    case 0x0EA0F800u: // FABS (vector, S)
    case 0x2EA0F800u: // FNEG (vector, S)
    case 0x2EA1F800u: // FSQRT (vector, S)
    case 0x0EE0F800u: // FABS (vector, D)
    case 0x2EE0F800u: // FNEG (vector, D)
    case 0x2EE1F800u: { // FSQRT (vector, D)
      const std::uint32_t opcode = insn & 0xBFF1FC00u;
      std::uint32_t esize_bits = 0u;
      std::uint32_t lanes = 0u;
      if (!vector_fp_shape(insn, esize_bits, lanes)) {
        return false;
      }
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0, 0};
      std::uint64_t fpsr_bits = 0u;
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        if (esize_bits == 32u) {
          const std::uint32_t value_bits = static_cast<std::uint32_t>(vector_get_elem(src, 32u, lane));
          std::uint32_t result_bits = value_bits;
          switch (opcode) {
            case 0x0EA1D800u: {
              std::uint64_t lane_fpsr = 0u;
              const std::uint32_t mode = current_fpcr_ctl();
              result_bits = fp_recip_estimate_bits<std::uint32_t>(value_bits, mode, lane_fpsr);
              fpsr_bits |= lane_fpsr;
              break;
            }
            case 0x2EA1D800u: {
              std::uint64_t lane_fpsr = 0u;
              result_bits = fp_rsqrt_estimate_bits<std::uint32_t>(value_bits, current_fpcr_ctl(), lane_fpsr);
              fpsr_bits |= lane_fpsr;
              break;
            }
            case 0x0EA0F800u: result_bits = fp_abs_bits(value_bits); break;
            case 0x2EA0F800u: result_bits = fp_neg_bits(value_bits); break;
            case 0x2EA1F800u: {
              std::uint64_t lane_fpsr = 0u;
              result_bits = fp_sqrt_bits<std::uint32_t, float>(value_bits, current_fpcr_ctl(), lane_fpsr);
              fpsr_bits |= lane_fpsr;
              break;
            }
            default: return false;
          }
          vector_set_elem(dst, 32u, lane, result_bits);
        } else {
          const std::uint64_t value_bits = vector_get_elem(src, 64u, lane);
          std::uint64_t result_bits = value_bits;
          switch (opcode) {
            case 0x0EE1D800u: {
              std::uint64_t lane_fpsr = 0u;
              const std::uint32_t mode = current_fpcr_ctl();
              result_bits = fp_recip_estimate_bits<std::uint64_t>(value_bits, mode, lane_fpsr);
              fpsr_bits |= lane_fpsr;
              break;
            }
            case 0x2EE1D800u: {
              std::uint64_t lane_fpsr = 0u;
              result_bits = fp_rsqrt_estimate_bits<std::uint64_t>(value_bits, current_fpcr_ctl(), lane_fpsr);
              fpsr_bits |= lane_fpsr;
              break;
            }
            case 0x0EE0F800u: result_bits = fp_abs_bits(value_bits); break;
            case 0x2EE0F800u: result_bits = fp_neg_bits(value_bits); break;
            case 0x2EE1F800u: {
              std::uint64_t lane_fpsr = 0u;
              result_bits = fp_sqrt_bits<std::uint64_t, double>(value_bits, current_fpcr_ctl(), lane_fpsr);
              fpsr_bits |= lane_fpsr;
              break;
            }
            default: return false;
          }
          vector_set_elem(dst, 64u, lane, result_bits);
        }
      }
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd] = dst;
      return true;
    }
    case 0x0E218800u: // FRINTN (vector, S)
    case 0x0EA18800u: // FRINTP (vector, S)
    case 0x0E219800u: // FRINTM (vector, S)
    case 0x0EA19800u: // FRINTZ (vector, S)
    case 0x2E218800u: // FRINTA (vector, S)
    case 0x2E219800u: // FRINTX (vector, S)
    case 0x2EA19800u: // FRINTI (vector, S)
    case 0x0E618800u: // FRINTN (vector, D)
    case 0x0EE18800u: // FRINTP (vector, D)
    case 0x0E619800u: // FRINTM (vector, D)
    case 0x0EE19800u: // FRINTZ (vector, D)
    case 0x2E618800u: // FRINTA (vector, D)
    case 0x2E619800u: // FRINTX (vector, D)
    case 0x2EE19800u: { // FRINTI (vector, D)
      const std::uint32_t opcode = insn & 0xBFF1FC00u;
      std::uint32_t esize_bits = 0u;
      std::uint32_t lanes = 0u;
      if (!vector_fp_shape(insn, esize_bits, lanes)) {
        return false;
      }
      const auto src = qregs_[rn];
      std::array<std::uint64_t, 2> dst = {0, 0};
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      const bool exact = opcode == 0x2E219800u || opcode == 0x2E619800u;
      std::uint64_t fpsr_bits = 0u;
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        if (esize_bits == 32u) {
          const std::uint32_t value_bits = static_cast<std::uint32_t>(vector_get_elem(src, 32u, lane));
          std::pair<std::uint32_t, std::uint64_t> rounded = {value_bits, 0u};
          switch (opcode) {
            case 0x0E218800u: rounded = fp_round_int_bits(value_bits, fp_round_ties_even, exact); break;
            case 0x0EA18800u: rounded = fp_round_int_bits(value_bits, static_cast<float (*)(float)>(std::ceil), exact); break;
            case 0x0E219800u: rounded = fp_round_int_bits(value_bits, static_cast<float (*)(float)>(std::floor), exact); break;
            case 0x0EA19800u: rounded = fp_round_int_bits(value_bits, static_cast<float (*)(float)>(std::trunc), exact); break;
            case 0x2E218800u: rounded = fp_round_int_bits(value_bits, fp_round_ties_away, exact); break;
            case 0x2E219800u:
            case 0x2EA19800u:
              rounded = fp_round_int_bits(value_bits, [&](float v) { return fp_round_by_mode(v, fpcr_mode); }, exact);
              break;
            default:
              return false;
          }
          fpsr_bits |= rounded.second;
          vector_set_elem(dst, 32u, lane, rounded.first);
        } else {
          const std::uint64_t value_bits = vector_get_elem(src, 64u, lane);
          std::pair<std::uint64_t, std::uint64_t> rounded = {value_bits, 0u};
          switch (opcode) {
            case 0x0E618800u: rounded = fp_round_int_bits(value_bits, fp_round_ties_even, exact); break;
            case 0x0EE18800u: rounded = fp_round_int_bits(value_bits, static_cast<double (*)(double)>(std::ceil), exact); break;
            case 0x0E619800u: rounded = fp_round_int_bits(value_bits, static_cast<double (*)(double)>(std::floor), exact); break;
            case 0x0EE19800u: rounded = fp_round_int_bits(value_bits, static_cast<double (*)(double)>(std::trunc), exact); break;
            case 0x2E618800u: rounded = fp_round_int_bits(value_bits, fp_round_ties_away, exact); break;
            case 0x2E619800u:
            case 0x2EE19800u:
              rounded = fp_round_int_bits(value_bits, [&](double v) { return fp_round_by_mode(v, fpcr_mode); }, exact);
              break;
            default:
              return false;
          }
          fpsr_bits |= rounded.second;
          vector_set_elem(dst, 64u, lane, rounded.first);
        }
      }
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xBF20FC00u) == 0x0E208400u) { // ADD (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t sum = (vector_get_elem(lhs, esize_bits, lane) + vector_get_elem(rhs, esize_bits, lane)) & ones(esize_bits);
      vector_set_elem(dst, esize_bits, lane, sum);
    }
    qregs_[rd] = dst;
    return true;
  }

  const auto vector_unsigned_abs_diff = [](std::uint64_t a, std::uint64_t b) -> std::uint64_t {
    return (a >= b) ? (a - b) : (b - a);
  };

  const auto vector_signed_abs_diff =
      [&](std::uint64_t a, std::uint64_t b, std::uint32_t esize_bits) -> std::uint64_t {
    __int128_t diff = static_cast<__int128_t>(sign_extend_bits(a, esize_bits)) -
                      static_cast<__int128_t>(sign_extend_bits(b, esize_bits));
    if (diff < 0) {
      diff = -diff;
    }
    return static_cast<std::uint64_t>(diff);
  };

  if ((insn & 0x9F20FC00u) == 0x0E200400u || // SHADD/UHADD (vector)
      (insn & 0x9F20FC00u) == 0x0E201400u || // SRHADD/URHADD (vector)
      (insn & 0x9F20FC00u) == 0x0E202400u) { // SHSUB/UHSUB (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t tag = insn & 0x9F20FC00u;
    const bool is_subtract = tag == 0x0E202400u;
    const bool is_rounded = tag == 0x0E201400u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > 32u || esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, esize_bits, lane);
      const std::uint64_t b = vector_get_elem(rhs, esize_bits, lane);
      std::uint64_t value = 0u;
      if (is_unsigned) {
        if (is_subtract) {
          const __int128_t diff = static_cast<__int128_t>(a) - static_cast<__int128_t>(b);
          value = static_cast<std::uint64_t>(diff >> 1u) & ones(esize_bits);
        } else {
          __uint128_t sum = static_cast<__uint128_t>(a) + static_cast<__uint128_t>(b);
          if (is_rounded) {
            sum += 1u;
          }
          value = static_cast<std::uint64_t>(sum >> 1u) & ones(esize_bits);
        }
      } else {
        const __int128_t lhs_value = static_cast<__int128_t>(sign_extend_bits(a, esize_bits));
        const __int128_t rhs_value = static_cast<__int128_t>(sign_extend_bits(b, esize_bits));
        __int128_t result = is_subtract ? (lhs_value - rhs_value) : (lhs_value + rhs_value);
        if (is_rounded) {
          result += 1;
        }
        value = static_cast<std::uint64_t>(result >> 1u) & ones(esize_bits);
      }
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E207C00u) { // SABA/UABA (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > 32u || esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    auto dst = acc;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, esize_bits, lane);
      const std::uint64_t b = vector_get_elem(rhs, esize_bits, lane);
      const std::uint64_t absdiff = is_unsigned ? vector_unsigned_abs_diff(a, b)
                                                : vector_signed_abs_diff(a, b, esize_bits);
      const std::uint64_t value =
          (vector_get_elem(acc, esize_bits, lane) + absdiff) & ones(esize_bits);
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E207400u) { // SABD/UABD (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > 32u || esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, esize_bits, lane);
      const std::uint64_t b = vector_get_elem(rhs, esize_bits, lane);
      const std::uint64_t absdiff = is_unsigned ? vector_unsigned_abs_diff(a, b)
                                                : vector_signed_abs_diff(a, b, esize_bits);
      vector_set_elem(dst, esize_bits, lane, absdiff & ones(esize_bits));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E206400u || // SMAX/UMAX (vector)
      (insn & 0x9F20FC00u) == 0x0E206C00u) { // SMIN/UMIN (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool is_min = (insn & 0x00000800u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > 32u || esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, esize_bits, lane);
      const std::uint64_t b = vector_get_elem(rhs, esize_bits, lane);
      std::uint64_t value = 0u;
      if (is_unsigned) {
        value = is_min ? std::min(a, b) : std::max(a, b);
      } else {
        const std::int64_t sa = sign_extend(a, esize_bits);
        const std::int64_t sb = sign_extend(b, esize_bits);
        value = static_cast<std::uint64_t>(is_min ? std::min(sa, sb) : std::max(sa, sb)) &
                ones(esize_bits);
      }
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E20B400u) { // SQDMULH/SQRDMULH (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool round = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size == 0u || size == 3u) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    bool saturated = false;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      bool lane_saturated = false;
      const std::uint64_t value = sqdmulh_result(
          vector_get_elem(lhs, esize_bits, lane),
          vector_get_elem(rhs, esize_bits, lane),
          esize_bits,
          round,
          lane_saturated);
      saturated |= lane_saturated;
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E20D000u || // SQDMULL/SQDMULL2 (vector)
      (insn & 0x9F20FC00u) == 0x0E209000u || // SQDMLAL/SQDMLAL2 (vector)
      (insn & 0x9F20FC00u) == 0x0E20B000u) { // SQDMLSL/SQDMLSL2 (vector)
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t tag = insn & 0x9F20FC00u;
    const bool accumulate = tag != 0x0E20D000u;
    const bool subtract = tag == 0x0E20B000u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size == 0u || size == 3u) {
      return false;
    }
    const std::uint32_t src_esize_bits = 8u << size;
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t lanes = 64u / src_esize_bits;
    const std::uint32_t src_base = upper_half ? lanes : 0u;
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    bool saturated = false;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      bool lane_saturated = false;
      const std::uint64_t value = accumulate
          ? sqdml_long_accumulate_result(
                vector_get_elem(acc, dst_esize_bits, lane),
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                vector_get_elem(rhs, src_esize_bits, src_base + lane),
                src_esize_bits,
                subtract,
                lane_saturated)
          : sqdmull_product_result(
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                vector_get_elem(rhs, src_esize_bits, src_base + lane),
                src_esize_bits,
                lane_saturated);
      saturated |= lane_saturated;
      vector_set_elem(dst, dst_esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E20C000u || // SMULL/SMULL2 (vector)
      (insn & 0xBF20FC00u) == 0x0E208000u || // SMLAL/SMLAL2 (vector)
      (insn & 0xBF20FC00u) == 0x0E20A000u || // SMLSL/SMLSL2 (vector)
      (insn & 0xBF20FC00u) == 0x2E20C000u || // UMULL/UMULL2 (vector)
      (insn & 0xBF20FC00u) == 0x2E208000u || // UMLAL/UMLAL2 (vector)
      (insn & 0xBF20FC00u) == 0x2E20A000u) { // UMLSL/UMLSL2 (vector)
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t tag = insn & 0x9F20FC00u;
    const bool accumulate = tag != 0x0E20C000u;
    const bool subtract = tag == 0x0E20A000u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 8u << size;
    if (src_esize_bits > 32u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t lanes = 64u / src_esize_bits;
    const std::uint32_t src_base = upper_half ? lanes : 0u;
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t value = accumulate
          ? long_multiply_accumulate_result(
                vector_get_elem(acc, dst_esize_bits, lane),
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                vector_get_elem(rhs, src_esize_bits, src_base + lane),
                src_esize_bits,
                subtract,
                is_unsigned)
          : long_multiply_result(
                vector_get_elem(lhs, src_esize_bits, src_base + lane),
                vector_get_elem(rhs, src_esize_bits, src_base + lane),
                src_esize_bits,
                is_unsigned);
      vector_set_elem(dst, dst_esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x2E208400u) { // SUB (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t diff = (vector_get_elem(lhs, esize_bits, lane) - vector_get_elem(rhs, esize_bits, lane)) & ones(esize_bits);
      vector_set_elem(dst, esize_bits, lane, diff);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E209400u) { // MLA/MLS (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool subtract = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    if (size == 3u) {
      return false;
    }
    const std::uint32_t esize_bits = 8u << size;
    const std::uint64_t mask = ones(esize_bits);
    const std::uint32_t total_bits = q ? 128u : 64u;
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto acc = qregs_[rd];
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t prod =
          (vector_get_elem(lhs, esize_bits, lane) *
           vector_get_elem(rhs, esize_bits, lane)) &
          mask;
      const std::uint64_t value =
          ((subtract
                ? (vector_get_elem(acc, esize_bits, lane) - prod)
                : (vector_get_elem(acc, esize_bits, lane) + prod)) &
           mask);
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E209C00u) { // MUL (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    if (size == 3u) {
      return false;
    }
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t prod = (vector_get_elem(lhs, esize_bits, lane) * vector_get_elem(rhs, esize_bits, lane)) & ones(esize_bits);
      vector_set_elem(dst, esize_bits, lane, prod);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E003800u) { // ZIP1 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    if ((lanes & 1u) != 0u) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < lanes / 2u; ++i) {
      vector_set_elem(dst, esize_bits, i * 2u, vector_get_elem(lhs, esize_bits, i));
      vector_set_elem(dst, esize_bits, i * 2u + 1u, vector_get_elem(rhs, esize_bits, i));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E001800u) { // UZP1 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    if ((lanes & 1u) != 0u) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < lanes / 2u; ++i) {
      vector_set_elem(dst, esize_bits, i, vector_get_elem(lhs, esize_bits, i * 2u));
      vector_set_elem(dst, esize_bits, i + (lanes / 2u), vector_get_elem(rhs, esize_bits, i * 2u));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E002800u) { // TRN1 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    if ((lanes & 1u) != 0u) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < lanes / 2u; ++i) {
      vector_set_elem(dst, esize_bits, i * 2u, vector_get_elem(lhs, esize_bits, i * 2u));
      vector_set_elem(dst, esize_bits, i * 2u + 1u, vector_get_elem(rhs, esize_bits, i * 2u));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E007800u) { // ZIP2 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    if ((lanes & 1u) != 0u) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < lanes / 2u; ++i) {
      vector_set_elem(dst, esize_bits, i * 2u, vector_get_elem(lhs, esize_bits, i + lanes / 2u));
      vector_set_elem(dst, esize_bits, i * 2u + 1u, vector_get_elem(rhs, esize_bits, i + lanes / 2u));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E005800u) { // UZP2 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    if ((lanes & 1u) != 0u) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < lanes / 2u; ++i) {
      vector_set_elem(dst, esize_bits, i, vector_get_elem(lhs, esize_bits, i * 2u + 1u));
      vector_set_elem(dst, esize_bits, i + (lanes / 2u), vector_get_elem(rhs, esize_bits, i * 2u + 1u));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E006800u) { // TRN2 (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    if ((lanes & 1u) != 0u) {
      return false;
    }
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < lanes / 2u; ++i) {
      vector_set_elem(dst, esize_bits, i * 2u, vector_get_elem(lhs, esize_bits, i * 2u + 1u));
      vector_set_elem(dst, esize_bits, i * 2u + 1u, vector_get_elem(rhs, esize_bits, i * 2u + 1u));
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF3FFC00u) == 0x0E31B800u || // ADDV
      (insn & 0xBF3FFC00u) == 0x0E303800u || // SADDLV
      (insn & 0xBF3FFC00u) == 0x2E303800u || // UADDLV
      (insn & 0xBF3FFC00u) == 0x0E30A800u || // SMAXV
      (insn & 0xBF3FFC00u) == 0x0E31A800u || // SMINV
      (insn & 0xBF3FFC00u) == 0x2E30A800u || // UMAXV
      (insn & 0xBF3FFC00u) == 0x2E31A800u) { // UMINV
    const std::uint32_t tag = insn & 0xBF3FFC00u;
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size == 3u || (size == 2u && !q)) {
      return false;
    }
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t datasize = q ? 128u : 64u;
    const std::uint32_t lanes = datasize / esize_bits;
    const auto src = qregs_[rn];
    switch (tag) {
      case 0x0E31B800u: { // ADDV
        std::uint64_t sum = 0u;
        for (std::uint32_t lane = 0; lane < lanes; ++lane) {
          sum = (sum + vector_get_elem(src, esize_bits, lane)) & ones(esize_bits);
        }
        write_vec_scalar(rd, sum, esize_bits);
        return true;
      }
      case 0x0E303800u:
      case 0x2E303800u: { // SADDLV/UADDLV
        const bool is_unsigned = tag == 0x2E303800u;
        const std::uint32_t dst_esize_bits = esize_bits * 2u;
        if (is_unsigned) {
          std::uint64_t sum = 0u;
          for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            sum += vector_get_elem(src, esize_bits, lane);
          }
          write_vec_scalar(rd, sum, dst_esize_bits);
        } else {
          std::int64_t sum = 0;
          for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            sum += sign_extend(vector_get_elem(src, esize_bits, lane), esize_bits);
          }
          write_vec_scalar(rd, static_cast<std::uint64_t>(sum), dst_esize_bits);
        }
        return true;
      }
      case 0x0E30A800u:
      case 0x0E31A800u: { // SMAXV/SMINV
        std::int64_t best = sign_extend(vector_get_elem(src, esize_bits, 0u), esize_bits);
        for (std::uint32_t lane = 1u; lane < lanes; ++lane) {
          const std::int64_t value = sign_extend(vector_get_elem(src, esize_bits, lane), esize_bits);
          if ((tag == 0x0E30A800u && value > best) || (tag == 0x0E31A800u && value < best)) {
            best = value;
          }
        }
        write_vec_scalar(rd, static_cast<std::uint64_t>(best), esize_bits);
        return true;
      }
      case 0x2E30A800u:
      case 0x2E31A800u: { // UMAXV/UMINV
        std::uint64_t best = vector_get_elem(src, esize_bits, 0u);
        for (std::uint32_t lane = 1u; lane < lanes; ++lane) {
          const std::uint64_t value = vector_get_elem(src, esize_bits, lane);
          if ((tag == 0x2E30A800u && value > best) || (tag == 0x2E31A800u && value < best)) {
            best = value;
          }
        }
        write_vec_scalar(rd, best, esize_bits);
        return true;
      }
      default:
        return false;
    }
  }

  if ((insn & 0x9F3FFC00u) == 0x0E202800u) { // SADDLP/UADDLP
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 8u << size;
    if (src_esize_bits > 32u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t src_lanes = (q ? 128u : 64u) / src_esize_bits;
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {0u, 0u};
    for (std::uint32_t lane = 0; lane < src_lanes / 2u; ++lane) {
      const std::uint64_t a = vector_get_elem(src, src_esize_bits, lane * 2u);
      const std::uint64_t b = vector_get_elem(src, src_esize_bits, lane * 2u + 1u);
      std::uint64_t result = 0u;
      if (is_unsigned) {
        result = (a + b) & ones(dst_esize_bits);
      } else {
        const std::int64_t sum = sign_extend(a, src_esize_bits) + sign_extend(b, src_esize_bits);
        switch (dst_esize_bits) {
          case 16u: result = static_cast<std::uint64_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(sum))); break;
          case 32u: result = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(sum))); break;
          case 64u: result = static_cast<std::uint64_t>(static_cast<std::int64_t>(sum)); break;
          default: return false;
        }
      }
      vector_set_elem(dst, dst_esize_bits, lane, result);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F3FFC00u) == 0x0E206800u) { // SADALP/UADALP
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 8u << size;
    if (src_esize_bits > 32u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t src_lanes = (q ? 128u : 64u) / src_esize_bits;
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {qregs_[rd][0], q ? qregs_[rd][1] : 0u};
    for (std::uint32_t lane = 0; lane < src_lanes / 2u; ++lane) {
      const std::uint64_t a = vector_get_elem(src, src_esize_bits, lane * 2u);
      const std::uint64_t b = vector_get_elem(src, src_esize_bits, lane * 2u + 1u);
      const std::uint64_t sum = is_unsigned
                                    ? ((a + b) & ones(dst_esize_bits))
                                    : (static_cast<std::uint64_t>(
                                          sign_extend(a, src_esize_bits) + sign_extend(b, src_esize_bits)) &
                                       ones(dst_esize_bits));
      const std::uint64_t accum = (vector_get_elem(dst, dst_esize_bits, lane) + sum) & ones(dst_esize_bits);
      vector_set_elem(dst, dst_esize_bits, lane, accum);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E20A400u || // SMAXP
      (insn & 0xBF20FC00u) == 0x0E20AC00u || // SMINP
      (insn & 0xBF20FC00u) == 0x2E20A400u || // UMAXP
      (insn & 0xBF20FC00u) == 0x2E20AC00u || // UMINP
      (insn & 0xBF20FC00u) == 0x0E20BC00u) { // ADDP (vector)
    const std::uint32_t tag = insn & 0xBF20FC00u;
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size < 3u) {
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      const std::uint32_t esize_bits = 8u << size;
      const std::uint32_t lanes = (q ? 128u : 64u) / esize_bits;
      const auto lhs = qregs_[rn];
      const auto rhs = qregs_[rm];
      std::array<std::uint64_t, 2> dst = {0, 0};
      const auto do_pairwise = [&](const std::array<std::uint64_t, 2>& src, std::uint32_t dst_base) {
        for (std::uint32_t lane = 0; lane < lanes / 2u; ++lane) {
          const std::uint64_t a = vector_get_elem(src, esize_bits, lane * 2u);
          const std::uint64_t b = vector_get_elem(src, esize_bits, lane * 2u + 1u);
          std::uint64_t result = 0u;
          switch (tag) {
            case 0x0E20A400u:
              result = static_cast<std::uint64_t>(
                  std::max(sign_extend(a, esize_bits), sign_extend(b, esize_bits))) & ones(esize_bits);
              break;
            case 0x0E20AC00u:
              result = static_cast<std::uint64_t>(
                  std::min(sign_extend(a, esize_bits), sign_extend(b, esize_bits))) & ones(esize_bits);
              break;
            case 0x2E20A400u:
              result = std::max(a, b);
              break;
            case 0x2E20AC00u:
              result = std::min(a, b);
              break;
            case 0x0E20BC00u:
              result = (a + b) & ones(esize_bits);
              break;
            default:
              return false;
          }
          vector_set_elem(dst, esize_bits, dst_base + lane, result);
        }
        return true;
      };
      if (!do_pairwise(lhs, 0u) || !do_pairwise(rhs, lanes / 2u)) {
        return false;
      }
      qregs_[rd] = dst;
      return true;
    }
  }


  {
    const std::uint32_t tag = insn & 0xBF80FC00u;
    if (tag == 0x0F008400u || // SHRN/SHRN2
        tag == 0x0F008C00u || // RSHRN/RSHRN2
        tag == 0x0F009400u || // SQSHRN/SQSHRN2
        tag == 0x2F009400u || // UQSHRN/UQSHRN2
        tag == 0x0F009C00u || // SQRSHRN/SQRSHRN2
        tag == 0x2F009C00u || // UQRSHRN/UQRSHRN2
        tag == 0x2F008400u || // SQSHRUN/SQSHRUN2
        tag == 0x2F008C00u) { // SQRSHRUN/SQRSHRUN2
      const bool upper_half = ((insn >> 30) & 1u) != 0u;
      const std::uint32_t immh = (insn >> 19) & 0xFu;
      const std::uint32_t immb = (insn >> 16) & 0x7u;
      if (immh == 0u) {
        return false;
      }
      const std::uint32_t src_esize_bits = 16u << highest_set_bit(immh);
      if (src_esize_bits < 16u || src_esize_bits > 64u) {
        return false;
      }
      const std::uint32_t immhb = (immh << 3u) | immb;
      const std::uint32_t shift = src_esize_bits - immhb;
      if (shift == 0u || shift > src_esize_bits / 2u) {
        return false;
      }
      const std::uint32_t dst_esize_bits = src_esize_bits / 2u;
      const std::uint32_t lanes = 64u / dst_esize_bits;
      const bool round = tag == 0x0F008C00u || tag == 0x0F009C00u ||
                         tag == 0x2F009C00u || tag == 0x2F008C00u;
      ShiftRightNarrowMode mode = ShiftRightNarrowMode::PlainUnsigned;
      switch (tag) {
        case 0x0F008400u:
        case 0x0F008C00u:
          mode = ShiftRightNarrowMode::PlainUnsigned;
          break;
        case 0x0F009400u:
        case 0x0F009C00u:
          mode = ShiftRightNarrowMode::SignedToSignedSaturating;
          break;
        case 0x2F009400u:
        case 0x2F009C00u:
          mode = ShiftRightNarrowMode::UnsignedToUnsignedSaturating;
          break;
        case 0x2F008400u:
        case 0x2F008C00u:
          mode = ShiftRightNarrowMode::SignedToUnsignedSaturating;
          break;
        default:
          return false;
      }
      const auto src = qregs_[rn];
      auto dst = upper_half ? qregs_[rd] : std::array<std::uint64_t, 2>{0u, 0u};
      const std::uint32_t dst_base = upper_half ? lanes : 0u;
      bool saturated = false;
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        bool lane_saturated = false;
        const std::uint64_t narrowed = shift_right_narrow_result(
            vector_get_elem(src, src_esize_bits, lane),
            src_esize_bits,
            dst_esize_bits,
            shift,
            round,
            mode,
            lane_saturated);
        saturated |= lane_saturated;
        vector_set_elem(dst, dst_esize_bits, dst_base + lane, narrowed);
      }
      qregs_[rd] = dst;
      if (saturated) {
        sysregs_.fp_or_fpsr(kFpsrQc);
      }
      return true;
    }
  }

  {
    const std::uint32_t tag = insn & 0xBF20FC00u;
    if (tag == 0x0E204000u || tag == 0x2E204000u ||
        tag == 0x0E206000u || tag == 0x2E206000u) { // ADDHN/RADDHN/SUBHN/RSUBHN and *2 variants
      const bool upper_half = ((insn >> 30) & 1u) != 0u;
      const bool is_rounded = ((insn >> 29) & 1u) != 0u;
      const bool is_subtract = (insn & 0x00002000u) != 0u;
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      const std::uint32_t size = (insn >> 22) & 0x3u;
      const std::uint32_t src_esize_bits = 16u << size;
      if (src_esize_bits < 16u || src_esize_bits > 64u) {
        return false;
      }
      const std::uint32_t dst_esize_bits = src_esize_bits / 2u;
      const std::uint32_t lanes = 64u / dst_esize_bits;
      const std::uint64_t src_mask = ones(src_esize_bits);
      const std::uint64_t dst_mask = ones(dst_esize_bits);
      const std::uint64_t rounding = is_rounded ? (1ull << (dst_esize_bits - 1u)) : 0u;
      const auto lhs = qregs_[rn];
      const auto rhs = qregs_[rm];
      auto dst = upper_half ? qregs_[rd] : std::array<std::uint64_t, 2>{0, 0};
      const std::uint32_t dst_base = upper_half ? lanes : 0u;
      for (std::uint32_t lane = 0; lane < lanes; ++lane) {
        std::uint64_t value = 0;
        const std::uint64_t a = vector_get_elem(lhs, src_esize_bits, lane);
        const std::uint64_t b = vector_get_elem(rhs, src_esize_bits, lane);
        if (is_subtract) {
          value = a - b;
        } else {
          value = a + b;
        }
        value = (value + rounding) & src_mask;
        vector_set_elem(dst, dst_esize_bits, dst_base + lane, (value >> dst_esize_bits) & dst_mask);
      }
      qregs_[rd] = dst;
      return true;
    }
  }

  if ((insn & 0xBF3FFC00u) == 0x0E212800u) { // XTN/XTN2
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 16u << size;
    if (src_esize_bits < 16u || src_esize_bits > 64u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits / 2u;
    const std::uint32_t lanes = 64u / dst_esize_bits;
    const auto src = qregs_[rn];
    auto dst = upper_half ? qregs_[rd] : std::array<std::uint64_t, 2>{0, 0};
    const std::uint32_t dst_base = upper_half ? lanes : 0u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t narrowed = vector_get_elem(src, src_esize_bits, lane) & ones(dst_esize_bits);
      vector_set_elem(dst, dst_esize_bits, dst_base + lane, narrowed);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F3FFC00u) == 0x0E214800u) { // SQXTN/UQXTN and *2 variants
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 16u << size;
    if (src_esize_bits < 16u || src_esize_bits > 64u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits / 2u;
    const std::uint32_t lanes = 64u / dst_esize_bits;
    const auto src = qregs_[rn];
    auto dst = upper_half ? qregs_[rd] : std::array<std::uint64_t, 2>{0, 0};
    const std::uint32_t dst_base = upper_half ? lanes : 0u;
    bool saturated = false;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      std::uint64_t narrowed = 0;
      const std::uint64_t value = vector_get_elem(src, src_esize_bits, lane);
      if (is_unsigned) {
        switch (dst_esize_bits) {
          case 8u:
            saturated |= value > std::numeric_limits<std::uint8_t>::max();
            narrowed = saturate_unsigned_value<std::uint8_t>(value);
            break;
          case 16u:
            saturated |= value > std::numeric_limits<std::uint16_t>::max();
            narrowed = saturate_unsigned_value<std::uint16_t>(value);
            break;
          case 32u:
            saturated |= value > std::numeric_limits<std::uint32_t>::max();
            narrowed = saturate_unsigned_value<std::uint32_t>(value);
            break;
          default: return false;
        }
      } else {
        const std::int64_t signed_value = sign_extend(value, src_esize_bits);
        switch (dst_esize_bits) {
          case 8u:
            saturated |= signed_value < std::numeric_limits<std::int8_t>::min() ||
                         signed_value > std::numeric_limits<std::int8_t>::max();
            narrowed = static_cast<std::uint64_t>(static_cast<std::uint8_t>(saturate_signed_value<std::int8_t>(signed_value)));
            break;
          case 16u:
            saturated |= signed_value < std::numeric_limits<std::int16_t>::min() ||
                         signed_value > std::numeric_limits<std::int16_t>::max();
            narrowed = static_cast<std::uint64_t>(static_cast<std::uint16_t>(saturate_signed_value<std::int16_t>(signed_value)));
            break;
          case 32u:
            saturated |= signed_value < std::numeric_limits<std::int32_t>::min() ||
                         signed_value > std::numeric_limits<std::int32_t>::max();
            narrowed = static_cast<std::uint64_t>(static_cast<std::uint32_t>(saturate_signed_value<std::int32_t>(signed_value)));
            break;
          default: return false;
        }
      }
      vector_set_elem(dst, dst_esize_bits, dst_base + lane, narrowed);
    }
    qregs_[rd] = dst;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E200000u || // SADDL/UADDL and *2 variants
      (insn & 0x9F20FC00u) == 0x0E202000u) { // SSUBL/USUBL and *2 variants
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool is_subtract = (insn & 0x00002000u) != 0u;
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 8u << size;
    if (src_esize_bits > 32u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t lanes = 64u / src_esize_bits;
    const std::uint32_t src_base = upper_half ? lanes : 0u;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      std::uint64_t result = 0;
      const std::uint64_t a = vector_get_elem(lhs, src_esize_bits, src_base + lane);
      const std::uint64_t b = vector_get_elem(rhs, src_esize_bits, src_base + lane);
      if (is_unsigned) {
        result = is_subtract ? ((a - b) & ones(dst_esize_bits))
                             : ((a + b) & ones(dst_esize_bits));
      } else {
        const __int128_t lhs_value = static_cast<__int128_t>(sign_extend_bits(a, src_esize_bits));
        const __int128_t rhs_value = static_cast<__int128_t>(sign_extend_bits(b, src_esize_bits));
        const __int128_t wide_result = is_subtract ? (lhs_value - rhs_value) : (lhs_value + rhs_value);
        switch (dst_esize_bits) {
          case 16u:
            result = static_cast<std::uint64_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(wide_result)));
            break;
          case 32u:
            result = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(wide_result)));
            break;
          case 64u:
            result = static_cast<std::uint64_t>(static_cast<std::int64_t>(wide_result));
            break;
          default: return false;
        }
      }
      vector_set_elem(dst, dst_esize_bits, lane, result);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E205000u || // SABAL/UABAL and *2 variants
      (insn & 0x9F20FC00u) == 0x0E207000u) { // SABDL/UABDL and *2 variants
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool is_accumulate = (insn & 0x00002000u) == 0u;
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 8u << size;
    if (src_esize_bits > 32u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t lanes = 64u / src_esize_bits;
    const std::uint32_t src_base = upper_half ? lanes : 0u;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    auto dst = is_accumulate ? qregs_[rd] : std::array<std::uint64_t, 2>{0u, 0u};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, src_esize_bits, src_base + lane);
      const std::uint64_t b = vector_get_elem(rhs, src_esize_bits, src_base + lane);
      const std::uint64_t absdiff = is_unsigned ? vector_unsigned_abs_diff(a, b)
                                                : vector_signed_abs_diff(a, b, src_esize_bits);
      const std::uint64_t accum = is_accumulate ? vector_get_elem(dst, dst_esize_bits, lane) : 0u;
      const std::uint64_t value = (accum + absdiff) & ones(dst_esize_bits);
      vector_set_elem(dst, dst_esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E201000u || // SADDW/UADDW and *2 variants
      (insn & 0x9F20FC00u) == 0x0E203000u) { // SSUBW/USUBW and *2 variants
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool is_subtract = (insn & 0x00002000u) != 0u;
    const bool upper_half = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t src_esize_bits = 8u << size;
    if (src_esize_bits > 32u) {
      return false;
    }
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    const std::uint32_t lanes = 64u / src_esize_bits;
    const std::uint32_t src_base = upper_half ? lanes : 0u;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t wide = vector_get_elem(lhs, dst_esize_bits, lane);
      const std::uint64_t narrow = vector_get_elem(rhs, src_esize_bits, src_base + lane);
      std::uint64_t result = 0;
      if (is_unsigned) {
        result = is_subtract ? ((wide - narrow) & ones(dst_esize_bits))
                             : ((wide + narrow) & ones(dst_esize_bits));
      } else {
        const std::uint64_t wide_signed = wide & ones(dst_esize_bits);
        const std::uint64_t narrow_signed =
            static_cast<std::uint64_t>(sign_extend_bits(narrow, src_esize_bits)) & ones(dst_esize_bits);
        result = is_subtract ? ((wide_signed - narrow_signed) & ones(dst_esize_bits))
                             : ((wide_signed + narrow_signed) & ones(dst_esize_bits));
      }
      vector_set_elem(dst, dst_esize_bits, lane, result);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E200C00u || // SQADD/UQADD (vector)
      (insn & 0x9F20FC00u) == 0x0E202C00u) { // SQSUB/UQSUB (vector)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool is_subtract = (insn & 0x00002000u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t esize_bits = 8u << size;
    const std::uint32_t total_bits = q ? 128u : 64u;
    if (esize_bits > total_bits) {
      return false;
    }
    const std::uint32_t lanes = total_bits / esize_bits;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    bool saturated = false;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, esize_bits, lane);
      const std::uint64_t b = vector_get_elem(rhs, esize_bits, lane);
      bool lane_saturated = false;
      const std::uint64_t value = is_unsigned
          ? sat_unsigned_add_sub(a, b, esize_bits, is_subtract, lane_saturated)
          : sat_signed_add_sub(a, b, esize_bits, is_subtract, lane_saturated);
      saturated |= lane_saturated;
      vector_set_elem(dst, esize_bits, lane, value);
    }
    qregs_[rd] = dst;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }


  if ((insn & 0xDF20FC00u) == 0x5E200C00u || // SQADD/UQADD (scalar)
      (insn & 0xDF20FC00u) == 0x5E202C00u) { // SQSUB/UQSUB (scalar)
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const bool is_subtract = (insn & 0x00002000u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t esize_bits = 8u << size;
    bool saturated = false;
    const std::uint64_t lhs = qregs_[rn][0] & ones(esize_bits);
    const std::uint64_t rhs = qregs_[rm][0] & ones(esize_bits);
    const std::uint64_t value = is_unsigned
        ? sat_unsigned_add_sub(lhs, rhs, esize_bits, is_subtract, saturated)
        : sat_signed_add_sub(lhs, rhs, esize_bits, is_subtract, saturated);
    qregs_[rd][0] = value & ones(esize_bits);
    qregs_[rd][1] = 0u;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0xDF20FC00u) == 0x5E20B400u) { // SQDMULH/SQRDMULH (scalar)
    const bool round = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size == 0u || size == 3u) {
      return false;
    }
    const std::uint32_t esize_bits = 8u << size;
    bool saturated = false;
    const std::uint64_t value = sqdmulh_result(
        qregs_[rn][0] & ones(esize_bits),
        qregs_[rm][0] & ones(esize_bits),
        esize_bits,
        round,
        saturated);
    qregs_[rd][0] = value & ones(esize_bits);
    qregs_[rd][1] = 0u;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0xDF20FC00u) == 0x5E20D000u || // SQDMULL (scalar)
      (insn & 0xDF20FC00u) == 0x5E209000u || // SQDMLAL (scalar)
      (insn & 0xDF20FC00u) == 0x5E20B000u) { // SQDMLSL (scalar)
    const std::uint32_t tag = insn & 0xDF20FC00u;
    const bool accumulate = tag != 0x5E20D000u;
    const bool subtract = tag == 0x5E20B000u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size == 0u || size == 3u) {
      return false;
    }
    const std::uint32_t src_esize_bits = 8u << size;
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    bool saturated = false;
    const std::uint64_t value = accumulate
        ? sqdml_long_accumulate_result(
              qregs_[rd][0] & ones(dst_esize_bits),
              qregs_[rn][0] & ones(src_esize_bits),
              qregs_[rm][0] & ones(src_esize_bits),
              src_esize_bits,
              subtract,
              saturated)
        : sqdmull_product_result(
              qregs_[rn][0] & ones(src_esize_bits),
              qregs_[rm][0] & ones(src_esize_bits),
              src_esize_bits,
              saturated);
    qregs_[rd][0] = value & ones(dst_esize_bits);
    qregs_[rd][1] = 0u;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }



  switch (insn & 0xFF3FFC00u) {
    case 0x1E21C000u: { // FSQRT Sd/Dd, Sn/Dn
      const std::uint32_t rn_fp = (insn >> 5) & 0x1Fu;
      const std::uint32_t ftype = (insn >> 22) & 0x3u;
      std::uint64_t fpsr_bits = 0u;
      if (ftype == 0u) {
        const std::uint32_t result = fp_sqrt_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(qregs_[rn_fp][0]),
            current_fpcr_ctl(),
            fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 1u) {
        const std::uint64_t result = fp_sqrt_bits<std::uint64_t, double>(qregs_[rn_fp][0], current_fpcr_ctl(), fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      return false;
    }
    case 0x5E21F800u: { // FRECPX Sd/Dd, Sn/Dn
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t ftype = (insn >> 22) & 0x3u;
      if (ftype == 2u) {
        const std::uint32_t result = fp_recip_exponent_bits<std::uint32_t>(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            current_fpcr_ctl(),
            fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 3u) {
        const std::uint64_t result = fp_recip_exponent_bits<std::uint64_t>(qregs_[rn][0], current_fpcr_ctl(), fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      return false;
    }
  }

  switch (insn & 0xFFE0FC00u) {
    case 0x5E20FC00u: // FRECPS Sd, Sn, Sm
    case 0x5E60FC00u: // FRECPS Dd, Dn, Dm
    case 0x5EA0FC00u: // FRSQRTS Sd, Sn, Sm
    case 0x5EE0FC00u: { // FRSQRTS Dd, Dn, Dm
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      const std::uint32_t opcode = insn & 0xFFE0FC00u;
      const bool is_double = opcode == 0x5E60FC00u || opcode == 0x5EE0FC00u;
      const bool is_rsqrt = opcode == 0x5EA0FC00u || opcode == 0x5EE0FC00u;
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      std::uint64_t fpsr_bits = 0u;
      if (!is_double) {
        const std::uint32_t lhs = static_cast<std::uint32_t>(qregs_[rn][0]);
        const std::uint32_t rhs = static_cast<std::uint32_t>(qregs_[rm][0]);
        const std::uint32_t result = is_rsqrt
            ? fp_rsqrt_step_fused_bits<std::uint32_t, float>(lhs, rhs, fpcr_mode, fpsr_bits)
            : fp_recip_step_fused_bits<std::uint32_t, float>(lhs, rhs, fpcr_mode, fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      const std::uint64_t lhs = qregs_[rn][0];
      const std::uint64_t rhs = qregs_[rm][0];
      const std::uint64_t result = is_rsqrt
          ? fp_rsqrt_step_fused_bits<std::uint64_t, double>(lhs, rhs, fpcr_mode, fpsr_bits)
          : fp_recip_step_fused_bits<std::uint64_t, double>(lhs, rhs, fpcr_mode, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
  }

  switch (insn & 0xFF3FFC00u) {
    case 0x5E21D800u: {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t ftype = (insn >> 22) & 0x3u;
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      if (ftype == 0u) { // SCVTF Sd, Sn
        const std::uint32_t result = fp_int_to_fp_bits<std::uint32_t>(
            static_cast<std::int32_t>(static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu)),
            fpcr_mode,
            fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 1u) { // SCVTF Dd, Dn
        const std::uint64_t result =
            fp_int_to_fp_bits<std::uint64_t>(static_cast<std::int64_t>(qregs_[rn][0]), fpcr_mode, fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 2u) { // FRECPE Sd, Sn
        const std::uint32_t result = fp_recip_estimate_bits<std::uint32_t>(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            fpcr_mode,
            fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 3u) { // FRECPE Dd, Dn
        const std::uint64_t result = fp_recip_estimate_bits<std::uint64_t>(qregs_[rn][0], fpcr_mode, fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      return false;
    }
    case 0x7E21D800u: {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t ftype = (insn >> 22) & 0x3u;
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      if (ftype == 0u) { // UCVTF Sd, Sn
        const std::uint32_t result = fp_int_to_fp_bits<std::uint32_t>(
            static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
            fpcr_mode,
            fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 1u) { // UCVTF Dd, Dn
        const std::uint64_t result = fp_int_to_fp_bits<std::uint64_t>(qregs_[rn][0], fpcr_mode, fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 2u) { // FRSQRTE Sd, Sn
        const std::uint32_t result = fp_rsqrt_estimate_bits<std::uint32_t>(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            fpcr_mode,
            fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      if (ftype == 3u) { // FRSQRTE Dd, Dn
        const std::uint64_t result = fp_rsqrt_estimate_bits<std::uint64_t>(qregs_[rn][0], fpcr_mode, fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        qregs_[rd][0] = result;
        qregs_[rd][1] = 0u;
        return true;
      }
      return false;
    }
  }

  switch (insn & 0xFF3FFC00u) {
    case 0x1E244000u: // FRINTN
      if (((insn >> 22) & 0x3u) == 0u) {
        const auto rounded = fp_round_int_bits(static_cast<std::uint32_t>(qregs_[rn][0]), fp_round_ties_even, false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else if (((insn >> 22) & 0x3u) == 1u) {
        const auto rounded = fp_round_int_bits(qregs_[rn][0], fp_round_ties_even, false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else {
        return false;
      }
      return true;
    case 0x1E24C000u: // FRINTP
      if (((insn >> 22) & 0x3u) == 0u) {
        const auto rounded = fp_round_int_bits(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            static_cast<float (*)(float)>(std::ceil),
            false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else if (((insn >> 22) & 0x3u) == 1u) {
        const auto rounded = fp_round_int_bits(qregs_[rn][0], static_cast<double (*)(double)>(std::ceil), false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else {
        return false;
      }
      return true;
    case 0x1E254000u: // FRINTM
      if (((insn >> 22) & 0x3u) == 0u) {
        const auto rounded = fp_round_int_bits(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            static_cast<float (*)(float)>(std::floor),
            false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else if (((insn >> 22) & 0x3u) == 1u) {
        const auto rounded = fp_round_int_bits(qregs_[rn][0], static_cast<double (*)(double)>(std::floor), false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else {
        return false;
      }
      return true;
    case 0x1E25C000u: // FRINTZ
      if (((insn >> 22) & 0x3u) == 0u) {
        const auto rounded = fp_round_int_bits(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            static_cast<float (*)(float)>(std::trunc),
            false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else if (((insn >> 22) & 0x3u) == 1u) {
        const auto rounded = fp_round_int_bits(qregs_[rn][0], static_cast<double (*)(double)>(std::trunc), false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else {
        return false;
      }
      return true;
    case 0x1E264000u: // FRINTA
      if (((insn >> 22) & 0x3u) == 0u) {
        const auto rounded = fp_round_int_bits(static_cast<std::uint32_t>(qregs_[rn][0]), fp_round_ties_away, false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else if (((insn >> 22) & 0x3u) == 1u) {
        const auto rounded = fp_round_int_bits(qregs_[rn][0], fp_round_ties_away, false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else {
        return false;
      }
      return true;
    case 0x1E274000u: { // FRINTX
      const std::uint32_t mode = current_fpcr_ctl();
      if (((insn >> 22) & 0x3u) == 0u) {
        const auto rounded = fp_round_int_bits(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            [&](float v) { return fp_round_by_mode(v, mode); },
            true);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else if (((insn >> 22) & 0x3u) == 1u) {
        const auto rounded = fp_round_int_bits(
            qregs_[rn][0],
            [&](double v) { return fp_round_by_mode(v, mode); },
            true);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else {
        return false;
      }
      return true;
    }
    case 0x1E27C000u: { // FRINTI
      const std::uint32_t mode = current_fpcr_ctl();
      if (((insn >> 22) & 0x3u) == 0u) {
        const auto rounded = fp_round_int_bits(
            static_cast<std::uint32_t>(qregs_[rn][0]),
            [&](float v) { return fp_round_by_mode(v, mode); },
            false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else if (((insn >> 22) & 0x3u) == 1u) {
        const auto rounded = fp_round_int_bits(
            qregs_[rn][0],
            [&](double v) { return fp_round_by_mode(v, mode); },
            false);
        sysregs_.fp_or_fpsr(rounded.second);
        qregs_[rd][0] = rounded.first;
        qregs_[rd][1] = 0u;
      } else {
        return false;
      }
      return true;
    }
  }

  if ((insn & 0xFF201FE0u) == 0x1E201000u) { // FMOV Sd/Dd, #imm
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint8_t imm8 = static_cast<std::uint8_t>((insn >> 13) & 0xFFu);
    if (ftype == 0u) {
      qregs_[rd][0] = vfp_expand_imm(false, imm8);
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      qregs_[rd][0] = vfp_expand_imm(true, imm8);
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E204000u) { // FMOV Sd, Sn
    const std::uint32_t rn_fp = (insn >> 5) & 0x1Fu;
    qregs_[rd][0] = qregs_[rn_fp][0] & 0xFFFFFFFFu;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E604000u) { // FMOV Dd, Dn
    const std::uint32_t rn_fp = (insn >> 5) & 0x1Fu;
    qregs_[rd][0] = qregs_[rn_fp][0];
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFF20FC00u) == 0x1E200800u) { // FMUL (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          HostFpBinaryOp::Mul,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          fpcr_mode,
          HostFpBinaryOp::Mul,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF20FC00u) == 0x5E20DC00u) { // FMULX (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_mulx_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint64_t result = fp_mulx_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          fpcr_mode,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF80F400u) == 0x5F809000u) { // FMUL (by element, scalar)
    const std::uint32_t sz = (insn >> 22) & 1u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    if (sz != 0u && l != 0u) {
      return false;
    }
    const std::uint32_t rm = ((insn >> 16) & 0xFu) | (((insn >> 20) & 1u) << 4u);
    const std::uint32_t index = sz == 0u ? ((h << 1u) | l) : h;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (sz == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(vector_get_elem(qregs_[rm], 32u, index)),
          fpcr_mode,
          HostFpBinaryOp::Mul,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
        qregs_[rn][0],
        vector_get_elem(qregs_[rm], 64u, index),
        fpcr_mode,
        HostFpBinaryOp::Mul,
        fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFF80F400u) == 0x7F809000u) { // FMULX (by element, scalar)
    const std::uint32_t sz = (insn >> 22) & 1u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    if (sz != 0u && l != 0u) {
      return false;
    }
    const std::uint32_t rm = ((insn >> 16) & 0xFu) | (((insn >> 20) & 1u) << 4u);
    const std::uint32_t index = sz == 0u ? ((h << 1u) | l) : h;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (sz == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_mulx_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(vector_get_elem(qregs_[rm], 32u, index)),
          fpcr_mode,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    const std::uint64_t result = fp_mulx_bits<std::uint64_t, double>(
        qregs_[rn][0],
        vector_get_elem(qregs_[rm], 64u, index),
        fpcr_mode,
        fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFF00F400u) == 0x5F00C000u || // SQDMULH (by element, scalar)
      (insn & 0xFF00F400u) == 0x5F00D000u) { // SQRDMULH (by element, scalar)
    const bool round = (insn & 0x00001000u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t m = (insn >> 20) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    std::uint32_t index = 0u;
    std::uint32_t rm = 0u;
    switch (size) {
      case 1u:
        index = (h << 2u) | (l << 1u) | m;
        rm = (insn >> 16) & 0xFu;
        break;
      case 2u:
        index = (h << 1u) | l;
        rm = ((insn >> 16) & 0xFu) | (m << 4u);
        break;
      default:
        return false;
    }
    const std::uint32_t esize_bits = 8u << size;
    bool saturated = false;
    const std::uint64_t value = sqdmulh_result(
        qregs_[rn][0] & ones(esize_bits),
        vector_get_elem(qregs_[rm], esize_bits, index),
        esize_bits,
        round,
        saturated);
    qregs_[rd][0] = value & ones(esize_bits);
    qregs_[rd][1] = 0u;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0xDF00F400u) == 0x5F00B000u || // SQDMULL (by element, scalar)
      (insn & 0xDF00F400u) == 0x5F003000u || // SQDMLAL (by element, scalar)
      (insn & 0xDF00F400u) == 0x5F007000u) { // SQDMLSL (by element, scalar)
    const std::uint32_t tag = insn & 0xDF00F400u;
    const bool accumulate = tag != 0x5F00B000u;
    const bool subtract = tag == 0x5F007000u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t m = (insn >> 20) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    std::uint32_t index = 0u;
    std::uint32_t rm = 0u;
    switch (size) {
      case 1u:
        index = (h << 2u) | (l << 1u) | m;
        rm = (insn >> 16) & 0xFu;
        break;
      case 2u:
        index = (h << 1u) | l;
        rm = ((insn >> 16) & 0xFu) | (m << 4u);
        break;
      default:
        return false;
    }
    const std::uint32_t src_esize_bits = 8u << size;
    const std::uint32_t dst_esize_bits = src_esize_bits * 2u;
    bool saturated = false;
    const std::uint64_t value = accumulate
        ? sqdml_long_accumulate_result(
              qregs_[rd][0] & ones(dst_esize_bits),
              qregs_[rn][0] & ones(src_esize_bits),
              vector_get_elem(qregs_[rm], src_esize_bits, index),
              src_esize_bits,
              subtract,
              saturated)
        : sqdmull_product_result(
              qregs_[rn][0] & ones(src_esize_bits),
              vector_get_elem(qregs_[rm], src_esize_bits, index),
              src_esize_bits,
              saturated);
    qregs_[rd][0] = value & ones(dst_esize_bits);
    qregs_[rd][1] = 0u;
    if (saturated) {
      sysregs_.fp_or_fpsr(kFpsrQc);
    }
    return true;
  }

  if ((insn & 0xFF80F400u) == 0x5F801000u || // FMLA (by element, scalar)
      (insn & 0xFF80F400u) == 0x5F805000u) { // FMLS (by element, scalar)
    const std::uint32_t sz = (insn >> 22) & 1u;
    const std::uint32_t l = (insn >> 21) & 1u;
    const std::uint32_t h = (insn >> 11) & 1u;
    if (sz != 0u && l != 0u) {
      return false;
    }
    const bool is_sub = (insn & 0x00004000u) != 0u;
    const std::uint32_t rm = ((insn >> 16) & 0xFu) | (((insn >> 20) & 1u) << 4u);
    const std::uint32_t index = sz == 0u ? ((h << 1u) | l) : h;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (sz == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_fma_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(vector_get_elem(qregs_[rm], 32u, index)),
          static_cast<std::uint32_t>(qregs_[rd][0] & 0xFFFFFFFFu),
          fpcr_mode,
          is_sub,
          false,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    const std::uint64_t result = fp_fma_bits<std::uint64_t, double>(
        qregs_[rn][0],
        vector_get_elem(qregs_[rm], 64u, index),
        qregs_[rd][0],
        fpcr_mode,
        is_sub,
        false,
        fpsr_bits);
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFF20FC00u) == 0x1E208800u) { // FNMUL (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          HostFpBinaryOp::Mul,
          fpsr_bits);
      result ^= fp_sign_mask_bits<std::uint32_t>();
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          fpcr_mode,
          HostFpBinaryOp::Mul,
          fpsr_bits);
      result ^= fp_sign_mask_bits<std::uint64_t>();
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF20FC00u) == 0x7E20D400u) { // FABD (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const bool is_double = ((insn >> 22) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (!is_double) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          HostFpBinaryOp::Sub,
          fpsr_bits);
      result &= ~fp_sign_mask_bits<std::uint32_t>();
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
        qregs_[rn][0],
        qregs_[rm][0],
        fpcr_mode,
        HostFpBinaryOp::Sub,
        fpsr_bits);
    result &= ~fp_sign_mask_bits<std::uint64_t>();
    sysregs_.fp_or_fpsr(fpsr_bits);
    qregs_[rd][0] = result;
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFF20FC00u) == 0x1E201800u) { // FDIV (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          HostFpBinaryOp::Div,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          fpcr_mode,
          HostFpBinaryOp::Div,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  switch (insn & 0xFFFFFC00u) {
    case 0x7E30D800u: { // FADDP Sd, Vn.2S
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 0u)),
          static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 1u)),
          fpcr_mode,
          HostFpBinaryOp::Add,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    case 0x7E70D800u: { // FADDP Dd, Vn.2D
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      std::uint64_t fpsr_bits = 0u;
      const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
          vector_get_elem(qregs_[rn], 64u, 0u),
          vector_get_elem(qregs_[rn], 64u, 1u),
          fpcr_mode,
          HostFpBinaryOp::Add,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    case 0x7E30F800u: // FMAXP Sd, Vn.2S
    case 0x7E70F800u: // FMAXP Dd, Vn.2D
    case 0x7EB0F800u: // FMINP Sd, Vn.2S
    case 0x7EF0F800u: // FMINP Dd, Vn.2D
    case 0x7E30C800u: // FMAXNMP Sd, Vn.2S
    case 0x7E70C800u: // FMAXNMP Dd, Vn.2D
    case 0x7EB0C800u: // FMINNMP Sd, Vn.2S
    case 0x7EF0C800u: { // FMINNMP Dd, Vn.2D
      const std::uint32_t opcode = insn & 0xFFFFFC00u;
      const bool is_double = ((insn >> 22) & 1u) != 0u;
      const bool is_min = opcode == 0x7EB0F800u || opcode == 0x7EF0F800u ||
                          opcode == 0x7EB0C800u || opcode == 0x7EF0C800u;
      const bool numeric_variant = opcode == 0x7E30C800u || opcode == 0x7E70C800u ||
                                   opcode == 0x7EB0C800u || opcode == 0x7EF0C800u;
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      std::uint64_t fpsr_bits = 0u;
      if (!is_double) {
        const std::uint32_t result = fp_minmax_result_bits<std::uint32_t, float>(
            static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 0u)),
            static_cast<std::uint32_t>(vector_get_elem(qregs_[rn], 32u, 1u)),
            fpcr_mode,
            is_min,
            numeric_variant,
            fpsr_bits);
        sysregs_.fp_or_fpsr(fpsr_bits);
        write_fp32(rd, std::bit_cast<float>(result));
        return true;
      }
      const std::uint64_t result = fp_minmax_result_bits<std::uint64_t, double>(
          vector_get_elem(qregs_[rn], 64u, 0u),
          vector_get_elem(qregs_[rn], 64u, 1u),
          fpcr_mode,
          is_min,
          numeric_variant,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      write_fp64(rd, std::bit_cast<double>(result));
      return true;
    }
  }

  if ((insn & 0xFF20FC00u) == 0x1E204800u || // FMAX (scalar)
      (insn & 0xFF20FC00u) == 0x1E205800u || // FMIN (scalar)
      (insn & 0xFF20FC00u) == 0x1E206800u || // FMAXNM (scalar)
      (insn & 0xFF20FC00u) == 0x1E207800u) { // FMINNM (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t opcode = insn & 0xFF20FC00u;
    const bool is_min = opcode == 0x1E205800u || opcode == 0x1E207800u;
    const bool numeric_variant = opcode == 0x1E206800u || opcode == 0x1E207800u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint64_t fpsr_bits = 0u;
    if (ftype == 0u) {
      const std::uint32_t result = fp_minmax_result_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          is_min,
          numeric_variant,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      write_fp32(rd, std::bit_cast<float>(result));
      return true;
    }
    if (ftype == 1u) {
      const std::uint64_t result = fp_minmax_result_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          fpcr_mode,
          is_min,
          numeric_variant,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      write_fp64(rd, std::bit_cast<double>(result));
      return true;
    }
    return false;
  }

  if ((insn & 0xFF20FC00u) == 0x1E202800u) { // FADD (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          HostFpBinaryOp::Add,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          fpcr_mode,
          HostFpBinaryOp::Add,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF20FC00u) == 0x1E203800u) { // FSUB (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_binary_arith_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          fpcr_mode,
          HostFpBinaryOp::Sub,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint64_t result = fp_binary_arith_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          fpcr_mode,
          HostFpBinaryOp::Sub,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF208000u) == 0x1F000000u ||
      (insn & 0xFF208000u) == 0x1F008000u ||
      (insn & 0xFF208000u) == 0x1F200000u ||
      (insn & 0xFF208000u) == 0x1F208000u) { // FMADD/FMSUB/FNMADD/FNMSUB (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ra = (insn >> 10) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const bool negate_addend = (insn & 0x00200000u) != 0u;
    const bool negate_product = negate_addend ^ ((insn & 0x00008000u) != 0u);
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint32_t result = fp_fma_bits<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu),
          static_cast<std::uint32_t>(qregs_[ra][0] & 0xFFFFFFFFu),
          fpcr_mode,
          negate_product,
          negate_addend,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      const std::uint64_t result = fp_fma_bits<std::uint64_t, double>(
          qregs_[rn][0],
          qregs_[rm][0],
          qregs_[ra][0],
          fpcr_mode,
          negate_product,
          negate_addend,
          fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E20C000u) { // FABS Sd, Sn
    qregs_[rd][0] = static_cast<std::uint64_t>(
        fp_abs_bits(static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu)));
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E60C000u) { // FABS Dd, Dn
    qregs_[rd][0] = fp_abs_bits(qregs_[rn][0]);
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E214000u) { // FNEG Sd, Sn
    qregs_[rd][0] = static_cast<std::uint64_t>(
        fp_neg_bits(static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu)));
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E614000u) { // FNEG Dd, Dn
    qregs_[rd][0] = fp_neg_bits(qregs_[rn][0]);
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFA0FC00u) == 0x5E20E400u || // FCMEQ (scalar, register)
      (insn & 0xFFA0FC00u) == 0x7E20E400u || // FCMGE (scalar, register)
      (insn & 0xFFA0FC00u) == 0x7EA0E400u) { // FCMGT (scalar, register)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t tag = insn & 0xFFA0FC00u;
    const bool is_double = ((insn >> 22) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (!is_double) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t a_bits = static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu);
      std::uint32_t b_bits = static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu);
      a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
      b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
      const float a = std::bit_cast<float>(a_bits);
      const float b = std::bit_cast<float>(b_bits);
      bool predicate = false;
      if (!std::isnan(a) && !std::isnan(b)) {
        switch (tag) {
          case 0x5E20E400u: predicate = (a == b); break;
          case 0x7E20E400u: predicate = (a >= b); break;
          case 0x7EA0E400u: predicate = (a > b); break;
        }
      }
      fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
      sysregs_.fp_or_fpsr(fpsr_bits);
      write_fp_compare_result(rd, predicate, 32u);
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    std::uint64_t a_bits = qregs_[rn][0];
    std::uint64_t b_bits = qregs_[rm][0];
    a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
    b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
    const double a = std::bit_cast<double>(a_bits);
    const double b = std::bit_cast<double>(b_bits);
    bool predicate = false;
    if (!std::isnan(a) && !std::isnan(b)) {
      switch (tag) {
        case 0x5E20E400u: predicate = (a == b); break;
        case 0x7E20E400u: predicate = (a >= b); break;
        case 0x7EA0E400u: predicate = (a > b); break;
      }
    }
    fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
    sysregs_.fp_or_fpsr(fpsr_bits);
    write_fp_compare_result(rd, predicate, 64u);
    return true;
  }

  if ((insn & 0xFFA0FC00u) == 0x7E20EC00u || // FACGE (scalar, register)
      (insn & 0xFFA0FC00u) == 0x7EA0EC00u) { // FACGT (scalar, register)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t tag = insn & 0xFFA0FC00u;
    const bool is_double = ((insn >> 22) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (!is_double) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t a_bits = static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu);
      std::uint32_t b_bits = static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu);
      a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
      b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
      const float a = std::bit_cast<float>(a_bits);
      const float b = std::bit_cast<float>(b_bits);
      bool predicate = false;
      if (!std::isnan(a) && !std::isnan(b)) {
        predicate = (tag == 0x7EA0EC00u) ? (std::fabs(a) > std::fabs(b))
                                         : (std::fabs(a) >= std::fabs(b));
      }
      fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
      sysregs_.fp_or_fpsr(fpsr_bits);
      write_fp_compare_result(rd, predicate, 32u);
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    std::uint64_t a_bits = qregs_[rn][0];
    std::uint64_t b_bits = qregs_[rm][0];
    a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
    b_bits = fp_flush_input_denormal_bits(b_bits, fpcr_mode, fpsr_bits);
    const double a = std::bit_cast<double>(a_bits);
    const double b = std::bit_cast<double>(b_bits);
    bool predicate = false;
    if (!std::isnan(a) && !std::isnan(b)) {
      predicate = (tag == 0x7EA0EC00u) ? (std::fabs(a) > std::fabs(b))
                                       : (std::fabs(a) >= std::fabs(b));
    }
    fpsr_bits |= fp_compare_fpsr_bits(a_bits, b_bits, false);
    sysregs_.fp_or_fpsr(fpsr_bits);
    write_fp_compare_result(rd, predicate, 64u);
    return true;
  }

  if ((insn & 0xFFA0FC00u) == 0x5EA0D800u || // FCMEQ (scalar, zero)
      (insn & 0xFFA0FC00u) == 0x7EA0C800u || // FCMGE (scalar, zero)
      (insn & 0xFFA0FC00u) == 0x5EA0C800u || // FCMGT (scalar, zero)
      (insn & 0xFFA0FC00u) == 0x7EA0D800u || // FCMLE (scalar, zero)
      (insn & 0xFFA0FC00u) == 0x5EA0E800u) { // FCMLT (scalar, zero)
    const std::uint32_t tag = insn & 0xFFA0FC00u;
    const bool is_double = ((insn >> 22) & 1u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (!is_double) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t a_bits = static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu);
      a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
      const float a = std::bit_cast<float>(a_bits);
      bool predicate = false;
      if (!std::isnan(a)) {
        switch (tag) {
          case 0x5EA0D800u: predicate = (a == 0.0f); break;
          case 0x7EA0C800u: predicate = (a >= 0.0f); break;
          case 0x5EA0C800u: predicate = (a > 0.0f); break;
          case 0x7EA0D800u: predicate = (a <= 0.0f); break;
          case 0x5EA0E800u: predicate = (a < 0.0f); break;
        }
      }
      fpsr_bits |= fp_compare_fpsr_bits(a_bits, static_cast<std::uint32_t>(0), false);
      sysregs_.fp_or_fpsr(fpsr_bits);
      write_fp_compare_result(rd, predicate, 32u);
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    std::uint64_t a_bits = qregs_[rn][0];
    a_bits = fp_flush_input_denormal_bits(a_bits, fpcr_mode, fpsr_bits);
    const double a = std::bit_cast<double>(a_bits);
    bool predicate = false;
    if (!std::isnan(a)) {
      switch (tag) {
        case 0x5EA0D800u: predicate = (a == 0.0); break;
        case 0x7EA0C800u: predicate = (a >= 0.0); break;
        case 0x5EA0C800u: predicate = (a > 0.0); break;
        case 0x7EA0D800u: predicate = (a <= 0.0); break;
        case 0x5EA0E800u: predicate = (a < 0.0); break;
      }
    }
    fpsr_bits |= fp_compare_fpsr_bits(a_bits, static_cast<std::uint64_t>(0), false);
    sysregs_.fp_or_fpsr(fpsr_bits);
    write_fp_compare_result(rd, predicate, 64u);
    return true;
  }


  if ((insn & 0xFF3FFC00u) == 0x5E21B800u) { // FCVTZS Sd|Dd, Sn|Dn
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    std::uint64_t fpsr_bits = 0;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 2u) {
      const long double value = fp_to_int_input_value<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          fpcr_mode,
          fpsr_bits);
      std::uint64_t convert_fpsr = 0u;
      const std::int32_t out = fp_to_signed_rtz<std::int32_t>(value, convert_fpsr);
      fpsr_bits |= convert_fpsr;
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = static_cast<std::uint32_t>(out);
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 3u) {
      const long double value = fp_to_int_input_value<std::uint64_t, double>(
          qregs_[rn][0],
          fpcr_mode,
          fpsr_bits);
      std::uint64_t convert_fpsr = 0u;
      const std::int64_t out = fp_to_signed_rtz<std::int64_t>(value, convert_fpsr);
      fpsr_bits |= convert_fpsr;
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = static_cast<std::uint64_t>(out);
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF3FFC00u) == 0x1E220000u || (insn & 0xFF3FFC00u) == 0x9E220000u) { // SCVTF Sd|Dd, Wn|Xn
    const bool from_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint64_t fpsr_bits = 0u;
    if (ftype == 0u) {
      const auto value = from_x ? static_cast<std::int64_t>(reg(rn))
                                : static_cast<std::int32_t>(reg32(rn));
      const std::uint32_t result = fp_int_to_fp_bits<std::uint32_t>(value, fpcr_mode, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      const auto value = from_x ? static_cast<std::int64_t>(reg(rn))
                                : static_cast<std::int64_t>(static_cast<std::int32_t>(reg32(rn)));
      const std::uint64_t result = fp_int_to_fp_bits<std::uint64_t>(value, fpcr_mode, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF3FFC00u) == 0x1E230000u || (insn & 0xFF3FFC00u) == 0x9E230000u) { // UCVTF Sd|Dd, Wn|Xn
    const bool from_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    std::uint64_t fpsr_bits = 0u;
    if (ftype == 0u) {
      const auto value = from_x ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
      const std::uint32_t result = fp_int_to_fp_bits<std::uint32_t>(value, fpcr_mode, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 1u) {
      const std::uint64_t value = from_x ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
      const std::uint64_t result = fp_int_to_fp_bits<std::uint64_t>(value, fpcr_mode, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = result;
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }


  if ((insn & 0xFF200C00u) == 0x1E200C00u) { // FCSEL Sd|Dd, Sn|Dn, Sm|Dm, cond
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t cond = (insn >> 12) & 0xFu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const bool take_rn = condition_holds(cond);
    if (ftype == 0u) {
      write_fp32(rd, take_rn ? read_fp32(rn) : read_fp32(rm));
      return true;
    }
    if (ftype == 1u) {
      write_fp64(rd, take_rn ? read_fp64(rn) : read_fp64(rm));
      return true;
    }
    return false;
  }

  if ((insn & 0xFFB80F20u) == 0x1E200400u) { // FCCMP/FCCMPE S/D
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t cond = (insn >> 12) & 0xFu;
    const std::uint32_t nzcv_imm = insn & 0xFu;
    const bool is_double = ((insn >> 22) & 0x1u) != 0u;
    const bool signaling_compare = (insn & 0x10u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (!condition_holds(cond)) {
      sysregs_.set_nzcv(static_cast<std::uint64_t>(nzcv_imm) << 28);
      return true;
    }
    if (!is_double) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t lhs_bits = static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu);
      std::uint32_t rhs_bits = static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu);
      lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
      rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
      const bool unordered = fp_is_nan_bits(lhs_bits) || fp_is_nan_bits(rhs_bits);
      if (unordered) {
        set_fp_compare_flags(true, false, false);
      } else {
        const float lhs = std::bit_cast<float>(lhs_bits);
        const float rhs = std::bit_cast<float>(rhs_bits);
        set_fp_compare_flags(false, lhs < rhs, lhs == rhs);
      }
      fpsr_bits |= fp_compare_fpsr_bits(lhs_bits, rhs_bits, signaling_compare);
      sysregs_.fp_or_fpsr(fpsr_bits);
      return true;
    }
    std::uint64_t fpsr_bits = 0u;
    std::uint64_t lhs_bits = qregs_[rn][0];
    std::uint64_t rhs_bits = qregs_[rm][0];
    lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
    rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
    const bool unordered = fp_is_nan_bits(lhs_bits) || fp_is_nan_bits(rhs_bits);
    if (unordered) {
      set_fp_compare_flags(true, false, false);
    } else {
      const double lhs = std::bit_cast<double>(lhs_bits);
      const double rhs = std::bit_cast<double>(rhs_bits);
      set_fp_compare_flags(false, lhs < rhs, lhs == rhs);
    }
    fpsr_bits |= fp_compare_fpsr_bits(lhs_bits, rhs_bits, signaling_compare);
    sysregs_.fp_or_fpsr(fpsr_bits);
    return true;
  }

  if (((insn & 0xFF20FC1Fu) == 0x1E202000u) || ((insn & 0xFF20FC1Fu) == 0x1E202010u)) { // FCMP/FCMPE Sn, Sm / Dn, Dm
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const bool signaling_compare = (insn & 0x10u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t lhs_bits = static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu);
      std::uint32_t rhs_bits = static_cast<std::uint32_t>(qregs_[rm][0] & 0xFFFFFFFFu);
      lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
      rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
      const bool unordered = fp_is_nan_bits(lhs_bits) || fp_is_nan_bits(rhs_bits);
      if (unordered) {
        set_fp_compare_flags(true, false, false);
      } else {
        const float lhs = std::bit_cast<float>(lhs_bits);
        const float rhs = std::bit_cast<float>(rhs_bits);
        set_fp_compare_flags(false, lhs < rhs, lhs == rhs);
      }
      fpsr_bits |= fp_compare_fpsr_bits(lhs_bits, rhs_bits, signaling_compare);
      sysregs_.fp_or_fpsr(fpsr_bits);
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      std::uint64_t lhs_bits = qregs_[rn][0];
      std::uint64_t rhs_bits = qregs_[rm][0];
      lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
      rhs_bits = fp_flush_input_denormal_bits(rhs_bits, fpcr_mode, fpsr_bits);
      const bool unordered = fp_is_nan_bits(lhs_bits) || fp_is_nan_bits(rhs_bits);
      if (unordered) {
        set_fp_compare_flags(true, false, false);
      } else {
        const double lhs = std::bit_cast<double>(lhs_bits);
        const double rhs = std::bit_cast<double>(rhs_bits);
        set_fp_compare_flags(false, lhs < rhs, lhs == rhs);
      }
      fpsr_bits |= fp_compare_fpsr_bits(lhs_bits, rhs_bits, signaling_compare);
      sysregs_.fp_or_fpsr(fpsr_bits);
      return true;
    }
    return false;
  }

  if (((insn & 0xFF20FC1Fu) == 0x1E202008u) || ((insn & 0xFF20FC1Fu) == 0x1E202018u)) { // FCMP/FCMPE Sn|Dn, #0.0
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const bool signaling_compare = (insn & 0x10u) != 0u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    if (ftype == 0u) {
      std::uint64_t fpsr_bits = 0u;
      std::uint32_t lhs_bits = static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu);
      lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
      if (fp_is_nan_bits(lhs_bits)) {
        set_fp_compare_flags(true, false, false);
      } else {
        const float lhs = std::bit_cast<float>(lhs_bits);
        set_fp_compare_flags(false, lhs < 0.0f, lhs == 0.0f);
      }
      fpsr_bits |= fp_compare_fpsr_bits(lhs_bits, 0u, signaling_compare);
      sysregs_.fp_or_fpsr(fpsr_bits);
      return true;
    }
    if (ftype == 1u) {
      std::uint64_t fpsr_bits = 0u;
      std::uint64_t lhs_bits = qregs_[rn][0];
      lhs_bits = fp_flush_input_denormal_bits(lhs_bits, fpcr_mode, fpsr_bits);
      if (fp_is_nan_bits(lhs_bits)) {
        set_fp_compare_flags(true, false, false);
      } else {
        const double lhs = std::bit_cast<double>(lhs_bits);
        set_fp_compare_flags(false, lhs < 0.0, lhs == 0.0);
      }
      fpsr_bits |= fp_compare_fpsr_bits(lhs_bits, static_cast<std::uint64_t>(0), signaling_compare);
      sysregs_.fp_or_fpsr(fpsr_bits);
      return true;
    }
    return false;
  }

  {
    const std::uint32_t tag = insn & 0xFF80FC00u;
    if (tag == 0x5F009400u || // SQSHRN (scalar)
        tag == 0x7F009400u || // UQSHRN (scalar)
        tag == 0x5F009C00u || // SQRSHRN (scalar)
        tag == 0x7F009C00u || // UQRSHRN (scalar)
        tag == 0x7F008400u || // SQSHRUN (scalar)
        tag == 0x7F008C00u) { // SQRSHRUN (scalar)
      const std::uint32_t immh = (insn >> 19) & 0xFu;
      const std::uint32_t immb = (insn >> 16) & 0x7u;
      if (immh == 0u) {
        return false;
      }
      const std::uint32_t src_esize_bits = 16u << highest_set_bit(immh);
      if (src_esize_bits < 16u || src_esize_bits > 64u) {
        return false;
      }
      const std::uint32_t immhb = (immh << 3u) | immb;
      const std::uint32_t shift = src_esize_bits - immhb;
      if (shift == 0u || shift > src_esize_bits / 2u) {
        return false;
      }
      const std::uint32_t dst_esize_bits = src_esize_bits / 2u;
      const bool round = tag == 0x5F009C00u || tag == 0x7F009C00u || tag == 0x7F008C00u;
      ShiftRightNarrowMode mode = ShiftRightNarrowMode::SignedToSignedSaturating;
      switch (tag) {
        case 0x5F009400u:
        case 0x5F009C00u:
          mode = ShiftRightNarrowMode::SignedToSignedSaturating;
          break;
        case 0x7F009400u:
        case 0x7F009C00u:
          mode = ShiftRightNarrowMode::UnsignedToUnsignedSaturating;
          break;
        case 0x7F008400u:
        case 0x7F008C00u:
          mode = ShiftRightNarrowMode::SignedToUnsignedSaturating;
          break;
        default:
          return false;
      }
      bool saturated = false;
      const std::uint64_t narrowed = shift_right_narrow_result(
          qregs_[rn][0] & ones(src_esize_bits),
          src_esize_bits,
          dst_esize_bits,
          shift,
          round,
          mode,
          saturated);
      qregs_[rd][0] = narrowed & ones(dst_esize_bits);
      qregs_[rd][1] = 0u;
      if (saturated) {
        sysregs_.fp_or_fpsr(kFpsrQc);
      }
      return true;
    }
  }

  if ((insn & 0xFF3FFC00u) == 0x1E380000u || (insn & 0xFF3FFC00u) == 0x9E380000u) { // FCVTZS
    const bool to_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    long double value = 0.0L;
    std::uint64_t fpsr_bits = 0;
    if (ftype == 0u) {
      value = fp_to_int_input_value<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          fpcr_mode,
          fpsr_bits);
    } else if (ftype == 1u) {
      value = fp_to_int_input_value<std::uint64_t, double>(
          qregs_[rn][0],
          fpcr_mode,
          fpsr_bits);
    } else {
      return false;
    }
    std::uint64_t convert_fpsr = 0u;
    if (to_x) {
      const std::int64_t out = fp_to_signed_rtz<std::int64_t>(value, convert_fpsr);
      fpsr_bits |= convert_fpsr;
      sysregs_.fp_or_fpsr(fpsr_bits);
      set_reg(rd, static_cast<std::uint64_t>(out));
    } else {
      const std::int32_t out = fp_to_signed_rtz<std::int32_t>(value, convert_fpsr);
      fpsr_bits |= convert_fpsr;
      sysregs_.fp_or_fpsr(fpsr_bits);
      set_reg32(rd, static_cast<std::uint32_t>(out));
    }
    return true;
  }

  switch (insn & 0xFF3FFC00u) {
    case 0x1E200000u: // FCVTNS Wd, Sn
    case 0x1E210000u: // FCVTNU Wd, Sn
    case 0x1E240000u: // FCVTAS Wd, Sn
    case 0x1E250000u: // FCVTAU Wd, Sn
    case 0x1E280000u: // FCVTPS Wd, Sn
    case 0x1E290000u: // FCVTPU Wd, Sn
    case 0x1E300000u: // FCVTMS Wd, Sn
    case 0x1E310000u: // FCVTMU Wd, Sn
    case 0x1E600000u: // FCVTNS Wd, Dn
    case 0x1E610000u: // FCVTNU Wd, Dn
    case 0x1E640000u: // FCVTAS Wd, Dn
    case 0x1E650000u: // FCVTAU Wd, Dn
    case 0x1E680000u: // FCVTPS Wd, Dn
    case 0x1E690000u: // FCVTPU Wd, Dn
    case 0x1E700000u: // FCVTMS Wd, Dn
    case 0x1E710000u: // FCVTMU Wd, Dn
    case 0x9E200000u: // FCVTNS Xd, Sn
    case 0x9E210000u: // FCVTNU Xd, Sn
    case 0x9E240000u: // FCVTAS Xd, Sn
    case 0x9E250000u: // FCVTAU Xd, Sn
    case 0x9E280000u: // FCVTPS Xd, Sn
    case 0x9E290000u: // FCVTPU Xd, Sn
    case 0x9E300000u: // FCVTMS Xd, Sn
    case 0x9E310000u: // FCVTMU Xd, Sn
    case 0x9E600000u: // FCVTNS Xd, Dn
    case 0x9E610000u: // FCVTNU Xd, Dn
    case 0x9E640000u: // FCVTAS Xd, Dn
    case 0x9E650000u: // FCVTAU Xd, Dn
    case 0x9E680000u: // FCVTPS Xd, Dn
    case 0x9E690000u: // FCVTPU Xd, Dn
    case 0x9E700000u: // FCVTMS Xd, Dn
    case 0x9E710000u: { // FCVTMU Xd, Dn
      const std::uint32_t opcode = insn & 0xFF3FFC00u;
      const bool to_x = (insn >> 31) != 0u;
      const std::uint32_t ftype = (insn >> 22) & 0x3u;
      const std::uint32_t fpcr_mode = current_fpcr_ctl();
      long double value = 0.0L;
      std::uint64_t fpsr_bits = 0u;
      if (ftype == 0u) {
        value = fp_to_int_input_value<std::uint32_t, float>(
            static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
            fpcr_mode,
            fpsr_bits);
      } else if (ftype == 1u) {
        value = fp_to_int_input_value<std::uint64_t, double>(
            qregs_[rn][0],
            fpcr_mode,
            fpsr_bits);
      } else {
        return false;
      }

      const bool is_unsigned = (opcode & 0x00010000u) != 0u;
      FpToIntRoundingMode rounding = FpToIntRoundingMode::TieEven;
      switch (opcode & 0x001F0000u) {
        case 0x00000000u:
        case 0x00010000u:
          rounding = FpToIntRoundingMode::TieEven;
          break;
        case 0x00040000u:
        case 0x00050000u:
          rounding = FpToIntRoundingMode::TieAway;
          break;
        case 0x00080000u:
        case 0x00090000u:
          rounding = FpToIntRoundingMode::PosInf;
          break;
        case 0x00100000u:
        case 0x00110000u:
          rounding = FpToIntRoundingMode::NegInf;
          break;
        default:
          return false;
      }

      if (is_unsigned) {
        if (to_x) {
          std::uint64_t convert_fpsr = 0u;
          const std::uint64_t out = fp_to_unsigned_round<std::uint64_t>(value, rounding, convert_fpsr);
          fpsr_bits |= convert_fpsr;
          sysregs_.fp_or_fpsr(fpsr_bits);
          set_reg(rd, out);
        } else {
          std::uint64_t convert_fpsr = 0u;
          const std::uint32_t out = fp_to_unsigned_round<std::uint32_t>(value, rounding, convert_fpsr);
          fpsr_bits |= convert_fpsr;
          sysregs_.fp_or_fpsr(fpsr_bits);
          set_reg32(rd, out);
        }
      } else {
        if (to_x) {
          std::uint64_t convert_fpsr = 0u;
          const std::int64_t out = fp_to_signed_round<std::int64_t>(value, rounding, convert_fpsr);
          fpsr_bits |= convert_fpsr;
          sysregs_.fp_or_fpsr(fpsr_bits);
          set_reg(rd, static_cast<std::uint64_t>(out));
        } else {
          std::uint64_t convert_fpsr = 0u;
          const std::int32_t out = fp_to_signed_round<std::int32_t>(value, rounding, convert_fpsr);
          fpsr_bits |= convert_fpsr;
          sysregs_.fp_or_fpsr(fpsr_bits);
          set_reg32(rd, static_cast<std::uint32_t>(out));
        }
      }
      return true;
    }
  }

  if ((insn & 0xFF3FFC00u) == 0x1E390000u || (insn & 0xFF3FFC00u) == 0x9E390000u) { // FCVTZU
    const bool to_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    const std::uint32_t fpcr_mode = current_fpcr_ctl();
    long double value = 0.0L;
    std::uint64_t fpsr_bits = 0;
    if (ftype == 0u) {
      value = fp_to_int_input_value<std::uint32_t, float>(
          static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu),
          fpcr_mode,
          fpsr_bits);
    } else if (ftype == 1u) {
      value = fp_to_int_input_value<std::uint64_t, double>(
          qregs_[rn][0],
          fpcr_mode,
          fpsr_bits);
    } else {
      return false;
    }
    std::uint64_t convert_fpsr = 0u;
    if (to_x) {
      const std::uint64_t out = fp_to_unsigned_rtz<std::uint64_t>(value, convert_fpsr);
      fpsr_bits |= convert_fpsr;
      sysregs_.fp_or_fpsr(fpsr_bits);
      set_reg(rd, out);
    } else {
      const std::uint32_t out = fp_to_unsigned_rtz<std::uint32_t>(value, convert_fpsr);
      fpsr_bits |= convert_fpsr;
      sysregs_.fp_or_fpsr(fpsr_bits);
      set_reg32(rd, out);
    }
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x9E660000u) { // FMOV Xt, Dn
    const std::uint32_t rn_fp = (insn >> 5) & 0x1Fu;
    set_reg(rd, qregs_[rn_fp][0]);
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x9E670000u) { // FMOV Dd, Xn
    const std::uint32_t rn_gp = (insn >> 5) & 0x1Fu;
    qregs_[rd][0] = reg(rn_gp);
    qregs_[rd][1] = 0u;
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E260000u) { // FMOV Wt, Sn
    const std::uint32_t rn_fp = (insn >> 5) & 0x1Fu;
    set_reg32(rd, static_cast<std::uint32_t>(qregs_[rn_fp][0] & 0xFFFFFFFFu));
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E270000u) { // FMOV Sd, Wn
    const std::uint32_t rn_gp = (insn >> 5) & 0x1Fu;
    qregs_[rd][0] = reg32(rn_gp);
    qregs_[rd][1] = 0u;
    return true;
  }

  // ADC/ADCS/SBC/SBCS (32/64-bit). NGC/NGCS fall out as aliases with Rn==ZR.
  {
    const std::uint32_t tag = insn & 0x7FE0FC00u;
    if (tag == 0x1A000000u || tag == 0x3A000000u || tag == 0x5A000000u || tag == 0x7A000000u) {
      const bool sf = (insn >> 31) != 0u;
      const bool is_sub = ((insn >> 30) & 1u) != 0u;
      const bool set_flags = ((insn >> 29) & 1u) != 0u;
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      const std::uint64_t carry_in = sysregs_.pstate().c ? 1u : 0u;
      std::uint64_t result = 0;
      bool c = false;
      bool v = false;
      bool n = false;
      bool z = false;
      if (sf) {
        const std::uint64_t lhs = reg(rn);
        const std::uint64_t rhs = reg(rm);
        if (!is_sub) {
          const std::uint64_t sum = lhs + rhs;
          const bool carry1 = sum < lhs;
          const std::uint64_t res = sum + carry_in;
          const bool carry2 = res < sum;
          result = res;
          c = carry1 || carry2;
          v = ((~(lhs ^ rhs) & (lhs ^ res) & (1ull << 63)) != 0u);
        } else {
          const std::uint64_t borrow = carry_in != 0u ? 0u : 1u;
          const std::uint64_t res = lhs - rhs - borrow;
          const bool borrow_out = lhs < rhs || (borrow != 0u && lhs == rhs);
          result = res;
          c = !borrow_out;
          v = (((lhs ^ rhs) & (lhs ^ res) & (1ull << 63)) != 0u);
        }
        n = ((result >> 63) & 1u) != 0u;
        z = result == 0u;
        set_reg(rd, result);
      } else {
        const std::uint32_t lhs = reg32(rn);
        const std::uint32_t rhs = reg32(rm);
        std::uint32_t res = 0;
        if (!is_sub) {
          const std::uint32_t sum = lhs + rhs;
          const bool carry1 = sum < lhs;
          res = static_cast<std::uint32_t>(sum + carry_in);
          const bool carry2 = res < sum;
          c = carry1 || carry2;
          v = ((~(lhs ^ rhs) & (lhs ^ res) & (1u << 31)) != 0u);
        } else {
          const std::uint32_t borrow = carry_in != 0u ? 0u : 1u;
          res = static_cast<std::uint32_t>(lhs - rhs - borrow);
          const bool borrow_out = lhs < rhs || (borrow != 0u && lhs == rhs);
          c = !borrow_out;
          v = (((lhs ^ rhs) & (lhs ^ res) & (1u << 31)) != 0u);
        }
        result = res;
        n = ((res >> 31) & 1u) != 0u;
        z = res == 0u;
        set_reg32(rd, res);
      }
      if (set_flags) {
        auto ps = sysregs_.pstate();
        ps.n = n;
        ps.z = z;
        ps.c = c;
        ps.v = v;
        sysregs_.set_pstate(ps);
      }
      return true;
    }
  }

  // ADR
  if ((insn & 0x9F000000u) == 0x10000000u) {
    const std::uint64_t this_pc = pc_ - 4;
    const std::uint64_t immlo = (insn >> 29) & 0x3u;
    const std::uint64_t immhi = (insn >> 5) & 0x7FFFFu;
    const std::int64_t imm = sign_extend((immhi << 2u) | immlo, 21);
    set_reg(rd, static_cast<std::uint64_t>(static_cast<std::int64_t>(this_pc) + imm));
    return true;
  }

  // ADRP
  if ((insn & 0x9F000000u) == 0x90000000u) {
    const std::uint64_t this_pc = pc_ - 4;
    const std::uint64_t page = this_pc & ~0xFFFull;
    const std::uint64_t immlo = (insn >> 29) & 0x3u;
    const std::uint64_t immhi = (insn >> 5) & 0x7FFFFu;
    const std::int64_t imm = sign_extend((immhi << 2u) | immlo, 21) << 12u;
    set_reg(rd, static_cast<std::uint64_t>(static_cast<std::int64_t>(page) + imm));
    return true;
  }

  // MOVZ (32/64-bit)
  if ((insn & 0x7F800000u) == 0x52800000u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint64_t imm16 = (insn >> 5) & 0xFFFFu;
    const std::uint64_t hw = (insn >> 21) & 0x3u;
    const std::uint64_t value = imm16 << (hw * 16);
    if (sf) {
      set_reg(rd, value);
    } else {
      set_reg32(rd, static_cast<std::uint32_t>(value));
    }
    return true;
  }

  // MOVK (32/64-bit)
  if ((insn & 0x7F800000u) == 0x72800000u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint64_t imm16 = (insn >> 5) & 0xFFFFu;
    const std::uint64_t hw = (insn >> 21) & 0x3u;
    const std::uint64_t shift = hw * 16;
    if (sf) {
      const std::uint64_t mask = ~(0xFFFFull << shift);
      const std::uint64_t updated = (reg(rd) & mask) | (imm16 << shift);
      set_reg(rd, updated);
    } else {
      if (hw > 1) {
        return false;
      }
      const std::uint32_t mask = ~(static_cast<std::uint32_t>(0xFFFFu) << shift);
      const std::uint32_t updated = (reg32(rd) & mask) | static_cast<std::uint32_t>(imm16 << shift);
      set_reg32(rd, updated);
    }
    return true;
  }

  // MOVN (32/64-bit)
  if ((insn & 0x7F800000u) == 0x12800000u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint64_t imm16 = (insn >> 5) & 0xFFFFu;
    const std::uint64_t hw = (insn >> 21) & 0x3u;
    const std::uint64_t value = ~(imm16 << (hw * 16));
    if (sf) {
      set_reg(rd, value);
    } else {
      if (hw > 1) {
        return false;
      }
      set_reg32(rd, static_cast<std::uint32_t>(value));
    }
    return true;
  }

  // FEAT_MTE is absent in the current model. ADDG/SUBG share the generic
  // ADD/SUB (immediate) top-level opcode space, so without an explicit guard
  // they execute as ordinary arithmetic instead of remaining UNDEFINED.
  if ((insn & 0x7FC00000u) == 0x11800000u || (insn & 0x7FC00000u) == 0x51800000u) {
    enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
    return true;
  }

  // ADD/ADDS (immediate, 32/64-bit)
  if ((insn & 0x7F000000u) == 0x11000000u || (insn & 0x7F000000u) == 0x31000000u) {
    const bool sf = (insn >> 31) != 0;
    const bool set_flags = (insn & 0x20000000u) != 0;
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint32_t shift = (insn >> 22) & 0x1u;
    const std::uint64_t rhs = imm12 << (shift ? 12u : 0u);
    const std::uint64_t lhs = sp_or_reg(rn);
    const std::uint64_t value = sf ? (lhs + rhs) : (static_cast<std::uint32_t>(lhs) + static_cast<std::uint32_t>(rhs));
    if (set_flags) {
      if (sf) {
        set_reg(rd, value);
      } else {
        set_reg32(rd, static_cast<std::uint32_t>(value));
      }
    } else {
      set_sp_or_reg(rd, value, !sf);
    }
    if (set_flags) {
      if (sf) {
        set_flags_add(lhs, rhs, value, false);
      } else {
        set_flags_add(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      }
    }
    return true;
  }

  // SUB/SUBS (immediate, 32/64-bit)
  if ((insn & 0x7F000000u) == 0x51000000u || (insn & 0x7F000000u) == 0x71000000u) {
    const bool sf = (insn >> 31) != 0;
    const bool set_flags = (insn & 0x20000000u) != 0;
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint32_t shift = (insn >> 22) & 0x1u;
    const std::uint64_t rhs = imm12 << (shift ? 12u : 0u);
    const std::uint64_t lhs = sp_or_reg(rn);
    const std::uint64_t value = sf ? (lhs - rhs) : (static_cast<std::uint32_t>(lhs) - static_cast<std::uint32_t>(rhs));
    if (set_flags) {
      if (sf) {
        set_reg(rd, value);
      } else {
        set_reg32(rd, static_cast<std::uint32_t>(value));
      }
    } else {
      set_sp_or_reg(rd, value, !sf);
    }
    if (set_flags) {
      if (sf) {
        set_flags_sub(lhs, rhs, value, false);
      } else {
        set_flags_sub(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      }
    }
    return true;
  }

  // ADD/ADDS (shifted register, 32/64-bit)
  if ((insn & 0x7F200000u) == 0x0B000000u || (insn & 0x7F200000u) == 0x2B000000u) {
    const bool sf = (insn >> 31) != 0;
    const bool set_flags = (insn & 0x20000000u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t shift_type = (insn >> 22) & 0x3u;
    const std::uint32_t imm6 = (insn >> 10) & 0x3Fu;
    if (shift_type > 2u) {
      return false;
    }
    std::uint64_t rhs = 0;
    if (sf) {
      const std::uint64_t v = reg(rm);
      const std::uint32_t sh = imm6 & 63u;
      if (shift_type == 0u) {
        rhs = (sh >= 64u) ? 0 : (v << sh);
      } else if (shift_type == 1u) {
        rhs = (sh >= 64u) ? 0 : (v >> sh);
      } else {
        rhs = static_cast<std::uint64_t>(static_cast<std::int64_t>(v) >> sh);
      }
    } else {
      const std::uint32_t v = reg32(rm);
      const std::uint32_t sh = imm6 & 31u;
      if (shift_type == 0u) {
        rhs = static_cast<std::uint32_t>(v << sh);
      } else if (shift_type == 1u) {
        rhs = static_cast<std::uint32_t>(v >> sh);
      } else {
        rhs = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> sh);
      }
    }
    const std::uint64_t lhs = sf ? reg(rn) : reg32(rn);
    const std::uint64_t value = sf ? (lhs + rhs) : (static_cast<std::uint32_t>(lhs) + static_cast<std::uint32_t>(rhs));
    if (set_flags) {
      if (sf) {
        set_reg(rd, value);
      } else {
        set_reg32(rd, static_cast<std::uint32_t>(value));
      }
    } else {
      set_sp_or_reg(rd, value, !sf);
    }
    if (set_flags) {
      if (sf) {
        set_flags_add(lhs, rhs, value, false);
      } else {
        set_flags_add(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      }
    }
    return true;
  }

  // SUB/SUBS (shifted register, 32/64-bit)
  if ((insn & 0x7F200000u) == 0x4B000000u || (insn & 0x7F200000u) == 0x6B000000u) {
    const bool sf = (insn >> 31) != 0;
    const bool set_flags = (insn & 0x20000000u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t shift_type = (insn >> 22) & 0x3u;
    const std::uint32_t imm6 = (insn >> 10) & 0x3Fu;
    if (shift_type > 2u) {
      return false;
    }
    std::uint64_t rhs = 0;
    if (sf) {
      const std::uint64_t v = reg(rm);
      const std::uint32_t sh = imm6 & 63u;
      if (shift_type == 0u) {
        rhs = (sh >= 64u) ? 0 : (v << sh);
      } else if (shift_type == 1u) {
        rhs = (sh >= 64u) ? 0 : (v >> sh);
      } else {
        rhs = static_cast<std::uint64_t>(static_cast<std::int64_t>(v) >> sh);
      }
    } else {
      const std::uint32_t v = reg32(rm);
      const std::uint32_t sh = imm6 & 31u;
      if (shift_type == 0u) {
        rhs = static_cast<std::uint32_t>(v << sh);
      } else if (shift_type == 1u) {
        rhs = static_cast<std::uint32_t>(v >> sh);
      } else {
        rhs = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> sh);
      }
    }
    const std::uint64_t lhs = sf ? reg(rn) : reg32(rn);
    const std::uint64_t value = sf ? (lhs - rhs) : (static_cast<std::uint32_t>(lhs) - static_cast<std::uint32_t>(rhs));
    if (set_flags) {
      if (sf) {
        set_reg(rd, value);
      } else {
        set_reg32(rd, static_cast<std::uint32_t>(value));
      }
    } else {
      set_sp_or_reg(rd, value, !sf);
    }
    if (set_flags) {
      if (sf) {
        set_flags_sub(lhs, rhs, value, false);
      } else {
        set_flags_sub(static_cast<std::uint32_t>(lhs), static_cast<std::uint32_t>(rhs), static_cast<std::uint32_t>(value), true);
      }
    }
    return true;
  }

  // ADD/SUB (extended register, 32/64-bit)
  if ((insn & 0x1F200000u) == 0x0B200000u) {
    const bool sf = (insn >> 31) != 0;
    const bool is_sub = ((insn >> 30) & 0x1u) != 0;
    const bool set_flags = ((insn >> 29) & 0x1u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t option = (insn >> 13) & 0x7u;
    const std::uint32_t imm3 = (insn >> 10) & 0x7u;
    if (imm3 > 4u) {
      return false;
    }

    std::uint64_t rhs = 0;
    const std::uint64_t rm_x = reg(rm);
    const std::uint32_t rm_w = reg32(rm);
    switch (option) {
    case 0u: rhs = rm_w & 0xFFu; break;                             // UXTB
    case 1u: rhs = rm_w & 0xFFFFu; break;                           // UXTH
    case 2u: rhs = rm_w; break;                                     // UXTW
    case 3u: rhs = sf ? rm_x : rm_w; break;                         // UXTX/LSL
    case 4u: rhs = static_cast<std::uint64_t>(sign_extend(rm_w & 0xFFu, 8)); break;   // SXTB
    case 5u: rhs = static_cast<std::uint64_t>(sign_extend(rm_w & 0xFFFFu, 16)); break; // SXTH
    case 6u: rhs = static_cast<std::uint64_t>(sign_extend(rm_w, 32)); break;            // SXTW
    case 7u: rhs = sf ? rm_x : rm_w; break;                         // SXTX
    default: return false;
    }
    rhs <<= imm3;

    const std::uint64_t lhs = sp_or_reg(rn);
    const std::uint64_t value = is_sub
        ? (sf ? (lhs - rhs) : static_cast<std::uint32_t>(lhs - rhs))
        : (sf ? (lhs + rhs) : static_cast<std::uint32_t>(lhs + rhs));

    if (set_flags) {
      if (sf) {
        set_reg(rd, value);
      } else {
        set_reg32(rd, static_cast<std::uint32_t>(value));
      }
      if (is_sub) {
        if (sf) {
          set_flags_sub(lhs, rhs, value, false);
        } else {
          set_flags_sub(static_cast<std::uint32_t>(lhs),
                        static_cast<std::uint32_t>(rhs),
                        static_cast<std::uint32_t>(value),
                        true);
        }
      } else {
        if (sf) {
          set_flags_add(lhs, rhs, value, false);
        } else {
          set_flags_add(static_cast<std::uint32_t>(lhs),
                        static_cast<std::uint32_t>(rhs),
                        static_cast<std::uint32_t>(value),
                        true);
        }
      }
    } else {
      set_sp_or_reg(rd, value, !sf);
    }
    return true;
  }

  // CSEL (32/64-bit)
  if ((insn & 0x1FE00C00u) == 0x1A800000u || (insn & 0x1FE00C00u) == 0x1A800400u) {
    const bool sf = (insn >> 31) != 0;
    const bool op = ((insn >> 30) & 0x1u) != 0;
    const bool s = ((insn >> 10) & 0x1u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t cond = (insn >> 12) & 0xFu;
    const bool take_rn = condition_holds(cond);
    const std::uint64_t rn_val = sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
    const std::uint64_t rm_val = sf ? reg(rm) : static_cast<std::uint64_t>(reg32(rm));

    std::uint64_t false_val = rm_val;
    if (!op && s) {          // CSINC
      false_val = rm_val + 1u;
    } else if (op && !s) {   // CSINV
      false_val = ~rm_val;
    } else if (op && s) {    // CSNEG
      false_val = static_cast<std::uint64_t>(-static_cast<std::int64_t>(rm_val));
    }
    const std::uint64_t value = take_rn ? rn_val : false_val;
    if (sf) {
      set_reg(rd, value);
    } else {
      set_reg32(rd, static_cast<std::uint32_t>(value));
    }
    return true;
  }

  // CRC32/CRC32C (b/h/w/x)
  {
    const std::uint32_t tag = insn & 0xFFC0FC00u;
    std::uint32_t bytes = 0;
    std::uint32_t poly = 0;
    if (tag == 0x1AC04000u) {        // CRC32B
      bytes = 1u; poly = 0xEDB88320u;
    } else if (tag == 0x1AC04400u) { // CRC32H
      bytes = 2u; poly = 0xEDB88320u;
    } else if (tag == 0x1AC04800u) { // CRC32W
      bytes = 4u; poly = 0xEDB88320u;
    } else if (tag == 0x9AC04C00u) { // CRC32X
      bytes = 8u; poly = 0xEDB88320u;
    } else if (tag == 0x1AC05000u) { // CRC32CB
      bytes = 1u; poly = 0x82F63B78u;
    } else if (tag == 0x1AC05400u) { // CRC32CH
      bytes = 2u; poly = 0x82F63B78u;
    } else if (tag == 0x1AC05800u) { // CRC32CW
      bytes = 4u; poly = 0x82F63B78u;
    } else if (tag == 0x9AC05C00u) { // CRC32CX
      bytes = 8u; poly = 0x82F63B78u;
    }

    if (bytes != 0u) {
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      const std::uint32_t crc_in = reg32(rn);
      const std::uint64_t data = (bytes == 8u) ? reg(rm) : static_cast<std::uint64_t>(reg32(rm));
      set_reg32(rd, crc32_update(crc_in, data, bytes, poly));
      return true;
    }
  }

  // Data-processing (1 source): RBIT/REV16/REV32/REV/CLZ/CLS
  if ((insn & 0x5FE0FC00u) == 0x5AC00000u ||
      (insn & 0x5FE0FC00u) == 0x5AC00400u ||
      (insn & 0x5FE0FC00u) == 0x5AC00800u ||
      (insn & 0x5FE0FC00u) == 0x5AC00C00u ||
      (insn & 0x5FE0FC00u) == 0x5AC01000u ||
      (insn & 0x5FE0FC00u) == 0x5AC01400u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint32_t opcode = (insn >> 10) & 0x3Fu;
    const std::uint64_t src = sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
    std::uint64_t result = 0;

    if (opcode == 0u) { // RBIT
      result = bit_reverse(src, sf ? 64u : 32u);
    } else if (opcode == 1u) { // REV16
      if (sf) {
        const std::uint64_t x = src;
        result = ((x & 0x00FF00FF00FF00FFull) << 8) | ((x & 0xFF00FF00FF00FF00ull) >> 8);
      } else {
        const std::uint32_t x = static_cast<std::uint32_t>(src);
        result = static_cast<std::uint32_t>(((x & 0x00FF00FFu) << 8) | ((x & 0xFF00FF00u) >> 8));
      }
    } else if (opcode == 2u) { // REV (W) / REV32 (X)
      if (sf) {
        const std::uint64_t x = src;
        const std::uint64_t lo = static_cast<std::uint32_t>(x & 0xFFFFFFFFu);
        const std::uint64_t hi = static_cast<std::uint32_t>((x >> 32) & 0xFFFFFFFFu);
        result = (static_cast<std::uint64_t>(__builtin_bswap32(static_cast<std::uint32_t>(hi))) << 32) |
                 static_cast<std::uint64_t>(__builtin_bswap32(static_cast<std::uint32_t>(lo)));
      } else {
        result = static_cast<std::uint32_t>(__builtin_bswap32(static_cast<std::uint32_t>(src)));
      }
    } else if (opcode == 3u) { // REV (X) only
      if (!sf) {
        return false;
      }
      result = __builtin_bswap64(src);
    } else if (opcode == 4u) { // CLZ
      if (sf) {
        result = (src == 0) ? 64u : static_cast<std::uint64_t>(__builtin_clzll(src));
      } else {
        const std::uint32_t x = static_cast<std::uint32_t>(src);
        result = (x == 0) ? 32u : static_cast<std::uint32_t>(__builtin_clz(x));
      }
    } else if (opcode == 5u) { // CLS
      result = sf ? cls64(src) : cls32(static_cast<std::uint32_t>(src));
    } else {
      return false;
    }

    if (sf) {
      set_reg(rd, result);
    } else {
      set_reg32(rd, static_cast<std::uint32_t>(result));
    }
    return true;
  }

  // UDIV / SDIV (32/64-bit)
  if ((insn & 0x7FE0FC00u) == 0x1AC00800u || (insn & 0x7FE0FC00u) == 0x1AC00C00u) {
    const bool sf = (insn >> 31) != 0;
    const bool is_signed = ((insn & 0x00000400u) != 0);
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    if (sf) {
      const std::uint64_t lhs_u = reg(rn);
      const std::uint64_t rhs_u = reg(rm);
      if (rhs_u == 0) {
        set_reg(rd, 0);
        return true;
      }
      if (is_signed) {
        const std::int64_t lhs = static_cast<std::int64_t>(lhs_u);
        const std::int64_t rhs = static_cast<std::int64_t>(rhs_u);
        if (lhs == static_cast<std::int64_t>(0x8000000000000000ull) && rhs == -1) {
          set_reg(rd, static_cast<std::uint64_t>(lhs));
        } else {
          set_reg(rd, static_cast<std::uint64_t>(lhs / rhs));
        }
      } else {
        set_reg(rd, lhs_u / rhs_u);
      }
    } else {
      const std::uint32_t lhs_u = reg32(rn);
      const std::uint32_t rhs_u = reg32(rm);
      if (rhs_u == 0) {
        set_reg32(rd, 0);
        return true;
      }
      if (is_signed) {
        const std::int32_t lhs = static_cast<std::int32_t>(lhs_u);
        const std::int32_t rhs = static_cast<std::int32_t>(rhs_u);
        if (lhs == static_cast<std::int32_t>(0x80000000u) && rhs == -1) {
          set_reg32(rd, static_cast<std::uint32_t>(lhs));
        } else {
          set_reg32(rd, static_cast<std::uint32_t>(lhs / rhs));
        }
      } else {
        set_reg32(rd, lhs_u / rhs_u);
      }
    }
    return true;
  }

  // Variable shift (32/64-bit): LSLV/LSRV/ASRV/RORV
  if ((insn & 0x7FE0FC00u) == 0x1AC02000u ||
      (insn & 0x7FE0FC00u) == 0x1AC02400u ||
      (insn & 0x7FE0FC00u) == 0x1AC02800u ||
      (insn & 0x7FE0FC00u) == 0x1AC02C00u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t shift_type = (insn >> 10) & 0x3u; // 0:LSLV 1:LSRV 2:ASRV 3:RORV
    const std::uint32_t width = sf ? 64u : 32u;
    const std::uint32_t mask_bits = sf ? 63u : 31u;
    const std::uint32_t amount = static_cast<std::uint32_t>((sf ? reg(rm) : reg32(rm)) & mask_bits);
    const std::uint64_t src = sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
    const std::uint64_t mask = ones(width);
    std::uint64_t result = 0;

    if (shift_type == 0u) {
      result = (src << amount) & mask;
    } else if (shift_type == 1u) {
      result = (src & mask) >> amount;
    } else if (shift_type == 2u) {
      if (sf) {
        result = static_cast<std::uint64_t>(static_cast<std::int64_t>(src) >> amount);
      } else {
        result = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(src)) >> amount);
      }
    } else {
      result = ror(src, amount, width);
    }

    if (sf) {
      set_reg(rd, result);
    } else {
      set_reg32(rd, static_cast<std::uint32_t>(result));
    }
    return true;
  }

  // MADD / MSUB (32/64-bit), including MUL alias (Ra = XZR/WZR)
  if ((insn & 0x1FE08000u) == 0x1B000000u || (insn & 0x1FE08000u) == 0x1B008000u) {
    const bool sf = (insn >> 31) != 0;
    const bool is_sub = ((insn >> 15) & 0x1u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ra = (insn >> 10) & 0x1Fu;
    if (sf) {
      const std::uint64_t prod = reg(rn) * reg(rm);
      const std::uint64_t acc = reg(ra);
      set_reg(rd, is_sub ? (acc - prod) : (acc + prod));
    } else {
      const std::uint32_t prod = reg32(rn) * reg32(rm);
      const std::uint32_t acc = reg32(ra);
      set_reg32(rd, is_sub ? static_cast<std::uint32_t>(acc - prod) : static_cast<std::uint32_t>(acc + prod));
    }
    return true;
  }

  // UMADDL/UMSUBL/SMADDL/SMSUBL (includes UMULL/SMULL aliases with Ra=XZR)
  if ((insn & 0xFFE08000u) == 0x9B200000u || // SMADDL
      (insn & 0xFFE08000u) == 0x9B208000u || // SMSUBL
      (insn & 0xFFE08000u) == 0x9BA00000u || // UMADDL
      (insn & 0xFFE08000u) == 0x9BA08000u) { // UMSUBL
    const bool is_unsigned = ((insn & 0x00800000u) != 0);
    const bool is_sub = ((insn & 0x00008000u) != 0);
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ra = (insn >> 10) & 0x1Fu;
    const std::uint64_t acc = reg(ra);
    std::uint64_t prod = 0;

    if (is_unsigned) {
      prod = static_cast<std::uint64_t>(reg32(rn)) * static_cast<std::uint64_t>(reg32(rm));
    } else {
      const std::int64_t lhs = static_cast<std::int64_t>(static_cast<std::int32_t>(reg32(rn)));
      const std::int64_t rhs = static_cast<std::int64_t>(static_cast<std::int32_t>(reg32(rm)));
      prod = static_cast<std::uint64_t>(lhs * rhs);
    }
    set_reg(rd, is_sub ? (acc - prod) : (acc + prod));
    return true;
  }

  // UMULH / SMULH
  if ((insn & 0xFFE0FC00u) == 0x9BC07C00u || (insn & 0xFFE0FC00u) == 0x9B407C00u) {
    const bool is_unsigned = (insn & 0x00800000u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    set_reg(rd, is_unsigned ? umulh64(reg(rn), reg(rm)) : smulh64(reg(rn), reg(rm)));
    return true;
  }

  // CCMP / CCMN (register + immediate, 32/64-bit)
  if ((insn & 0x1FE00010u) == 0x1A400000u) {
    const bool sf = (insn >> 31) != 0;
    const bool is_ccmn = ((insn >> 30) & 0x1u) == 0u;
    const bool imm_form = ((insn >> 11) & 0x1u) != 0;
    const std::uint32_t cond = (insn >> 12) & 0xFu;
    const std::uint32_t nzcv_imm = insn & 0xFu;
    const std::uint64_t lhs = sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
    const std::uint64_t rhs = imm_form
        ? static_cast<std::uint64_t>((insn >> 16) & 0x1Fu)
        : (sf ? reg((insn >> 16) & 0x1Fu) : static_cast<std::uint64_t>(reg32((insn >> 16) & 0x1Fu)));

    if (condition_holds(cond)) {
      auto p = sysregs_.pstate();
      if (sf) {
        if (is_ccmn) {
          const std::uint64_t result = lhs + rhs;
          p.n = ((result >> 63) & 1u) != 0;
          p.z = result == 0;
          p.c = result < lhs;
          const bool lhs_sign = ((lhs >> 63) & 1u) != 0;
          const bool rhs_sign = ((rhs >> 63) & 1u) != 0;
          const bool res_sign = ((result >> 63) & 1u) != 0;
          p.v = (lhs_sign == rhs_sign) && (lhs_sign != res_sign);
        } else {
          const std::uint64_t result = lhs - rhs;
          p.n = ((result >> 63) & 1u) != 0;
          p.z = result == 0;
          p.c = lhs >= rhs;
          const bool lhs_sign = ((lhs >> 63) & 1u) != 0;
          const bool rhs_sign = ((rhs >> 63) & 1u) != 0;
          const bool res_sign = ((result >> 63) & 1u) != 0;
          p.v = (lhs_sign != rhs_sign) && (lhs_sign != res_sign);
        }
      } else {
        const std::uint32_t l32 = static_cast<std::uint32_t>(lhs);
        const std::uint32_t r32 = static_cast<std::uint32_t>(rhs);
        if (is_ccmn) {
          const std::uint32_t result = static_cast<std::uint32_t>(l32 + r32);
          p.n = ((result >> 31) & 1u) != 0;
          p.z = result == 0;
          p.c = result < l32;
          const bool lhs_sign = ((l32 >> 31) & 1u) != 0;
          const bool rhs_sign = ((r32 >> 31) & 1u) != 0;
          const bool res_sign = ((result >> 31) & 1u) != 0;
          p.v = (lhs_sign == rhs_sign) && (lhs_sign != res_sign);
        } else {
          const std::uint32_t result = static_cast<std::uint32_t>(l32 - r32);
          p.n = ((result >> 31) & 1u) != 0;
          p.z = result == 0;
          p.c = l32 >= r32;
          const bool lhs_sign = ((l32 >> 31) & 1u) != 0;
          const bool rhs_sign = ((r32 >> 31) & 1u) != 0;
          const bool res_sign = ((result >> 31) & 1u) != 0;
          p.v = (lhs_sign != rhs_sign) && (lhs_sign != res_sign);
        }
      }
      sysregs_.set_pstate(p);
    } else {
      sysregs_.set_nzcv(static_cast<std::uint64_t>(nzcv_imm) << 28);
    }
    return true;
  }

  // EXTR / ROR (immediate) (32/64-bit)
  if ((insn & 0x1F800000u) == 0x13800000u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint32_t n = (insn >> 22) & 0x1u;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t lsb = (insn >> 10) & 0x3Fu;
    const std::uint32_t width = sf ? 64u : 32u;
    if ((sf && n != 1u) || (!sf && n != 0u)) {
      return false;
    }
    if ((!sf && lsb >= 32u) || lsb >= 64u) {
      return false;
    }
    const std::uint64_t mask = ones(width);
    const std::uint64_t rn_val = (sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn))) & mask;
    const std::uint64_t rm_val = (sf ? reg(rm) : static_cast<std::uint64_t>(reg32(rm))) & mask;
    const std::uint64_t result = (lsb == 0u)
        ? rm_val
        : ((rm_val >> lsb) | ((rn_val << (width - lsb)) & mask));
    if (sf) {
      set_reg(rd, result);
    } else {
      set_reg32(rd, static_cast<std::uint32_t>(result));
    }
    return true;
  }

  // Logical shifted register (32/64-bit): AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS
  if ((insn & 0x1F000000u) == 0x0A000000u) {
    const bool sf = (insn >> 31) != 0;
    const bool n = ((insn >> 21) & 0x1u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t shift_type = (insn >> 22) & 0x3u;
    const std::uint32_t imm6 = (insn >> 10) & 0x3Fu;
    if (!sf && (imm6 & 0x20u)) {
      return false;
    }

    const std::uint64_t width_mask = sf ? ~0ull : 0xFFFFFFFFull;
    const std::uint32_t sh = sf ? (imm6 & 63u) : (imm6 & 31u);
    const std::uint64_t rhs_src = sf ? reg(rm) : static_cast<std::uint64_t>(reg32(rm));
    std::uint64_t rhs = 0;
    if (shift_type == 0u) {        // LSL
      rhs = (rhs_src << sh);
    } else if (shift_type == 1u) { // LSR
      rhs = (rhs_src & width_mask) >> sh;
    } else if (shift_type == 2u) { // ASR
      if (sf) {
        rhs = static_cast<std::uint64_t>(static_cast<std::int64_t>(rhs_src) >> sh);
      } else {
        rhs = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(rhs_src)) >> sh);
      }
    } else { // ROR
      rhs = ror(rhs_src, sh, sf ? 64u : 32u);
    }
    rhs &= width_mask;
    if (n) {
      rhs = (~rhs) & width_mask;
    }
    const std::uint64_t lhs = sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
    const std::uint32_t opc = (insn >> 29) & 0x3u;
    std::uint64_t value = 0;

    if (opc == 0u) {
      value = lhs & rhs;
    } else if (opc == 1u) {
      value = lhs | rhs;
    } else if (opc == 2u) {
      value = lhs ^ rhs;
    } else if (opc == 3u) {
      value = lhs & rhs;
    } else {
      return false;
    }

    if (opc == 3u) {
      set_flags_logic(value, !sf);
      if (rd != 31) {
        if (sf) {
          set_reg(rd, value);
        } else {
          set_reg32(rd, static_cast<std::uint32_t>(value));
        }
      }
    } else {
      if (sf) {
        set_reg(rd, value);
      } else {
        set_reg32(rd, static_cast<std::uint32_t>(value));
      }
    }
    return true;
  }

  // Logical immediate (32/64-bit): AND/ORR/EOR/ANDS
  if ((insn & 0x1F000000u) == 0x12000000u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint32_t opc = (insn >> 29) & 0x3u;
    const std::uint32_t n = (insn >> 22) & 0x1u;
    const std::uint32_t immr = (insn >> 16) & 0x3Fu;
    const std::uint32_t imms = (insn >> 10) & 0x3Fu;
    const std::uint32_t datasize = sf ? 64u : 32u;
    if (!sf && n != 0u) {
      return false;
    }
    std::uint64_t wmask = 0;
    std::uint64_t tmask = 0;
    if (!decode_bit_masks(n, imms, immr, datasize, wmask, tmask)) {
      return false;
    }
    (void)tmask;
    const std::uint64_t lhs = sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
    std::uint64_t value = 0;
    if (opc == 0u) {
      value = lhs & wmask;
    } else if (opc == 1u) {
      value = lhs | wmask;
    } else if (opc == 2u) {
      value = lhs ^ wmask;
    } else if (opc == 3u) {
      value = lhs & wmask;
      set_flags_logic(value, !sf);
      if (rd == 31u) {
        return true;
      }
    } else {
      return false;
    }
    if (sf) {
      set_reg(rd, value);
    } else {
      set_reg32(rd, static_cast<std::uint32_t>(value));
    }
    return true;
  }

  // Bitfield (32/64-bit): SBFM/BFM/UBFM (+ aliases: ASR/SBFX/BFI/BFXIL/LSL/LSR/UBFX)
  if ((insn & 0x1F800000u) == 0x13000000u) {
    const bool sf = (insn >> 31) != 0;
    const std::uint32_t opc = (insn >> 29) & 0x3u;
    const std::uint32_t n = (insn >> 22) & 0x1u;
    const std::uint32_t immr = (insn >> 16) & 0x3Fu;
    const std::uint32_t imms = (insn >> 10) & 0x3Fu;
    const std::uint32_t datasize = sf ? 64u : 32u;
    if ((sf && n != 1u) || (!sf && n != 0u)) {
      return false;
    }

    std::uint64_t wmask = 0;
    std::uint64_t tmask = 0;
    if (!decode_bit_masks(n, imms, immr, datasize, wmask, tmask)) {
      return false;
    }

    const std::uint64_t src = sf ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
    const std::uint64_t dst = sf ? reg(rd) : static_cast<std::uint64_t>(reg32(rd));
    const std::uint64_t bot = ror(src, immr, datasize) & wmask;
    std::uint64_t result = 0;

    if (opc == 0u) { // SBFM
      const bool sign = ((src >> (imms & (datasize - 1u))) & 1u) != 0;
      const std::uint64_t top = sign ? ones(datasize) : 0;
      result = (top & ~tmask) | (bot & tmask);
    } else if (opc == 1u) { // BFM
      const std::uint64_t merged = (dst & ~wmask) | bot;
      result = (dst & ~tmask) | (merged & tmask);
    } else if (opc == 2u) { // UBFM
      result = bot & tmask;
    } else {
      return false;
    }

    if (sf) {
      set_reg(rd, result);
    } else {
      set_reg32(rd, static_cast<std::uint32_t>(result));
    }
    return true;
  }

  return false;
}

bool Cpu::exec_load_store(std::uint32_t insn) {
  const std::uint32_t rt = insn & 0x1Fu;
  const std::uint32_t rn = (insn >> 5) & 0x1Fu;
  bool access_failed = false;
  bool sync_exception_taken = false;
  const auto data_abort = [this, &sync_exception_taken](std::uint64_t va) {
    if (sync_exception_taken) {
      return;
    }
    this->data_abort(va);
  };
  const auto raw_mmu_read = [this](std::uint64_t va, std::size_t size) -> std::optional<std::uint64_t> {
    std::uint64_t value = 0;
    if (!mmu_read_value(va, size, &value)) {
      return std::nullopt;
    }
    return value;
  };
  const auto raw_mmu_write = [this](std::uint64_t va, std::uint64_t value, std::size_t size) -> bool {
    return mmu_write_value(va, value, size);
  };
  const auto raw_mmu_write_pair_atomic =
      [this](std::uint64_t addr, std::uint64_t first, std::uint64_t second, std::size_t elem_size) -> bool {
    last_data_fault_va_.reset();
    const std::size_t total_size = elem_size * 2u;
    if (elem_size == 0u || total_size > 16u) {
      return false;
    }

    std::array<std::uint64_t, 16> pas{};
    for (std::size_t i = 0; i < total_size; ++i) {
      std::uint64_t pa = 0;
      if (!translate_data_address_fast(addr + i, AccessType::Write, &pa)) {
        last_data_fault_va_ = addr + i;
        return false;
      }
      pas[i] = pa;
    }
    if (maybe_take_watchpoint_exception(addr, total_size, AccessType::Write)) {
      return false;
    }

    for (std::size_t i = 0; i < total_size; ++i) {
      const std::uint64_t value = (i < elem_size)
                                      ? ((first >> (i * 8u)) & 0xFFu)
                                      : ((second >> ((i - elem_size) * 8u)) & 0xFFu);
      if (!bus_.write(pas[i], value, 1u)) {
        note_external_data_abort(true, false, addr + i);
        last_data_fault_va_ = addr + i;
        return false;
      }
      on_code_write(addr + i, pas[i], 1u);
      if (callbacks_.memory_write) {
        callbacks_.memory_write(*this, pas[i], 1u);
      }
    }
    clear_exclusive_monitor();
    return true;
  };
  const auto mmu_read = [&](std::uint64_t va, std::size_t size) -> std::optional<std::uint64_t> {
    if (access_failed) {
      return std::nullopt;
    }
    if (maybe_take_sp_alignment_fault(rn) ||
        maybe_take_data_alignment_fault(va, size, AccessType::Read)) {
      access_failed = true;
      sync_exception_taken = true;
      return std::nullopt;
    }
    auto value = raw_mmu_read(va, size);
    if (!value.has_value() && this->exception_taken_this_step_) {
      sync_exception_taken = true;
    }
    access_failed |= !value.has_value();
    return value;
  };
  const auto mmu_write = [&](std::uint64_t va, std::uint64_t value, std::size_t size) -> bool {
    if (access_failed) {
      return false;
    }
    if (maybe_take_sp_alignment_fault(rn) ||
        maybe_take_data_alignment_fault(va, size, AccessType::Write)) {
      access_failed = true;
      sync_exception_taken = true;
      return false;
    }
    const bool ok = raw_mmu_write(va, value, size);
    if (!ok && this->exception_taken_this_step_) {
      sync_exception_taken = true;
    }
    access_failed |= !ok;
    return ok;
  };
  const auto mmu_strict_read = [&](std::uint64_t va, std::size_t size) -> std::optional<std::uint64_t> {
    if (access_failed) {
      return std::nullopt;
    }
    if (maybe_take_sp_alignment_fault(rn) ||
        maybe_take_data_alignment_fault(va, size, AccessType::Read, true)) {
      access_failed = true;
      sync_exception_taken = true;
      return std::nullopt;
    }
    auto value = raw_mmu_read(va, size);
    if (!value.has_value() && this->exception_taken_this_step_) {
      sync_exception_taken = true;
    }
    access_failed |= !value.has_value();
    return value;
  };
  const auto mmu_strict_write = [&](std::uint64_t va, std::uint64_t value, std::size_t size) -> bool {
    if (access_failed) {
      return false;
    }
    if (maybe_take_sp_alignment_fault(rn) ||
        maybe_take_data_alignment_fault(va, size, AccessType::Write, true)) {
      access_failed = true;
      sync_exception_taken = true;
      return false;
    }
    const bool ok = raw_mmu_write(va, value, size);
    if (!ok && this->exception_taken_this_step_) {
      sync_exception_taken = true;
    }
    access_failed |= !ok;
    return ok;
  };

  // LDRAA/LDRAB are architecturally allocated PAuth loads. With FEAT_PAuth
  // absent they must decode as UNDEFINED, but their encodings collide with the
  // generic X post/pre-index load/store masks handled later in this function.
  if (is_ldraa_ldrab_encoding(insn)) {
    enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
    return true;
  }

  const auto load_vec = [&](std::uint64_t addr, std::uint32_t vt, std::size_t size) -> bool {
    if (size == 16u) {
      const auto lo = mmu_read(addr, 8u);
      const auto hi = mmu_read(addr + 8u, 8u);
      if (!lo.has_value() || !hi.has_value()) {
        return false;
      }
      qregs_[vt][0] = *lo;
      qregs_[vt][1] = *hi;
      return true;
    }
    if (size == 8u) {
      const auto value = mmu_read(addr, 8u);
      if (!value.has_value()) {
        return false;
      }
      qregs_[vt][0] = *value;
      qregs_[vt][1] = 0u;
      return true;
    }
    if (size == 4u) {
      const auto value = mmu_read(addr, 4u);
      if (!value.has_value()) {
        return false;
      }
      qregs_[vt][0] = *value & 0xFFFFFFFFu;
      qregs_[vt][1] = 0u;
      return true;
    }
    if (size == 2u) {
      const auto value = mmu_read(addr, 2u);
      if (!value.has_value()) {
        return false;
      }
      qregs_[vt][0] = *value & 0xFFFFu;
      qregs_[vt][1] = 0u;
      return true;
    }
    if (size == 1u) {
      const auto value = mmu_read(addr, 1u);
      if (!value.has_value()) {
        return false;
      }
      qregs_[vt][0] = *value & 0xFFu;
      qregs_[vt][1] = 0u;
      return true;
    }
    return false;
  };
  const auto store_vec = [&](std::uint64_t addr, std::uint32_t vt, std::size_t size) -> bool {
    if (size == 16u) {
      return mmu_write(addr, qregs_[vt][0], 8u) && mmu_write(addr + 8u, qregs_[vt][1], 8u);
    }
    if (size == 8u) {
      return mmu_write(addr, qregs_[vt][0], 8u);
    }
    if (size == 4u) {
      return mmu_write(addr, qregs_[vt][0] & 0xFFFFFFFFu, 4u);
    }
    if (size == 2u) {
      return mmu_write(addr, qregs_[vt][0] & 0xFFFFu, 2u);
    }
    if (size == 1u) {
      return mmu_write(addr, qregs_[vt][0] & 0xFFu, 1u);
    }
    return false;
  };
  const auto load_vec_whole = [&](std::uint64_t addr,
                                  std::uint32_t vt,
                                  std::size_t size,
                                  std::uint64_t& fault_addr) -> bool {
    fault_addr = addr;
    if (access_failed) {
      return false;
    }
    if (maybe_take_sp_alignment_fault(rn) ||
        maybe_take_data_alignment_fault(addr, size, AccessType::Read)) {
      access_failed = true;
      sync_exception_taken = true;
      return false;
    }
    if (size == 16u) {
      const auto lo = raw_mmu_read(addr, 8u);
      if (!lo.has_value()) {
        access_failed = true;
        fault_addr = this->last_data_fault_va_.value_or(addr);
        return false;
      }
      const auto hi = raw_mmu_read(addr + 8u, 8u);
      if (!hi.has_value()) {
        access_failed = true;
        fault_addr = this->last_data_fault_va_.value_or(addr + 8u);
        return false;
      }
      qregs_[vt][0] = *lo;
      qregs_[vt][1] = *hi;
      return true;
    }
    const auto value = raw_mmu_read(addr, size);
    if (!value.has_value()) {
      access_failed = true;
      fault_addr = this->last_data_fault_va_.value_or(addr);
      return false;
    }
    if (size == 8u) {
      qregs_[vt][0] = *value;
      qregs_[vt][1] = 0u;
      return true;
    }
    if (size == 4u) {
      qregs_[vt][0] = *value & 0xFFFFFFFFu;
      qregs_[vt][1] = 0u;
      return true;
    }
    if (size == 2u) {
      qregs_[vt][0] = *value & 0xFFFFu;
      qregs_[vt][1] = 0u;
      return true;
    }
    if (size == 1u) {
      qregs_[vt][0] = *value & 0xFFu;
      qregs_[vt][1] = 0u;
      return true;
    }
    return false;
  };
  const auto store_vec_whole = [&](std::uint64_t addr,
                                   std::uint32_t vt,
                                   std::size_t size,
                                   std::uint64_t& fault_addr) -> bool {
    fault_addr = addr;
    if (access_failed) {
      return false;
    }
    if (maybe_take_sp_alignment_fault(rn) ||
        maybe_take_data_alignment_fault(addr, size, AccessType::Write)) {
      access_failed = true;
      sync_exception_taken = true;
      return false;
    }
    if (size == 16u) {
      if (!raw_mmu_write(addr, qregs_[vt][0], 8u)) {
        access_failed = true;
        fault_addr = this->last_data_fault_va_.value_or(addr);
        return false;
      }
      if (!raw_mmu_write(addr + 8u, qregs_[vt][1], 8u)) {
        access_failed = true;
        fault_addr = this->last_data_fault_va_.value_or(addr + 8u);
        return false;
      }
      return true;
    }
    std::uint64_t value = 0;
    if (size == 8u) {
      value = qregs_[vt][0];
    } else if (size == 4u) {
      value = qregs_[vt][0] & 0xFFFFFFFFu;
    } else if (size == 2u) {
      value = qregs_[vt][0] & 0xFFFFu;
    } else if (size == 1u) {
      value = qregs_[vt][0] & 0xFFu;
    } else {
      return false;
    }
    const bool ok = raw_mmu_write(addr, value, size);
    if (!ok) {
      fault_addr = this->last_data_fault_va_.value_or(addr);
    }
    access_failed |= !ok;
    return ok;
  };
  const auto load_vec_pair = [&](std::uint64_t addr,
                                 std::uint32_t vt,
                                 std::uint32_t vt2,
                                 std::size_t elem_size,
                                 std::uint64_t& fault_addr) -> bool {
    if (!load_vec_whole(addr, vt, elem_size, fault_addr)) {
      return false;
    }
    return load_vec_whole(addr + elem_size, vt2, elem_size, fault_addr);
  };
  const auto store_vec_pair = [&](std::uint64_t addr,
                                  std::uint32_t vt,
                                  std::uint32_t vt2,
                                  std::size_t elem_size,
                                  std::uint64_t& fault_addr) -> bool {
    if (!store_vec_whole(addr, vt, elem_size, fault_addr)) {
      return false;
    }
    return store_vec_whole(addr + elem_size, vt2, elem_size, fault_addr);
  };
  const auto load_vec_seq_element_bytes = [&](std::uint64_t addr,
                                              std::uint32_t vt,
                                              std::size_t reg_bytes,
                                              std::size_t reg_count,
                                              std::uint64_t& fault_addr) -> bool {
    fault_addr = addr;
    std::array<std::array<std::uint64_t, 2>, 4> regs{};
    for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
      for (std::size_t lane = 0; lane < reg_bytes; ++lane) {
        fault_addr = addr + reg_idx * reg_bytes + lane;
        const auto value = mmu_read(fault_addr, 1u);
        if (!value.has_value()) {
          return false;
        }
        vector_set_elem(regs[reg_idx], 8u, static_cast<std::uint32_t>(lane), *value & 0xFFu);
      }
    }
    for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
      qregs_[(vt + static_cast<std::uint32_t>(reg_idx)) & 0x1Fu] = regs[reg_idx];
    }
    return true;
  };
  const auto store_vec_seq_element_bytes = [&](std::uint64_t addr,
                                               std::uint32_t vt,
                                               std::size_t reg_bytes,
                                               std::size_t reg_count,
                                               std::uint64_t& fault_addr) -> bool {
    fault_addr = addr;
    std::array<std::array<std::uint64_t, 2>, 4> regs{};
    for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
      regs[reg_idx] = qregs_[(vt + static_cast<std::uint32_t>(reg_idx)) & 0x1Fu];
    }
    for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
      for (std::size_t lane = 0; lane < reg_bytes; ++lane) {
        fault_addr = addr + reg_idx * reg_bytes + lane;
        if (!mmu_write(fault_addr,
                       vector_get_elem(regs[reg_idx], 8u, static_cast<std::uint32_t>(lane)) & 0xFFu,
                       1u)) {
          return false;
        }
      }
    }
    return true;
  };
  const auto load_vec_struct_bytes = [&](std::uint64_t addr,
                                         std::uint32_t vt,
                                         std::size_t reg_bytes,
                                         std::size_t reg_count,
                                         std::uint64_t& fault_addr) -> bool {
    fault_addr = addr;
    std::array<std::array<std::uint64_t, 2>, 4> regs{};
    for (std::size_t lane = 0; lane < reg_bytes; ++lane) {
      for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
        fault_addr = addr + lane * reg_count + reg_idx;
        const auto value = mmu_read(fault_addr, 1u);
        if (!value.has_value()) {
          return false;
        }
        vector_set_elem(regs[reg_idx], 8u, static_cast<std::uint32_t>(lane), *value & 0xFFu);
      }
    }
    for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
      qregs_[(vt + static_cast<std::uint32_t>(reg_idx)) & 0x1Fu] = regs[reg_idx];
    }
    return true;
  };
  const auto store_vec_struct_bytes = [&](std::uint64_t addr,
                                          std::uint32_t vt,
                                          std::size_t reg_bytes,
                                          std::size_t reg_count,
                                          std::uint64_t& fault_addr) -> bool {
    fault_addr = addr;
    std::array<std::array<std::uint64_t, 2>, 4> regs{};
    for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
      regs[reg_idx] = qregs_[(vt + static_cast<std::uint32_t>(reg_idx)) & 0x1Fu];
    }
    for (std::size_t lane = 0; lane < reg_bytes; ++lane) {
      for (std::size_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
        fault_addr = addr + lane * reg_count + reg_idx;
        if (!mmu_write(fault_addr,
                       vector_get_elem(regs[reg_idx], 8u, static_cast<std::uint32_t>(lane)) & 0xFFu,
                       1u)) {
          return false;
        }
      }
    }
    return true;
  };
  const auto access_vec_single_struct = [&](std::uint64_t addr,
                                            std::uint32_t vt,
                                            std::uint32_t reg_count,
                                            std::uint32_t esize_bits,
                                            std::uint32_t lane_index,
                                            std::uint32_t datasize_bits,
                                            bool is_load,
                                            bool replicate,
                                            std::uint64_t& fault_addr) -> bool {
    const std::size_t ebytes = static_cast<std::size_t>(esize_bits / 8u);
    if (reg_count == 0u || reg_count > 4u || (esize_bits != 8u && esize_bits != 16u &&
                                              esize_bits != 32u && esize_bits != 64u)) {
      return false;
    }

    if (is_load) {
      std::array<std::array<std::uint64_t, 2>, 4> regs{};
      for (std::uint32_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
        const std::uint64_t elem_addr = addr + static_cast<std::uint64_t>(reg_idx) * ebytes;
        fault_addr = elem_addr;
        if (access_failed) {
          return false;
        }
        if (maybe_take_sp_alignment_fault(rn) ||
            maybe_take_data_alignment_fault(elem_addr, ebytes, AccessType::Read)) {
          access_failed = true;
          sync_exception_taken = true;
          return false;
        }
        std::uint64_t value = 0u;
        for (std::size_t byte = 0; byte < ebytes; ++byte) {
          fault_addr = elem_addr + byte;
          const auto part = raw_mmu_read(fault_addr, 1u);
          if (!part.has_value()) {
            access_failed = true;
            return false;
          }
          value |= (*part & 0xFFu) << (byte * 8u);
        }

        if (replicate) {
          std::array<std::uint64_t, 2> dst{};
          const std::uint32_t lanes = datasize_bits / esize_bits;
          for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            vector_set_elem(dst, esize_bits, lane, value & ones(esize_bits));
          }
          regs[reg_idx] = dst;
        } else {
          auto dst = qregs_[(vt + reg_idx) & 0x1Fu];
          vector_set_elem(dst, esize_bits, lane_index, value & ones(esize_bits));
          regs[reg_idx] = dst;
        }
      }

      for (std::uint32_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
        qregs_[(vt + reg_idx) & 0x1Fu] = regs[reg_idx];
      }
      return true;
    }

    for (std::uint32_t reg_idx = 0; reg_idx < reg_count; ++reg_idx) {
      const auto& src = qregs_[(vt + reg_idx) & 0x1Fu];
      const std::uint64_t elem_addr = addr + static_cast<std::uint64_t>(reg_idx) * ebytes;
      fault_addr = elem_addr;
      if (access_failed) {
        return false;
      }
      if (maybe_take_sp_alignment_fault(rn) ||
          maybe_take_data_alignment_fault(elem_addr, ebytes, AccessType::Write)) {
        access_failed = true;
        sync_exception_taken = true;
        return false;
      }
      const std::uint64_t elem = vector_get_elem(src, esize_bits, lane_index) & ones(esize_bits);
      for (std::size_t byte = 0; byte < ebytes; ++byte) {
        fault_addr = elem_addr + byte;
        if (!raw_mmu_write(fault_addr, (elem >> (byte * 8u)) & 0xFFu, 1u)) {
          access_failed = true;
          return false;
        }
      }
    }

    return true;
  };
  const auto decode_vec_single_struct = [&](std::uint32_t insn,
                                            bool& is_load,
                                            bool& replicate,
                                            std::uint32_t& reg_count,
                                            std::uint32_t& esize_bits,
                                            std::uint32_t& lane_index,
                                            std::uint32_t& datasize_bits) -> bool {
    const std::uint32_t q = (insn >> 30) & 1u;
    const std::uint32_t l = (insn >> 22) & 1u;
    const std::uint32_t r = (insn >> 21) & 1u;
    const std::uint32_t opcode = (insn >> 13) & 0x7u;
    const std::uint32_t s = (insn >> 12) & 1u;
    const std::uint32_t size = (insn >> 10) & 0x3u;
    std::uint32_t scale = (opcode >> 1) & 0x3u;

    is_load = l != 0u;
    reg_count = (((opcode & 1u) << 1u) | r) + 1u;
    replicate = false;
    lane_index = 0u;

    switch (scale) {
    case 0x3u:
      if (!is_load || s != 0u) {
        return false;
      }
      scale = size;
      replicate = true;
      break;
    case 0x0u:
      lane_index = (q << 3u) | (s << 2u) | size;
      break;
    case 0x1u:
      if ((size & 1u) != 0u) {
        return false;
      }
      lane_index = (q << 2u) | (s << 1u) | (size >> 1u);
      break;
    case 0x2u:
      if ((size & 0x2u) != 0u) {
        return false;
      }
      if ((size & 0x1u) == 0u) {
        lane_index = (q << 1u) | s;
      } else {
        if (s != 0u) {
          return false;
        }
        lane_index = q;
        scale = 0x3u;
      }
      break;
    default:
      return false;
    }

    datasize_bits = 64u << q;
    esize_bits = 8u << scale;
    return true;
  };
  const auto checked_fp_mem_base = [&]() -> std::optional<std::uint64_t> {
    if (maybe_take_sp_alignment_fault(rn)) {
      return std::nullopt;
    }
    return sp_or_reg(rn);
  };
  const auto exec_vec_seq_postindex = [&](std::size_t reg_bytes,
                                          std::size_t reg_count,
                                          bool is_load) -> bool {
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    const bool ok = is_load ? load_vec_seq_element_bytes(addr, rt, reg_bytes, reg_count, fault_addr)
                            : store_vec_seq_element_bytes(addr, rt, reg_bytes, reg_count, fault_addr);
    if (!ok) {
      data_abort(fault_addr);
      return true;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint64_t off = rm == 31u ? reg_bytes * reg_count : reg(rm);
    set_sp_or_reg(rn, addr + off, false);
    return true;
  };
  const auto exec_vec_struct_postindex = [&](std::size_t reg_bytes,
                                             std::size_t reg_count,
                                             bool is_load) -> bool {
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    const bool ok = is_load ? load_vec_struct_bytes(addr, rt, reg_bytes, reg_count, fault_addr)
                            : store_vec_struct_bytes(addr, rt, reg_bytes, reg_count, fault_addr);
    if (!ok) {
      data_abort(fault_addr);
      return true;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint64_t off = rm == 31u ? reg_bytes * reg_count : reg(rm);
    set_sp_or_reg(rn, addr + off, false);
    return true;
  };
  const auto exec_fp_pair_transfer = [&](std::size_t elem_size,
                                         bool is_load,
                                         bool writeback,
                                         bool post_index) -> bool {
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) *
                              static_cast<std::int64_t>(elem_size);
    const std::uint64_t base_addr = *base;
    const std::uint64_t addr =
        post_index ? base_addr
                   : static_cast<std::uint64_t>(static_cast<std::int64_t>(base_addr) + imm7);
    std::uint64_t fault_addr = addr;
    const bool ok =
        is_load ? load_vec_pair(addr, rt, rt2, elem_size, fault_addr)
                : store_vec_pair(addr, rt, rt2, elem_size, fault_addr);
    if (!ok) {
      data_abort(fault_addr);
      return true;
    }
    if (writeback) {
      const std::uint64_t wb_addr =
          post_index ? static_cast<std::uint64_t>(static_cast<std::int64_t>(base_addr) + imm7)
                     : addr;
      set_sp_or_reg(rn, wb_addr, false);
    }
    return true;
  };

  // SIMD&FP memory subset, including whole-register structured load/store
  // forms needed by user-space code generation and AdvSIMD compliance tests.
  if ((insn & 0xFFC00000u) == 0x3D800000u || (insn & 0xFFC00000u) == 0x3DC00000u ||
      (insn & 0xFFC00000u) == 0x3D000000u || (insn & 0xFFC00000u) == 0x3D400000u ||
      (insn & 0xFFC00000u) == 0x7D000000u || (insn & 0xFFC00000u) == 0x7D400000u ||
      (insn & 0xFFC00000u) == 0xBD000000u || (insn & 0xFFC00000u) == 0xBD400000u ||
      (insn & 0xFFC00000u) == 0xFD000000u || (insn & 0xFFC00000u) == 0xFD400000u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    std::size_t size = 16u;
    if ((insn & 0xFFC00000u) == 0x3D000000u || (insn & 0xFFC00000u) == 0x3D400000u) {
      size = 1u;
    } else if ((insn & 0xFFC00000u) == 0x7D000000u || (insn & 0xFFC00000u) == 0x7D400000u) {
      size = 2u;
    } else if ((insn & 0xFFC00000u) == 0xBD000000u || (insn & 0xFFC00000u) == 0xBD400000u) {
      size = 4u;
    } else if ((insn & 0xFFC00000u) == 0xFD000000u || (insn & 0xFFC00000u) == 0xFD400000u) {
      size = 8u;
    }
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base + (((insn >> 10) & 0xFFFu) * size);
    std::uint64_t fault_addr = addr;
    const bool ok = is_load ? load_vec_whole(addr, rt, size, fault_addr)
                            : store_vec_whole(addr, rt, size, fault_addr);
    if (!ok) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xFFC00C00u) == 0x3C800C00u || (insn & 0xFFC00C00u) == 0x3CC00C00u ||
      (insn & 0xFFC00C00u) == 0x3C800400u || (insn & 0xFFC00C00u) == 0x3CC00400u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    const std::size_t size = 16u;
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const bool pre_index = ((insn & 0xFFC00C00u) == 0x3C800C00u) || ((insn & 0xFFC00C00u) == 0x3CC00C00u);
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = pre_index ? static_cast<std::uint64_t>(static_cast<std::int64_t>(*base) + simm9) : *base;
    std::uint64_t fault_addr = addr;
    const bool ok = is_load ? load_vec_whole(addr, rt, size, fault_addr)
                            : store_vec_whole(addr, rt, size, fault_addr);
    if (!ok) {
      data_abort(fault_addr);
      return true;
    }
    set_sp_or_reg(rn, pre_index ? addr : static_cast<std::uint64_t>(static_cast<std::int64_t>(*base) + simm9), false);
    return true;
  }

  if ((insn & 0xFFE00C00u) == 0x3C200800u || (insn & 0xFFE00C00u) == 0x3C600800u ||
      (insn & 0xFFE00C00u) == 0x7C200800u || (insn & 0xFFE00C00u) == 0x7C600800u ||
      (insn & 0xFFE00C00u) == 0xBC200800u || (insn & 0xFFE00C00u) == 0xBC600800u ||
      (insn & 0xFFE00C00u) == 0xFC200800u || (insn & 0xFFE00C00u) == 0xFC600800u ||
      (insn & 0xFFE00C00u) == 0x3CA00800u || (insn & 0xFFE00C00u) == 0x3CE00800u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    std::size_t size = 16u;
    if ((insn & 0xFFE00C00u) == 0x3C200800u || (insn & 0xFFE00C00u) == 0x3C600800u) {
      size = 1u;
    } else if ((insn & 0xFFE00C00u) == 0x7C200800u || (insn & 0xFFE00C00u) == 0x7C600800u) {
      size = 2u;
    } else if ((insn & 0xFFE00C00u) == 0xBC200800u || (insn & 0xFFE00C00u) == 0xBC600800u) {
      size = 4u;
    } else if ((insn & 0xFFE00C00u) == 0xFC200800u || (insn & 0xFFE00C00u) == 0xFC600800u) {
      size = 8u;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t option = (insn >> 13) & 0x7u;
    const bool s = ((insn >> 12) & 1u) != 0u;
    std::uint64_t off = 0;
    if (option == 0x2u) {
      off = reg32(rm);
    } else if (option == 0x3u) {
      off = reg(rm);
    } else if (option == 0x6u) {
      off = static_cast<std::uint64_t>(sign_extend(reg32(rm), 32));
    } else if (option == 0x7u) {
      off = reg(rm);
    } else {
      return false;
    }
    if (s) {
      if (size == 16u) {
        off <<= 4u;
      } else if (size == 8u) {
        off <<= 3u;
      } else if (size == 4u) {
        off <<= 2u;
      } else if (size == 2u) {
        off <<= 1u;
      }
    }
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base + off;
    std::uint64_t fault_addr = addr;
    const bool ok = is_load ? load_vec_whole(addr, rt, size, fault_addr)
                            : store_vec_whole(addr, rt, size, fault_addr);
    if (!ok) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xFFC00C00u) == 0x3C000000u || (insn & 0xFFC00C00u) == 0x3C400000u ||
      (insn & 0xFFC00C00u) == 0x7C000000u || (insn & 0xFFC00C00u) == 0x7C400000u ||
      (insn & 0xFFC00C00u) == 0x3C800000u || (insn & 0xFFC00C00u) == 0x3CC00000u ||
      (insn & 0xFFC00C00u) == 0xBC000000u || (insn & 0xFFC00C00u) == 0xBC400000u ||
      (insn & 0xFFC00C00u) == 0xFC000000u || (insn & 0xFFC00C00u) == 0xFC400000u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    std::size_t size = 16u;
    if ((insn & 0xFFC00C00u) == 0x3C000000u || (insn & 0xFFC00C00u) == 0x3C400000u) {
      size = 1u;
    } else if ((insn & 0xFFC00C00u) == 0x7C000000u || (insn & 0xFFC00C00u) == 0x7C400000u) {
      size = 2u;
    } else if ((insn & 0xFFC00C00u) == 0xBC000000u || (insn & 0xFFC00C00u) == 0xBC400000u) {
      size = 4u;
    } else if ((insn & 0xFFC00C00u) == 0xFC000000u || (insn & 0xFFC00C00u) == 0xFC400000u) {
      size = 8u;
    }
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(*base) + simm9);
    std::uint64_t fault_addr = addr;
    const bool ok = is_load ? load_vec_whole(addr, rt, size, fault_addr)
                            : store_vec_whole(addr, rt, size, fault_addr);
    if (!ok) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBF9F0000u) == 0x0D000000u || (insn & 0xBF800000u) == 0x0D800000u) {
    const bool writeback = (insn & 0x00800000u) != 0u;
    bool is_load = false;
    bool replicate = false;
    std::uint32_t reg_count = 0u;
    std::uint32_t esize_bits = 0u;
    std::uint32_t lane_index = 0u;
    std::uint32_t datasize_bits = 0u;
    if (!decode_vec_single_struct(insn, is_load, replicate, reg_count, esize_bits, lane_index, datasize_bits)) {
      return false;
    }
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!access_vec_single_struct(addr, rt, reg_count, esize_bits, lane_index, datasize_bits,
                                  is_load, replicate, fault_addr)) {
      data_abort(fault_addr);
      return true;
    }
    if (writeback) {
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      const std::uint64_t imm_off = static_cast<std::uint64_t>(reg_count) * static_cast<std::uint64_t>(esize_bits / 8u);
      const std::uint64_t off = rm == 31u ? imm_off : reg(rm);
      set_sp_or_reg(rn, addr + off, false);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C407000u) { // LD1 {Vt.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t size = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!load_vec_seq_element_bytes(addr, rt, size, 1u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C007000u) { // ST1 {Vt.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t size = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!store_vec_seq_element_bytes(addr, rt, size, 1u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0CC07000u) { // LD1 {Vt.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t size = q ? 16u : 8u;
    return exec_vec_seq_postindex(size, 1u, true);
  }

  if ((insn & 0xBFE0FC00u) == 0x0C807000u) { // ST1 {Vt.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t size = q ? 16u : 8u;
    return exec_vec_seq_postindex(size, 1u, false);
  }

  if ((insn & 0xBFFFFC00u) == 0x0C40A000u) { // LD1 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!load_vec_seq_element_bytes(addr, rt, reg_bytes, 2u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C00A000u) { // ST1 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!store_vec_seq_element_bytes(addr, rt, reg_bytes, 2u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0CC0A000u) { // LD1 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_seq_postindex(reg_bytes, 2u, true);
  }

  if ((insn & 0xBFE0FC00u) == 0x0C80A000u) { // ST1 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_seq_postindex(reg_bytes, 2u, false);
  }

  if ((insn & 0xBFFFFC00u) == 0x0C406000u) { // LD1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!load_vec_seq_element_bytes(addr, rt, reg_bytes, 3u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C006000u) { // ST1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!store_vec_seq_element_bytes(addr, rt, reg_bytes, 3u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0CC06000u) { // LD1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_seq_postindex(reg_bytes, 3u, true);
  }

  if ((insn & 0xBFE0FC00u) == 0x0C806000u) { // ST1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_seq_postindex(reg_bytes, 3u, false);
  }

  if ((insn & 0xBFFFFC00u) == 0x0C402000u) { // LD1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!load_vec_seq_element_bytes(addr, rt, reg_bytes, 4u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C002000u) { // ST1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!store_vec_seq_element_bytes(addr, rt, reg_bytes, 4u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0CC02000u) { // LD1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_seq_postindex(reg_bytes, 4u, true);
  }

  if ((insn & 0xBFE0FC00u) == 0x0C802000u) { // ST1 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_seq_postindex(reg_bytes, 4u, false);
  }

  if ((insn & 0xBFFFFC00u) == 0x0C408000u) { // LD2 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!load_vec_struct_bytes(addr, rt, reg_bytes, 2u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C008000u) { // ST2 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!store_vec_struct_bytes(addr, rt, reg_bytes, 2u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0CC08000u) { // LD2 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_struct_postindex(reg_bytes, 2u, true);
  }

  if ((insn & 0xBFE0FC00u) == 0x0C808000u) { // ST2 {Vt.8B/16B,Vt2.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_struct_postindex(reg_bytes, 2u, false);
  }

  if ((insn & 0xBFFFFC00u) == 0x0C404000u) { // LD3 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!load_vec_struct_bytes(addr, rt, reg_bytes, 3u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C004000u) { // ST3 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!store_vec_struct_bytes(addr, rt, reg_bytes, 3u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0CC04000u) { // LD3 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_struct_postindex(reg_bytes, 3u, true);
  }

  if ((insn & 0xBFE0FC00u) == 0x0C804000u) { // ST3 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_struct_postindex(reg_bytes, 3u, false);
  }

  if ((insn & 0xBFFFFC00u) == 0x0C400000u) { // LD4 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!load_vec_struct_bytes(addr, rt, reg_bytes, 4u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C000000u) { // ST4 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    const auto base = checked_fp_mem_base();
    if (!base.has_value()) {
      return true;
    }
    const std::uint64_t addr = *base;
    std::uint64_t fault_addr = addr;
    if (!store_vec_struct_bytes(addr, rt, reg_bytes, 4u, fault_addr)) {
      data_abort(fault_addr);
    }
    return true;
  }

  if ((insn & 0xBFE0FC00u) == 0x0CC00000u) { // LD4 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_struct_postindex(reg_bytes, 4u, true);
  }

  if ((insn & 0xBFE0FC00u) == 0x0C800000u) { // ST4 {Vt.8B/16B,Vt2.8B/16B,Vt3.8B/16B,Vt4.8B/16B}, [Xn|SP], #imm|Xm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t reg_bytes = q ? 16u : 8u;
    return exec_vec_struct_postindex(reg_bytes, 4u, false);
  }

  // LDTR/STTR family (unprivileged immediate). In this model FEAT_UAO is not
  // implemented, so their explicit memory effects always use EL0 permission
  // checks when executed above EL0.
  {
    enum class LoadExtend { Zero, Sign32, Sign64 };
    const std::uint32_t tag = insn & 0xFFE00C00u;
    std::size_t size = 0u;
    bool is_load = false;
    LoadExtend extend = LoadExtend::Zero;
    switch (tag) {
    case 0x38400800u: size = 1u; is_load = true; break;   // LDTRB Wt
    case 0x38C00800u: size = 1u; is_load = true; extend = LoadExtend::Sign32; break; // LDTRSB Wt
    case 0x38800800u: size = 1u; is_load = true; extend = LoadExtend::Sign64; break; // LDTRSB Xt
    case 0x78400800u: size = 2u; is_load = true; break;   // LDTRH Wt
    case 0x78C00800u: size = 2u; is_load = true; extend = LoadExtend::Sign32; break; // LDTRSH Wt
    case 0x78800800u: size = 2u; is_load = true; extend = LoadExtend::Sign64; break; // LDTRSH Xt
    case 0xB8400800u: size = 4u; is_load = true; break;   // LDTR Wt
    case 0xB8800800u: size = 4u; is_load = true; extend = LoadExtend::Sign64; break; // LDTRSW Xt
    case 0xF8400800u: size = 8u; is_load = true; break;   // LDTR Xt
    case 0x38000800u: size = 1u; break;                   // STTRB Wt
    case 0x78000800u: size = 2u; break;                   // STTRH Wt
    case 0xB8000800u: size = 4u; break;                   // STTR Wt
    case 0xF8000800u: size = 8u; break;                   // STTR Xt
    default: break;
    }
    if (size != 0u) {
      const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
      const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
      if (maybe_take_sp_alignment_fault(rn)) {
        return true;
      }
      if (is_load) {
        std::uint64_t value = 0;
        if (maybe_take_data_alignment_fault(addr, size, AccessType::UnprivilegedRead)) {
          return true;
        }
        if (!mmu_read_value(addr, size, &value, AccessType::UnprivilegedRead)) {
          data_abort(addr);
          return true;
        }
        switch (extend) {
        case LoadExtend::Zero:
          if (size == 8u) {
            set_reg(rt, value);
          } else {
            set_reg32(rt, static_cast<std::uint32_t>(value));
          }
          break;
        case LoadExtend::Sign32:
          if (size == 1u) {
            set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value & 0xFFu))));
          } else {
            set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value & 0xFFFFu))));
          }
          break;
        case LoadExtend::Sign64:
          if (size == 1u) {
            set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int8_t>(value & 0xFFu))));
          } else if (size == 2u) {
            set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int16_t>(value & 0xFFFFu))));
          } else {
            set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value & 0xFFFFFFFFu))));
          }
          break;
        }
      } else {
        const std::uint64_t value = (size == 8u) ? reg(rt) : (size == 4u) ? reg32(rt) : (size == 2u) ? (reg32(rt) & 0xFFFFu) : (reg32(rt) & 0xFFu);
        if (maybe_take_data_alignment_fault(addr, size, AccessType::UnprivilegedWrite)) {
          return true;
        }
        if (!mmu_write_value(addr, value, size, AccessType::UnprivilegedWrite)) {
          data_abort(addr);
          return true;
        }
      }
      return true;
    }
  }

  // PRFM / PRFUM.
  // Architecturally these are prefetch hints. The PE may treat them as NOPs,
  // and they must not raise synchronous exceptions.
  if ((insn & 0xFF000000u) == 0xD8000000u ||
      (insn & 0xFFC00000u) == 0xF9800000u ||
      (insn & 0xFFE00C00u) == 0xF8A00800u ||
      (insn & 0xFFE00C00u) == 0xF8800000u) {
    return true;
  }

  // LDXR / LDAXR / LDXRB / LDAXRB / LDXRH / LDAXRH.
  {
    const std::uint32_t tag = insn & 0xFFE0FC00u;
    std::size_t size = 0;
    if (tag == 0x08407C00u || tag == 0x0840FC00u) {
      size = 1u;
    } else if (tag == 0x48407C00u || tag == 0x4840FC00u) {
      size = 2u;
    } else if (tag == 0x88407C00u || tag == 0x8840FC00u) {
      size = 4u;
    } else if (tag == 0xC8407C00u || tag == 0xC840FC00u) {
      size = 8u;
    }
    if (size != 0u) {
      const std::uint64_t addr = sp_or_reg(rn);
      const auto value = mmu_strict_read(addr, size);
      if (!value.has_value()) {
        data_abort(addr);
        return true;
      }
      if (size == 8u) {
        set_reg(rt, *value);
      } else {
        set_reg32(rt, static_cast<std::uint32_t>(*value));
      }
      set_exclusive_monitor(addr, size);
      return true;
    }
  }

  // STXR / STLXR / STXRB / STLXRB / STXRH / STLXRH.
  {
    const std::uint32_t tag = insn & 0xFFE0FC00u;
    std::size_t size = 0;
    if (tag == 0x08007C00u || tag == 0x0800FC00u) {
      size = 1u;
    } else if (tag == 0x48007C00u || tag == 0x4800FC00u) {
      size = 2u;
    } else if (tag == 0x88007C00u || tag == 0x8800FC00u) {
      size = 4u;
    } else if (tag == 0xC8007C00u || tag == 0xC800FC00u) {
      size = 8u;
    }
    if (size != 0u) {
      const std::uint32_t rs = (insn >> 16) & 0x1Fu;
      const std::uint64_t addr = sp_or_reg(rn);
      bool match = false;
      if (maybe_take_sp_alignment_fault(rn) ||
          maybe_take_data_alignment_fault(addr, size, AccessType::Write, true)) {
        clear_exclusive_monitor();
        return true;
      }
      if (!check_exclusive_monitor(addr, size, &match)) {
        clear_exclusive_monitor();
        data_abort(addr);
        return true;
      }
      clear_exclusive_monitor();
      if (!match) {
        set_reg32(rs, 1u);
        return true;
      }
      const std::uint64_t value = (size == 8u) ? reg(rt) : static_cast<std::uint64_t>(reg32(rt));
      if (!raw_mmu_write(addr, value, size)) {
        data_abort(addr);
        return true;
      }
      set_reg32(rs, 0u);
      return true;
    }
  }

  // LSE CASP/CASPA/CASPL/CASPAL.
  // In the current SoC model every guest memory access commits synchronously
  // into a single global order, so acquire/release/ordered variants collapse
  // to the same program-visible semantics as the base atomic RMW.
  if ((insn & 0x3FA07C00u) == 0x08207C00u) {
    const std::size_t elem_size = (((insn >> 30) & 0x1u) != 0u) ? 8u : 4u;
    const std::uint32_t rs = (insn >> 16) & 0x1Fu;
    const std::uint32_t rt_pair = rt;
    const std::uint64_t mask = (elem_size == 8u) ? ~0ull : 0xFFFFFFFFull;
    const std::uint64_t addr = sp_or_reg(rn);
    if (maybe_take_sp_alignment_fault(rn) ||
        maybe_take_data_alignment_fault(addr, elem_size * 2u, AccessType::Read, true)) {
      return true;
    }
    const auto old_lo = mmu_strict_read(addr, elem_size);
    const auto old_hi = mmu_strict_read(addr + elem_size, elem_size);
    if (!old_lo.has_value() || !old_hi.has_value()) {
      if (sync_exception_taken) {
        return true;
      }
      data_abort(addr);
      return true;
    }
    const std::uint64_t old_lo_value = *old_lo & mask;
    const std::uint64_t old_hi_value = *old_hi & mask;
    const std::uint64_t compare_lo = ((elem_size == 8u) ? reg(rs) : static_cast<std::uint64_t>(reg32(rs))) & mask;
    const std::uint64_t compare_hi = ((elem_size == 8u) ? reg(rs + 1u) : static_cast<std::uint64_t>(reg32(rs + 1u))) & mask;
    const std::uint64_t desired_lo = ((elem_size == 8u) ? reg(rt_pair) : static_cast<std::uint64_t>(reg32(rt_pair))) & mask;
    const std::uint64_t desired_hi = ((elem_size == 8u) ? reg(rt_pair + 1u) : static_cast<std::uint64_t>(reg32(rt_pair + 1u))) & mask;
    if (old_lo_value == compare_lo && old_hi_value == compare_hi) {
      if (!raw_mmu_write_pair_atomic(addr, desired_lo, desired_hi, elem_size)) {
        if (this->exception_taken_this_step_) {
          return true;
        }
        data_abort(addr);
        return true;
      }
    }
    if (elem_size == 8u) {
      set_reg(rs, old_lo_value);
      set_reg(rs + 1u, old_hi_value);
    } else {
      set_reg32(rs, static_cast<std::uint32_t>(old_lo_value));
      set_reg32(rs + 1u, static_cast<std::uint32_t>(old_hi_value));
    }
    return true;
  }

  // LSE atomic CAS/CASA/CASL/CASAL. As above, the current globally ordered
  // execution model already satisfies the required program-visible ordering.
  if ((insn & 0x3FA07C00u) == 0x08A07C00u) {
    const std::uint32_t size_code = (insn >> 30) & 0x3u;
    const std::size_t size = (size_code == 0u) ? 1u : (size_code == 1u) ? 2u : (size_code == 2u) ? 4u : 8u;
    if (size != 0u) {
      const std::uint32_t rs = (insn >> 16) & 0x1Fu;
      const std::uint64_t mask = (size == 8u) ? ~0ull : 0xFFFFFFFFull;
      const std::uint64_t addr = sp_or_reg(rn);
      const auto old = mmu_strict_read(addr, size);
      if (!old.has_value()) {
        if (sync_exception_taken) {
          return true;
        }
        data_abort(addr);
        return true;
      }
      const std::uint64_t old_value = *old & mask;
      const std::uint64_t compare_value = ((size == 8u) ? reg(rs) : static_cast<std::uint64_t>(reg32(rs))) & mask;
      const std::uint64_t desired_value = ((size == 8u) ? reg(rt) : static_cast<std::uint64_t>(reg32(rt))) & mask;
      if (old_value == compare_value) {
        if (!mmu_strict_write(addr, desired_value, size)) {
          data_abort(addr);
          return true;
        }
      }
      if (size == 8u) {
        set_reg(rs, old_value);
      } else {
        set_reg32(rs, static_cast<std::uint32_t>(old_value));
      }
      return true;
    }
  }

  // LSE SWP/LDADD/LDCLR/LDEOR/LDSET. Their ordering variants collapse to the
  // same behaviour here because guest-visible memory effects are already
  // globally ordered and synchronously committed.
  {
    const std::uint32_t tag = insn & 0x3F20FC00u;
    if (tag == 0x38208000u || tag == 0x38200000u || tag == 0x38201000u ||
        tag == 0x38202000u || tag == 0x38203000u) {
      const std::uint32_t size_code = (insn >> 30) & 0x3u;
      const std::size_t size = (size_code == 0u) ? 1u : (size_code == 1u) ? 2u : (size_code == 2u) ? 4u : 8u;
      if (size != 0u) {
        const std::uint32_t rs = (insn >> 16) & 0x1Fu;
        const std::uint64_t mask = (size == 8u) ? ~0ull : 0xFFFFFFFFull;
        const std::uint64_t addr = sp_or_reg(rn);
        const auto old = mmu_strict_read(addr, size);
        if (!old.has_value()) {
          if (sync_exception_taken) {
            return true;
          }
          data_abort(addr);
          return true;
        }
        const std::uint64_t old_value = *old & mask;
        const std::uint64_t src_value = ((size == 8u) ? reg(rs) : static_cast<std::uint64_t>(reg32(rs))) & mask;
        std::uint64_t new_value = old_value;
        switch (tag) {
        case 0x38208000u: new_value = src_value; break;
        case 0x38200000u: new_value = (old_value + src_value) & mask; break;
        case 0x38201000u: new_value = old_value & (~src_value & mask); break;
        case 0x38202000u: new_value = (old_value ^ src_value) & mask; break;
        case 0x38203000u: new_value = (old_value | src_value) & mask; break;
        default: break;
        }
        if (!mmu_strict_write(addr, new_value, size)) {
          data_abort(addr);
          return true;
        }
        if (size == 8u) {
          set_reg(rt, old_value);
        } else {
          set_reg32(rt, static_cast<std::uint32_t>(old_value));
        }
        return true;
      }
    }
  }

  // LDAR / STLR families (byte/half/word/xword).
  {
    const std::uint32_t tag = insn & 0xFFE0FC00u;
    std::size_t size = 0;
    bool is_load = false;
    if (tag == 0x08C0FC00u) {
      size = 1u;
      is_load = true;
    } else if (tag == 0x48C0FC00u) {
      size = 2u;
      is_load = true;
    } else if (tag == 0x88C0FC00u) {
      size = 4u;
      is_load = true;
    } else if (tag == 0xC8C0FC00u) {
      size = 8u;
      is_load = true;
    } else if (tag == 0x0880FC00u) {
      size = 1u;
    } else if (tag == 0x4880FC00u) {
      size = 2u;
    } else if (tag == 0x8880FC00u) {
      size = 4u;
    } else if (tag == 0xC880FC00u) {
      size = 8u;
    }
    if (size != 0u) {
      const std::uint64_t addr = sp_or_reg(rn);
      if (is_load) {
        const auto value = mmu_strict_read(addr, size);
        if (!value.has_value()) {
          if (sync_exception_taken) {
            return true;
          }
          data_abort(addr);
          return true;
        }
        if (size == 8u) {
          set_reg(rt, *value);
        } else {
          set_reg32(rt, static_cast<std::uint32_t>(*value));
        }
      } else {
        const std::uint64_t value = (size == 8u) ? reg(rt) : static_cast<std::uint64_t>(reg32(rt));
        if (!mmu_strict_write(addr, value, size)) {
          data_abort(addr);
          return true;
        }
      }
      return true;
    }
  }

  // FEAT_LRCPC is absent in the current model. LDAPR W/X/B/H live in the
  // generic load/store decode space, and the W/B/H encodings otherwise alias
  // LDURSW/LDURSB/LDURSH. Keep the whole base family architecturally
  // UNDEFINED instead of letting the overlapping unscaled-load handlers
  // perform guest-visible memory accesses.
  {
    const std::uint32_t tag = insn & 0xFFE0FC00u;
    if (tag == 0x38A0C000u ||  // LDAPRB Wt, [Xn]
        tag == 0x78A0C000u ||  // LDAPRH Wt, [Xn]
        tag == 0xB8A0C000u ||  // LDAPR Wt, [Xn]
        tag == 0xF8A0C000u) {  // LDAPR Xt, [Xn]
      enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
      return true;
    }
  }

  // FEAT_LRCPC2 is absent in the current model. The unscaled-immediate
  // LDAPUR/STLUR family overlaps the ordinary LDUR/STUR decode space, so keep
  // the whole family architecturally UNDEFINED instead of letting the generic
  // unscaled handlers perform guest-visible accesses.
  {
    const std::uint32_t tag = insn & 0xFFE00C00u;
    if (tag == 0x19400000u ||  // LDAPURB Wt, [Xn|SP, #simm9]
        tag == 0x19C00000u ||  // LDAPURSB Wt, [Xn|SP, #simm9]
        tag == 0x19800000u ||  // LDAPURSB Xt, [Xn|SP, #simm9]
        tag == 0x59400000u ||  // LDAPURH Wt, [Xn|SP, #simm9]
        tag == 0x59C00000u ||  // LDAPURSH Wt, [Xn|SP, #simm9]
        tag == 0x59800000u ||  // LDAPURSH Xt, [Xn|SP, #simm9]
        tag == 0x99400000u ||  // LDAPUR Wt, [Xn|SP, #simm9]
        tag == 0x99800000u ||  // LDAPURSW Xt, [Xn|SP, #simm9]
        tag == 0x19000000u ||  // STLURB Wt, [Xn|SP, #simm9]
        tag == 0x59000000u ||  // STLURH Wt, [Xn|SP, #simm9]
        tag == 0x99000000u) {  // STLUR Wt/Xt, [Xn|SP, #simm9]
      enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
      return true;
    }
  }

  // FEAT_LS64 / FEAT_LS64_V / FEAT_LS64_ACCDATA are absent in the current
  // model. LD64B/ST64B/ST64BV/ST64BV0 live in generic load/store decode
  // space; without an explicit intercept they alias unrelated unscaled
  // load/store handlers such as STUR Xt.
  {
    const std::uint32_t tag = insn & 0xFFE0FC00u;
    if (tag == 0xF820D000u ||  // LD64B Xt, [Xn]
        tag == 0xF8209000u ||  // ST64B Xt, [Xn]
        tag == 0xF820B000u ||  // ST64BV Xstatus, Xt, [Xn]
        tag == 0xF820A000u) {  // ST64BV0 Xstatus, Xt, [Xn]
      enter_sync_exception(pc_ - 4u, 0x00u, 0u, false, 0u);
      return true;
    }
  }

  // LDXP / LDAXP / STXP / STLXP (32/64-bit pair exclusive).
  {
    const std::uint32_t tag = insn & 0xFFE08000u;
    std::size_t elem_size = 0u;
    bool is_load = false;
    if (tag == 0x88600000u || tag == 0x88608000u) {
      elem_size = 4u;
      is_load = true;
    } else if (tag == 0x88200000u || tag == 0x88208000u) {
      elem_size = 4u;
    } else if (tag == 0xC8600000u || tag == 0xC8608000u) {
      elem_size = 8u;
      is_load = true;
    } else if (tag == 0xC8200000u || tag == 0xC8208000u) {
      elem_size = 8u;
    }
    if (elem_size != 0u) {
      const std::size_t total_size = elem_size * 2u;
      const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
      const std::uint64_t addr = sp_or_reg(rn);
      if (is_load) {
        if (maybe_take_sp_alignment_fault(rn) ||
            maybe_take_data_alignment_fault(addr, total_size, AccessType::Read, true)) {
          return true;
        }
        const auto v1 = mmu_strict_read(addr, elem_size);
        const auto v2 = mmu_strict_read(addr + elem_size, elem_size);
        if (!v1.has_value() || !v2.has_value()) {
          if (sync_exception_taken) {
            return true;
          }
          data_abort(addr);
          return true;
        }
        if (elem_size == 8u) {
          set_reg(rt, *v1);
          set_reg(rt2, *v2);
        } else {
          set_reg32(rt, static_cast<std::uint32_t>(*v1));
          set_reg32(rt2, static_cast<std::uint32_t>(*v2));
        }
        set_exclusive_monitor(addr, total_size);
        return true;
      }

      const std::uint32_t rs = (insn >> 16) & 0x1Fu;
      bool match = false;
      if (maybe_take_sp_alignment_fault(rn) ||
          maybe_take_data_alignment_fault(addr, total_size, AccessType::Write, true)) {
        clear_exclusive_monitor();
        return true;
      }
      if (!check_exclusive_monitor(addr, total_size, &match)) {
        clear_exclusive_monitor();
        data_abort(addr);
        return true;
      }
      clear_exclusive_monitor();
      if (!match) {
        set_reg32(rs, 1u);
        return true;
      }
      const std::uint64_t first = (elem_size == 8u) ? reg(rt) : static_cast<std::uint64_t>(reg32(rt));
      const std::uint64_t second = (elem_size == 8u) ? reg(rt2) : static_cast<std::uint64_t>(reg32(rt2));
      if (!raw_mmu_write_pair_atomic(addr, first, second, elem_size)) {
        if (this->exception_taken_this_step_) {
          return true;
        }
        data_abort(addr);
        return true;
      }
      set_reg32(rs, 0u);
      return true;
    }
  }

  // Sign-extending loads (register offset): LDRSB/LDRSH/LDRSW.
  {
    const std::uint32_t tag = insn & 0xFFE00C00u;
    std::size_t size = 0;
    std::uint32_t sign_bits = 0;
    bool dest_is_64 = false;
    if (tag == 0x38E00800u) {
      size = 1u; sign_bits = 8u; dest_is_64 = false;
    } else if (tag == 0x38A00800u) {
      size = 1u; sign_bits = 8u; dest_is_64 = true;
    } else if (tag == 0x78E00800u) {
      size = 2u; sign_bits = 16u; dest_is_64 = false;
    } else if (tag == 0x78A00800u) {
      size = 2u; sign_bits = 16u; dest_is_64 = true;
    } else if (tag == 0xB8A00800u) {
      size = 4u; sign_bits = 32u; dest_is_64 = true;
    }
    if (size != 0u) {
      const std::uint32_t rm = (insn >> 16) & 0x1Fu;
      const std::uint32_t option = (insn >> 13) & 0x7u;
      const bool s = ((insn >> 12) & 0x1u) != 0;
      std::uint64_t off = 0;
      if (option == 0x2u) {         // UXTW
        off = reg32(rm);
      } else if (option == 0x3u) {  // LSL/UXTX
        off = reg(rm);
      } else if (option == 0x6u) {  // SXTW
        off = static_cast<std::uint64_t>(sign_extend(reg32(rm), 32));
      } else if (option == 0x7u) {  // SXTX
        off = reg(rm);
      } else {
        return false;
      }
      off <<= (s ? (size == 2u ? 1u : size == 4u ? 2u : 0u) : 0u);
      const std::uint64_t addr = sp_or_reg(rn) + off;
      const auto value = mmu_read(addr, size);
      if (!value.has_value()) {
        data_abort(addr);
        return true;
      }
      const std::int64_t sx = sign_extend(*value & ones(sign_bits), sign_bits);
      if (dest_is_64) {
        set_reg(rt, static_cast<std::uint64_t>(sx));
      } else {
        set_reg32(rt, static_cast<std::uint32_t>(sx));
      }
      return true;
    }
  }

  // LDR/STR (register offset), minimal subset used by U-Boot.
  if (((insn & 0x3FE00C00u) == 0x38200800u || (insn & 0x3FE00C00u) == 0x38600800u) &&
      ((((insn >> 13) & 0x7u) == 0x2u) || (((insn >> 13) & 0x7u) == 0x3u) ||
       (((insn >> 13) & 0x7u) == 0x6u) || (((insn >> 13) & 0x7u) == 0x7u))) {
    const bool is_load = ((insn >> 22) & 0x1u) != 0;
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t size_code = (insn >> 30) & 0x3u;
    const std::uint32_t option = (insn >> 13) & 0x7u;
    const bool s = ((insn >> 12) & 0x1u) != 0;
    const std::size_t size = (size_code == 0u) ? 1u : (size_code == 1u) ? 2u : (size_code == 2u) ? 4u : 8u;
    std::uint64_t off = 0;
    if (option == 0x2u) {         // UXTW
      off = reg32(rm);
    } else if (option == 0x3u) {  // LSL/UXTX
      off = reg(rm);
    } else if (option == 0x6u) {  // SXTW
      off = static_cast<std::uint64_t>(sign_extend(reg32(rm), 32));
    } else {                      // SXTX
      off = reg(rm);
    }
    off <<= (s ? size_code : 0u);
    const std::uint64_t addr = sp_or_reg(rn) + off;
    if (is_load) {
      const auto value = mmu_read(addr, size);
      if (!value.has_value()) {
        data_abort(addr);
        return true;
      }
      if (size == 8u) {
        set_reg(rt, *value);
      } else if (size == 4u) {
        set_reg32(rt, static_cast<std::uint32_t>(*value));
      } else if (size == 2u) {
        set_reg32(rt, static_cast<std::uint16_t>(*value));
      } else {
        set_reg32(rt, static_cast<std::uint8_t>(*value));
      }
    } else {
      const std::uint64_t value = (size == 8u) ? reg(rt) : (size == 4u) ? reg32(rt) : (size == 2u) ? (reg32(rt) & 0xFFFFu) : (reg32(rt) & 0xFFu);
      if (!mmu_write(addr, value, size)) {
        data_abort(addr);
        return true;
      }
    }
    return true;
  }

  // LDR Xt, #imm19 (literal)
  if ((insn & 0xFF000000u) == 0x58000000u) {
    const std::int64_t imm19 = sign_extend((insn >> 5) & 0x7FFFFu, 19) << 2u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_ - 4) + imm19);
    const auto value = raw_mmu_read(addr, 8u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, *value);
    return true;
  }

  // LDR Wt, #imm19 (literal)
  if ((insn & 0xFF000000u) == 0x18000000u) {
    const std::int64_t imm19 = sign_extend((insn >> 5) & 0x7FFFFu, 19) << 2u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_ - 4) + imm19);
    const auto value = raw_mmu_read(addr, 4);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(*value));
    return true;
  }

  // LDRSW Xt, #imm19 (literal)
  if ((insn & 0xFF000000u) == 0x98000000u) {
    const std::int64_t imm19 = sign_extend((insn >> 5) & 0x7FFFFu, 19) << 2u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_ - 4) + imm19);
    const auto value = raw_mmu_read(addr, 4u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*value & 0xFFFFFFFFu))));
    return true;
  }

  // LDR St/Dt/Qt, #imm19 (literal)
  if ((insn & 0x3F000000u) == 0x1C000000u) {
    const std::uint32_t opc = (insn >> 30) & 0x3u;
    if (opc != 0x3u) {
      const std::size_t size = 4u << opc;
      const std::int64_t imm19 = sign_extend((insn >> 5) & 0x7FFFFu, 19) << 2u;
      const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(pc_ - 4) + imm19);
      if (!load_vec(addr, rt, size)) {
        data_abort(last_data_fault_va_.value_or(addr));
        return true;
      }
      return true;
    }
  }

  // Post-index / pre-index loads-stores (minimal no-sign-extend family).
  auto post_pre = [&](std::size_t size, bool is_load, bool is_pre) -> bool {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = is_pre
        ? static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9)
        : base;
    if (is_load) {
      const auto value = mmu_read(addr, size);
      if (!value.has_value()) {
        data_abort(addr);
        return true;
      }
      if (size == 8u) {
        set_reg(rt, *value);
      } else if (size == 4u) {
        set_reg32(rt, static_cast<std::uint32_t>(*value));
      } else if (size == 2u) {
        set_reg32(rt, static_cast<std::uint16_t>(*value));
      } else {
        set_reg32(rt, static_cast<std::uint8_t>(*value));
      }
    } else {
      const std::uint64_t value = (size == 8u) ? reg(rt) : (size == 4u) ? reg32(rt) : (size == 2u) ? (reg32(rt) & 0xFFFFu) : (reg32(rt) & 0xFFu);
      if (!mmu_write(addr, value, size)) {
        data_abort(addr);
        return true;
      }
    }
    if (is_pre) {
      set_sp_or_reg(rn, addr, false);
    } else {
      set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9), false);
    }
    return true;
  };

  // LDRB/STRB post/pre-index
  if ((insn & 0xFFC00C00u) == 0x38400400u) return post_pre(1, true, false);
  if ((insn & 0xFFC00C00u) == 0x38000400u) return post_pre(1, false, false);
  if ((insn & 0xFFC00C00u) == 0x38400C00u) return post_pre(1, true, true);
  if ((insn & 0xFFC00C00u) == 0x38000C00u) return post_pre(1, false, true);

  // LDRH/STRH post/pre-index
  if ((insn & 0xFFC00C00u) == 0x78400400u) return post_pre(2, true, false);
  if ((insn & 0xFFC00C00u) == 0x78000400u) return post_pre(2, false, false);
  if ((insn & 0xFFC00C00u) == 0x78400C00u) return post_pre(2, true, true);
  if ((insn & 0xFFC00C00u) == 0x78000C00u) return post_pre(2, false, true);

  // LDR/STR W post/pre-index
  if ((insn & 0xFFC00C00u) == 0xB8400400u) return post_pre(4, true, false);
  if ((insn & 0xFFC00C00u) == 0xB8000400u) return post_pre(4, false, false);
  if ((insn & 0xFFC00C00u) == 0xB8400C00u) return post_pre(4, true, true);
  if ((insn & 0xFFC00C00u) == 0xB8000C00u) return post_pre(4, false, true);

  // LDR/STR X post/pre-index
  if ((insn & 0xFFC00C00u) == 0xF8400400u) return post_pre(8, true, false);
  if ((insn & 0xFFC00C00u) == 0xF8000400u) return post_pre(8, false, false);
  if ((insn & 0xFFC00C00u) == 0xF8400C00u) return post_pre(8, true, true);
  if ((insn & 0xFFC00C00u) == 0xF8000C00u) return post_pre(8, false, true);

  auto post_pre_fp = [&](std::size_t size, bool is_load, bool is_pre) -> bool {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = is_pre
        ? static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9)
        : base;
    const bool ok = is_load ? load_vec(addr, rt, size) : store_vec(addr, rt, size);
    if (!ok) {
      data_abort(addr);
      return true;
    }
    if (is_pre) {
      set_sp_or_reg(rn, addr, false);
    } else {
      set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9), false);
    }
    return true;
  };

  if ((insn & 0xFFC00C00u) == 0xBC400400u) return post_pre_fp(4, true, false);
  if ((insn & 0xFFC00C00u) == 0xBC000400u) return post_pre_fp(4, false, false);
  if ((insn & 0xFFC00C00u) == 0xBC400C00u) return post_pre_fp(4, true, true);
  if ((insn & 0xFFC00C00u) == 0xBC000C00u) return post_pre_fp(4, false, true);
  if ((insn & 0xFFC00C00u) == 0x3C400400u) return post_pre_fp(1, true, false);
  if ((insn & 0xFFC00C00u) == 0x3C000400u) return post_pre_fp(1, false, false);
  if ((insn & 0xFFC00C00u) == 0x3C400C00u) return post_pre_fp(1, true, true);
  if ((insn & 0xFFC00C00u) == 0x3C000C00u) return post_pre_fp(1, false, true);
  if ((insn & 0xFFC00C00u) == 0x7C400400u) return post_pre_fp(2, true, false);
  if ((insn & 0xFFC00C00u) == 0x7C000400u) return post_pre_fp(2, false, false);
  if ((insn & 0xFFC00C00u) == 0x7C400C00u) return post_pre_fp(2, true, true);
  if ((insn & 0xFFC00C00u) == 0x7C000C00u) return post_pre_fp(2, false, true);
  if ((insn & 0xFFC00C00u) == 0xFC400400u) return post_pre_fp(8, true, false);
  if ((insn & 0xFFC00C00u) == 0xFC000400u) return post_pre_fp(8, false, false);
  if ((insn & 0xFFC00C00u) == 0xFC400C00u) return post_pre_fp(8, true, true);
  if ((insn & 0xFFC00C00u) == 0xFC000C00u) return post_pre_fp(8, false, true);

  auto post_pre_signed = [&](std::size_t size, bool dest64, bool is_pre) -> bool {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = is_pre
        ? static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9)
        : base;
    const auto value = mmu_read(addr, size);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    if (size == 1u) {
      const auto sv = static_cast<std::int64_t>(static_cast<std::int8_t>(*value & 0xFFu));
      if (dest64) {
        set_reg(rt, static_cast<std::uint64_t>(sv));
      } else {
        set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(sv)));
      }
    } else if (size == 2u) {
      const auto sv = static_cast<std::int64_t>(static_cast<std::int16_t>(*value & 0xFFFFu));
      if (dest64) {
        set_reg(rt, static_cast<std::uint64_t>(sv));
      } else {
        set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(sv)));
      }
    } else {
      return false;
    }
    if (is_pre) {
      set_sp_or_reg(rn, addr, false);
    } else {
      set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9), false);
    }
    return true;
  };

  // LDRSB/LDRSH post/pre-index
  if ((insn & 0xFFE00C00u) == 0x38C00400u) return post_pre_signed(1, false, false);
  if ((insn & 0xFFE00C00u) == 0x38800400u) return post_pre_signed(1, true, false);
  if ((insn & 0xFFE00C00u) == 0x38C00C00u) return post_pre_signed(1, false, true);
  if ((insn & 0xFFE00C00u) == 0x38800C00u) return post_pre_signed(1, true, true);
  if ((insn & 0xFFE00C00u) == 0x78C00400u) return post_pre_signed(2, false, false);
  if ((insn & 0xFFE00C00u) == 0x78800400u) return post_pre_signed(2, true, false);
  if ((insn & 0xFFE00C00u) == 0x78C00C00u) return post_pre_signed(2, false, true);
  if ((insn & 0xFFE00C00u) == 0x78800C00u) return post_pre_signed(2, true, true);

  // LDUR Xt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0xF8400000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 8u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, *value);
    return true;
  }

  // LDURB Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x38400000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 1u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint8_t>(*value));
    return true;
  }

  // LDURSB Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x38C00000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 1u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(*value & 0xFFu))));
    return true;
  }

  // LDURSB Xt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x38800000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 1u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int8_t>(*value & 0xFFu))));
    return true;
  }

  // STURB Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x38000000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    if (!mmu_write(addr, reg32(rt) & 0xFFu, 1u)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDURH Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x78400000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 2u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint16_t>(*value));
    return true;
  }

  // LDURSH Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x78C00000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 2u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(*value & 0xFFFFu))));
    return true;
  }

  // LDURSH Xt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x78800000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 2u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int16_t>(*value & 0xFFFFu))));
    return true;
  }

  // STURH Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x78000000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    if (!mmu_write(addr, reg32(rt) & 0xFFFFu, 2u)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // STUR Xt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0xF8000000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    if (!mmu_write(addr, reg(rt), 8u)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDUR Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0xB8400000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 4);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(*value));
    return true;
  }

  // STUR Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0xB8000000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    if (!mmu_write(addr, reg32(rt), 4)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  if ((insn & 0xFFC00C00u) == 0x7C800C00u || (insn & 0xFFC00C00u) == 0x7CC00C00u ||
      (insn & 0xFFC00C00u) == 0x7C800400u || (insn & 0xFFC00C00u) == 0x7CC00400u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    const std::size_t size = 8u;
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const bool pre_index = ((insn & 0xFFC00C00u) == 0x7C800C00u) || ((insn & 0xFFC00C00u) == 0x7CC00C00u);
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = pre_index ? static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9) : base;
    const bool ok = is_load ? load_vec(addr, rt, size) : store_vec(addr, rt, size);
    if (!ok) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, pre_index ? addr : static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9), false);
    return true;
  }

  // STP/LDP scalar SIMD&FP pair transfers, including no-allocate pair forms.
  if ((insn & 0xFFC00000u) == 0x2D800000u) {
    return exec_fp_pair_transfer(4u, false, true, false);
  }
  if ((insn & 0xFFC00000u) == 0x2C800000u) {
    return exec_fp_pair_transfer(4u, false, true, true);
  }
  if ((insn & 0xFFC00000u) == 0x2D000000u || (insn & 0xFFC00000u) == 0x2C000000u) {
    return exec_fp_pair_transfer(4u, false, false, false);
  }
  if ((insn & 0xFFC00000u) == 0x2DC00000u) {
    return exec_fp_pair_transfer(4u, true, true, false);
  }
  if ((insn & 0xFFC00000u) == 0x2CC00000u) {
    return exec_fp_pair_transfer(4u, true, true, true);
  }
  if ((insn & 0xFFC00000u) == 0x2D400000u || (insn & 0xFFC00000u) == 0x2C400000u) {
    return exec_fp_pair_transfer(4u, true, false, false);
  }

  if ((insn & 0xFFC00000u) == 0x6D800000u) {
    return exec_fp_pair_transfer(8u, false, true, false);
  }
  if ((insn & 0xFFC00000u) == 0x6C800000u) {
    return exec_fp_pair_transfer(8u, false, true, true);
  }
  if ((insn & 0xFFC00000u) == 0x6D000000u || (insn & 0xFFC00000u) == 0x6C000000u) {
    return exec_fp_pair_transfer(8u, false, false, false);
  }
  if ((insn & 0xFFC00000u) == 0x6DC00000u) {
    return exec_fp_pair_transfer(8u, true, true, false);
  }
  if ((insn & 0xFFC00000u) == 0x6CC00000u) {
    return exec_fp_pair_transfer(8u, true, true, true);
  }
  if ((insn & 0xFFC00000u) == 0x6D400000u || (insn & 0xFFC00000u) == 0x6C400000u) {
    return exec_fp_pair_transfer(8u, true, false, false);
  }

  if ((insn & 0xFFC00000u) == 0xAD800000u) {
    return exec_fp_pair_transfer(16u, false, true, false);
  }
  if ((insn & 0xFFC00000u) == 0xAC800000u) {
    return exec_fp_pair_transfer(16u, false, true, true);
  }
  if ((insn & 0xFFC00000u) == 0xAD000000u || (insn & 0xFFC00000u) == 0xAC000000u) {
    return exec_fp_pair_transfer(16u, false, false, false);
  }
  if ((insn & 0xFFC00000u) == 0xADC00000u) {
    return exec_fp_pair_transfer(16u, true, true, false);
  }
  if ((insn & 0xFFC00000u) == 0xACC00000u) {
    return exec_fp_pair_transfer(16u, true, true, true);
  }
  if ((insn & 0xFFC00000u) == 0xAD400000u || (insn & 0xFFC00000u) == 0xAC400000u) {
    return exec_fp_pair_transfer(16u, true, false, false);
  }

  // STP Xt1, Xt2, [Xn|SP, #imm]!
  if ((insn & 0xFFC00000u) == 0xA9800000u && ((insn >> 23) & 0x3u) == 0x3u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    if (!mmu_write(addr, reg(rt), 8) || !mmu_write(addr + 8, reg(rt2), 8)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  // STP Wt1, Wt2, [Xn|SP, #imm]!  
  if ((insn & 0xFFC00000u) == 0x29800000u && ((insn >> 23) & 0x3u) == 0x3u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    if (!mmu_write(addr, reg32(rt), 4) || !mmu_write(addr + 4, reg32(rt2), 4)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  // STP Xt1, Xt2, [Xn|SP], #imm
  if ((insn & 0xFFC00000u) == 0xA8800000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = sp_or_reg(rn);
    if (!mmu_write(addr, reg(rt), 8) || !mmu_write(addr + 8, reg(rt2), 8)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  // STP Wt1, Wt2, [Xn|SP], #imm
  if ((insn & 0xFFC00000u) == 0x28800000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t addr = sp_or_reg(rn);
    if (!mmu_write(addr, reg32(rt), 4) || !mmu_write(addr + 4, reg32(rt2), 4)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  // STP/STNP Xt1, Xt2, [Xn|SP, #imm] (signed offset, no writeback)
  if ((insn & 0xFFC00000u) == 0xA9000000u || (insn & 0xFFC00000u) == 0xA8000000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    if (!mmu_write(addr, reg(rt), 8) || !mmu_write(addr + 8, reg(rt2), 8)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // STP/STNP Wt1, Wt2, [Xn|SP, #imm] (signed offset, no writeback)
  if ((insn & 0xFFC00000u) == 0x29000000u || (insn & 0xFFC00000u) == 0x28000000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    if (!mmu_write(addr, reg32(rt), 4) || !mmu_write(addr + 4, reg32(rt2), 4)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDP Xt1, Xt2, [Xn|SP, #imm]!
  if ((insn & 0xFFC00000u) == 0xA9C00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    const auto v1 = mmu_read(addr, 8);
    const auto v2 = mmu_read(addr + 8, 8);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, *v1);
    set_reg(rt2, *v2);
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  // LDP Wt1, Wt2, [Xn|SP, #imm]!
  if ((insn & 0xFFC00000u) == 0x29C00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    const auto v1 = mmu_read(addr, 4);
    const auto v2 = mmu_read(addr + 4, 4);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(*v1));
    set_reg32(rt2, static_cast<std::uint32_t>(*v2));
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  // LDP Xt1, Xt2, [Xn|SP], #imm
  if ((insn & 0xFFC00000u) == 0xA8C00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = sp_or_reg(rn);
    const auto v1 = mmu_read(addr, 8);
    const auto v2 = mmu_read(addr + 8, 8);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, *v1);
    set_reg(rt2, *v2);
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  // LDP Wt1, Wt2, [Xn|SP], #imm
  if ((insn & 0xFFC00000u) == 0x28C00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t addr = sp_or_reg(rn);
    const auto v1 = mmu_read(addr, 4);
    const auto v2 = mmu_read(addr + 4, 4);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(*v1));
    set_reg32(rt2, static_cast<std::uint32_t>(*v2));
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  // LDP/LDNP Xt1, Xt2, [Xn|SP, #imm] (signed offset, no writeback)
  if ((insn & 0xFFC00000u) == 0xA9400000u || (insn & 0xFFC00000u) == 0xA8400000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    const auto v1 = mmu_read(addr, 8);
    const auto v2 = mmu_read(addr + 8, 8);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, *v1);
    set_reg(rt2, *v2);
    return true;
  }

  // LDP/LDNP Wt1, Wt2, [Xn|SP, #imm] (signed offset, no writeback)
  if ((insn & 0xFFC00000u) == 0x29400000u || (insn & 0xFFC00000u) == 0x28400000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    const auto v1 = mmu_read(addr, 4);
    const auto v2 = mmu_read(addr + 4, 4);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(*v1));
    set_reg32(rt2, static_cast<std::uint32_t>(*v2));
    return true;
  }

  // LDPSW Xt1, Xt2, [Xn|SP, #imm]!
  if ((insn & 0xFFC00000u) == 0x69C00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    const auto v1 = mmu_read(addr, 4);
    const auto v2 = mmu_read(addr + 4, 4);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(sign_extend(*v1 & 0xFFFFFFFFu, 32)));
    set_reg(rt2, static_cast<std::uint64_t>(sign_extend(*v2 & 0xFFFFFFFFu, 32)));
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  // LDPSW Xt1, Xt2, [Xn|SP], #imm
  if ((insn & 0xFFC00000u) == 0x68C00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t addr = sp_or_reg(rn);
    const auto v1 = mmu_read(addr, 4);
    const auto v2 = mmu_read(addr + 4, 4);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(sign_extend(*v1 & 0xFFFFFFFFu, 32)));
    set_reg(rt2, static_cast<std::uint64_t>(sign_extend(*v2 & 0xFFFFFFFFu, 32)));
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  // LDPSW Xt1, Xt2, [Xn|SP, #imm] (signed offset, no writeback)
  if ((insn & 0xFFC00000u) == 0x69400000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 2u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    const auto v1 = mmu_read(addr, 4);
    const auto v2 = mmu_read(addr + 4, 4);
    if (!v1.has_value() || !v2.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(sign_extend(*v1 & 0xFFFFFFFFu, 32)));
    set_reg(rt2, static_cast<std::uint64_t>(sign_extend(*v2 & 0xFFFFFFFFu, 32)));
    return true;
  }

  // LDR Xt, [Xn, #imm12]
  if ((insn & 0xFFC00000u) == 0xF9400000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 3u);
    const auto value = mmu_read(addr, 8u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, *value);
    return true;
  }

  // STR Xt, [Xn, #imm12]
  if ((insn & 0xFFC00000u) == 0xF9000000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 3u);
    if (!mmu_write(addr, reg(rt), 8u)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDR Wt, [Xn, #imm12]
  if ((insn & 0xFFC00000u) == 0xB9400000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 2u);
    const auto value = mmu_read(addr, 4u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, *value & 0xFFFFFFFFu);
    return true;
  }

  // LDRSW Xt, [Xn|SP, #imm12] (unsigned offset, scaled by 4)
  if ((insn & 0xFFC00000u) == 0xB9800000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 2u);
    const auto value = mmu_read(addr, 4u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*value & 0xFFFFFFFFu))));
    return true;
  }

  // LDURSW Xt, [Xn|SP, #simm9] (unscaled)
  if ((insn & 0xFFC00C00u) == 0xB8800000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 4u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*value & 0xFFFFFFFFu))));
    return true;
  }

  // LDRSW Xt, [Xn|SP, #simm9]! (pre-index)
  if ((insn & 0xFFC00C00u) == 0xB8800C00u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 4u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*value & 0xFFFFFFFFu))));
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  // LDRSW Xt, [Xn|SP], #simm9 (post-index)
  if ((insn & 0xFFC00C00u) == 0xB8800400u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t base = sp_or_reg(rn);
    const auto value = mmu_read(base, 4);
    if (!value.has_value()) {
      data_abort(base);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*value & 0xFFFFFFFFu))));
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + simm9), false);
    return true;
  }

  // STR Wt, [Xn, #imm12]
  if ((insn & 0xFFC00000u) == 0xB9000000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 2u);
    if (!mmu_write(addr, reg(rt) & 0xFFFFFFFFu, 4u)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDRSB Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x39C00000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + imm12;
    const auto value = mmu_read(addr, 1u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(*value & 0xFFu))));
    return true;
  }

  // LDRSB Xt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x39800000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + imm12;
    const auto value = mmu_read(addr, 1u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int8_t>(*value & 0xFFu))));
    return true;
  }

  // LDRSH Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x79C00000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 1u);
    const auto value = mmu_read(addr, 2u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(*value & 0xFFFFu))));
    return true;
  }

  // LDRSH Xt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x79800000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 1u);
    const auto value = mmu_read(addr, 2u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int16_t>(*value & 0xFFFFu))));
    return true;
  }

  // LDRB Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x39400000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + imm12;
    const auto value = mmu_read(addr, 1u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint8_t>(*value));
    return true;
  }

  // STRB Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x39000000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + imm12;
    if (!mmu_write(addr, reg32(rt) & 0xFFu, 1u)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDRH Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x79400000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 1u);
    const auto value = mmu_read(addr, 2u);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg32(rt, static_cast<std::uint16_t>(*value));
    return true;
  }

  // STRH Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x79000000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 1u);
    if (!mmu_write(addr, reg32(rt) & 0xFFFFu, 2u)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  return false;
}

bool Cpu::condition_holds(std::uint32_t cond) const {
  const auto p = sysregs_.pstate();
  switch (cond & 0xFu) {
  case 0x0: return p.z;
  case 0x1: return !p.z;
  case 0x2: return p.c;
  case 0x3: return !p.c;
  case 0x4: return p.n;
  case 0x5: return !p.n;
  case 0x6: return p.v;
  case 0x7: return !p.v;
  case 0x8: return p.c && !p.z;
  case 0x9: return !p.c || p.z;
  case 0xA: return p.n == p.v;
  case 0xB: return p.n != p.v;
  case 0xC: return !p.z && (p.n == p.v);
  case 0xD: return p.z || (p.n != p.v);
  case 0xE: return true;
  default: return false;
  }
}

void Cpu::set_flags_add(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result, bool is_32bit) {
  auto p = sysregs_.pstate();
  const std::uint32_t sign_bit = is_32bit ? 31u : 63u;
  const std::uint64_t mask = is_32bit ? 0xFFFFFFFFull : ~0ull;
  const std::uint64_t l = lhs & mask;
  const std::uint64_t r = rhs & mask;
  const std::uint64_t res = result & mask;
  p.n = ((res >> sign_bit) & 1u) != 0;
  p.z = res == 0;
  p.c = is_32bit ? (static_cast<std::uint32_t>(res) < static_cast<std::uint32_t>(l)) : (res < l);
  const bool lhs_sign = ((l >> sign_bit) & 1u) != 0;
  const bool rhs_sign = ((r >> sign_bit) & 1u) != 0;
  const bool res_sign = ((res >> sign_bit) & 1u) != 0;
  p.v = (lhs_sign == rhs_sign) && (lhs_sign != res_sign);
  sysregs_.set_pstate(p);
}

void Cpu::set_flags_sub(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result, bool is_32bit) {
  auto p = sysregs_.pstate();
  const std::uint32_t sign_bit = is_32bit ? 31u : 63u;
  const std::uint64_t mask = is_32bit ? 0xFFFFFFFFull : ~0ull;
  const std::uint64_t l = lhs & mask;
  const std::uint64_t r = rhs & mask;
  const std::uint64_t res = result & mask;
  p.n = ((res >> sign_bit) & 1u) != 0;
  p.z = res == 0;
  p.c = is_32bit ? (static_cast<std::uint32_t>(l) >= static_cast<std::uint32_t>(r)) : (l >= r);
  const bool lhs_sign = ((l >> sign_bit) & 1u) != 0;
  const bool rhs_sign = ((r >> sign_bit) & 1u) != 0;
  const bool res_sign = ((res >> sign_bit) & 1u) != 0;
  p.v = (lhs_sign != rhs_sign) && (lhs_sign != res_sign);
  sysregs_.set_pstate(p);
}

void Cpu::set_flags_logic(std::uint64_t result, bool is_32bit) {
  const std::uint64_t masked = is_32bit ? (result & 0xFFFFFFFFu) : result;
  auto p = sysregs_.pstate();
  p.n = is_32bit ? (((masked >> 31) & 1u) != 0) : (((masked >> 63) & 1u) != 0);
  p.z = masked == 0;
  p.c = false;
  p.v = false;
  sysregs_.set_pstate(p);
}

std::int64_t Cpu::sign_extend(std::uint64_t value, std::uint32_t bits) {
  if (bits == 0u) {
    return 0;
  }
  if (bits >= 64u) {
    return static_cast<std::int64_t>(value);
  }
  const std::uint64_t sign = 1ull << (bits - 1);
  const std::uint64_t mask = ones(bits);
  value &= mask;
  return static_cast<std::int64_t>((value ^ sign) - sign);
}


bool Cpu::save_state(std::ostream& out) const {
  const auto write_translation_fault = [&](const TranslationFault& fault) {
    const std::uint8_t kind = static_cast<std::uint8_t>(fault.kind);
    return snapshot_io::write(out, kind) && snapshot_io::write(out, fault.level) && snapshot_io::write_bool(out, fault.write);
  };
  const auto write_tlb_entry = [&](const TlbEntry& entry) {
    return snapshot_io::write(out, entry.pa_page) &&
           snapshot_io::write(out, entry.asid) &&
           snapshot_io::write(out, entry.level) &&
           snapshot_io::write(out, entry.attr_index) &&
           snapshot_io::write(out, entry.mair_attr) &&
           snapshot_io::write_bool(out, entry.global_entry) &&
           snapshot_io::write_bool(out, entry.writable) &&
           snapshot_io::write_bool(out, entry.user_accessible) &&
           snapshot_io::write_bool(out, entry.executable) &&
           snapshot_io::write_bool(out, entry.pxn) &&
           snapshot_io::write_bool(out, entry.uxn) &&
           snapshot_io::write(out, static_cast<std::uint8_t>(entry.memory_type)) &&
           snapshot_io::write(out, static_cast<std::uint8_t>(entry.leaf_shareability)) &&
           snapshot_io::write(out, static_cast<std::uint8_t>(entry.walk_attrs.inner)) &&
           snapshot_io::write(out, static_cast<std::uint8_t>(entry.walk_attrs.outer)) &&
           snapshot_io::write(out, static_cast<std::uint8_t>(entry.walk_attrs.shareability)) &&
           snapshot_io::write(out, entry.walk_attrs.ips_bits);
  };

  if (!snapshot_io::write_array(out, regs_)) {
    return false;
  }
  for (const auto& qreg : qregs_) {
    if (!snapshot_io::write_array(out, qreg)) {
      return false;
    }
  }
  if (!sysregs_.save_state(out) ||
      !snapshot_io::write(out, exception_depth_) ||
      !snapshot_io::write_array(out, exception_is_irq_stack_) ||
      !snapshot_io::write_array(out, exception_intid_stack_) ||
      !snapshot_io::write_array(out, exception_prev_prio_stack_) ||
      !snapshot_io::write_array(out, exception_prio_dropped_stack_) ||
      !snapshot_io::write(out, manual_irq_depth_) ||
      !snapshot_io::write_array(out, manual_irq_intid_stack_) ||
      !snapshot_io::write_array(out, manual_irq_prev_prio_stack_) ||
      !snapshot_io::write_array(out, manual_irq_prio_dropped_stack_) ||
      !snapshot_io::write_bool(out, sync_reported_) ||
      !snapshot_io::write_bool(out, waiting_for_interrupt_) ||
      !snapshot_io::write_bool(out, waiting_for_event_) ||
      !snapshot_io::write_bool(out, event_register_) ||
      !snapshot_io::write(out, icc_pmr_el1_) ||
      !snapshot_io::write(out, running_priority_) ||
      !snapshot_io::write(out, icc_ctlr_el1_) ||
      !snapshot_io::write(out, icc_sre_el1_) ||
      !snapshot_io::write(out, icc_bpr1_el1_) ||
      !snapshot_io::write(out, icc_igrpen1_el1_) ||
      !snapshot_io::write_array(out, icc_ap0r_el1_) ||
      !snapshot_io::write_array(out, icc_ap1r_el1_) ||
      !snapshot_io::write(out, dcc_rx_data_) ||
      !snapshot_io::write(out, dcc_tx_data_) ||
      !snapshot_io::write(out, dcc_int_enable_) ||
      !snapshot_io::write_bool(out, dcc_rx_full_) ||
      !snapshot_io::write_bool(out, dcc_tx_full_) ||
      !snapshot_io::write_bool(out, exclusive_valid_) ||
      !snapshot_io::write(out, exclusive_addr_) ||
      !snapshot_io::write(out, exclusive_size_) ||
      !snapshot_io::write(out, exclusive_pa_count_) ||
      !snapshot_io::write_array(out, exclusive_phys_addrs_)) {
    return false;
  }
  std::uint64_t tlb_size = 0;
  for (const auto& set : tlb_entries_) {
    for (const auto& entry : set) {
      tlb_size += entry.valid ? 1u : 0u;
    }
  }
  if (!snapshot_io::write(out, tlb_size)) {
    return false;
  }
  for (const auto& set : tlb_entries_) {
    for (const auto& entry : set) {
      if (!entry.valid) {
        continue;
      }
      if (!snapshot_io::write(out, entry.va_page) || !write_tlb_entry(entry)) {
        return false;
      }
    }
  }
  const bool has_last_fault = last_translation_fault_.has_value();
  if (!snapshot_io::write_bool(out, has_last_fault)) {
    return false;
  }
  if (has_last_fault && !write_translation_fault(*last_translation_fault_)) {
    return false;
  }
  return snapshot_io::write(out, pc_) && snapshot_io::write(out, steps_) && snapshot_io::write_bool(out, halted_);
}

bool Cpu::load_state(std::istream& in, std::uint32_t version) {
  constexpr std::size_t kLegacyExceptionStackCapacity = 8;
  const auto read_translation_fault = [&](TranslationFault& fault) {
    std::uint8_t kind = 0;
    if (!snapshot_io::read(in, kind) || kind > static_cast<std::uint8_t>(TranslationFault::Kind::ExternalAbortOnWalk) ||
        !snapshot_io::read(in, fault.level) || !snapshot_io::read_bool(in, fault.write)) {
      return false;
    }
    fault.kind = static_cast<TranslationFault::Kind>(kind);
    return true;
  };
  const auto read_tlb_entry = [&](TlbEntry& entry) {
    std::uint8_t memory_type = 0;
    std::uint8_t leaf_shareability = 0;
    std::uint8_t walk_inner = 0;
    std::uint8_t walk_outer = 0;
    std::uint8_t walk_shareability = 0;
    if (!snapshot_io::read(in, entry.pa_page)) {
      return false;
    }
    if (version >= 7) {
      if (!snapshot_io::read(in, entry.asid) || !snapshot_io::read(in, entry.level) || !snapshot_io::read(in, entry.attr_index) || !snapshot_io::read(in, entry.mair_attr) ||
          !((version >= 22) ? snapshot_io::read_bool(in, entry.global_entry) : ((entry.global_entry = false), true)) ||
          !snapshot_io::read_bool(in, entry.writable) || !snapshot_io::read_bool(in, entry.user_accessible) || !snapshot_io::read_bool(in, entry.executable) || !snapshot_io::read_bool(in, entry.pxn) ||
          !snapshot_io::read_bool(in, entry.uxn) || !snapshot_io::read(in, memory_type) || !snapshot_io::read(in, leaf_shareability) ||
          !snapshot_io::read(in, walk_inner) || !snapshot_io::read(in, walk_outer) || !snapshot_io::read(in, walk_shareability) ||
          !snapshot_io::read(in, entry.walk_attrs.ips_bits)) {
        return false;
      }
    } else if (!snapshot_io::read(in, entry.level) || !snapshot_io::read(in, entry.attr_index) || !snapshot_io::read(in, entry.mair_attr) ||
        !snapshot_io::read_bool(in, entry.writable) || !snapshot_io::read_bool(in, entry.executable) || !snapshot_io::read_bool(in, entry.pxn) ||
        !snapshot_io::read_bool(in, entry.uxn) || !snapshot_io::read(in, memory_type) || !snapshot_io::read(in, leaf_shareability) ||
        !snapshot_io::read(in, walk_inner) || !snapshot_io::read(in, walk_outer) || !snapshot_io::read(in, walk_shareability) ||
        !snapshot_io::read(in, entry.walk_attrs.ips_bits)) {
      return false;
    }
    if (version < 7) {
      entry.asid = 0;
      entry.user_accessible = false;
    }
    if (version < 22) {
      entry.global_entry = false;
    }
    entry.memory_type = static_cast<MemoryType>(memory_type);
    entry.leaf_shareability = static_cast<Shareability>(leaf_shareability);
    entry.walk_attrs.inner = static_cast<Cacheability>(walk_inner);
    entry.walk_attrs.outer = static_cast<Cacheability>(walk_outer);
    entry.walk_attrs.shareability = static_cast<Shareability>(walk_shareability);
    return true;
  };

  if (!snapshot_io::read_array(in, regs_)) {
    return false;
  }
  for (auto& qreg : qregs_) {
    if (!snapshot_io::read_array(in, qreg)) {
      return false;
    }
  }
  irq_query_epoch_ = 0;
  irq_query_threshold_ = 0;
  irq_query_negative_valid_ = false;
  irq_delivery_threshold_ = 0xFF;
  exception_is_irq_stack_.fill(false);
  exception_intid_stack_.fill(0);
  exception_prev_prio_stack_.fill(0x100);
  exception_prio_dropped_stack_.fill(false);
  manual_irq_depth_ = 0;
  manual_irq_intid_stack_.fill(0);
  manual_irq_prev_prio_stack_.fill(0x100);
  manual_irq_prio_dropped_stack_.fill(false);
  if (!sysregs_.load_state(in, version) ||
      !snapshot_io::read(in, exception_depth_) ||
      exception_depth_ > exception_is_irq_stack_.size()) {
    return false;
  }
  if (version >= 12) {
    if (!snapshot_io::read_array(in, exception_is_irq_stack_) ||
        !snapshot_io::read_array(in, exception_intid_stack_)) {
      return false;
    }
    if (version >= 3) {
      if (!snapshot_io::read_array(in, exception_prev_prio_stack_) ||
          !snapshot_io::read_array(in, exception_prio_dropped_stack_)) {
        return false;
      }
    } else {
      exception_prev_prio_stack_.fill(0x100);
      exception_prio_dropped_stack_.fill(false);
    }
  } else {
    std::array<bool, kLegacyExceptionStackCapacity> legacy_is_irq{};
    std::array<std::uint32_t, kLegacyExceptionStackCapacity> legacy_intid{};
    if (exception_depth_ > legacy_is_irq.size() ||
        !snapshot_io::read_array(in, legacy_is_irq) ||
        !snapshot_io::read_array(in, legacy_intid)) {
      return false;
    }
    std::copy(legacy_is_irq.begin(), legacy_is_irq.end(), exception_is_irq_stack_.begin());
    std::copy(legacy_intid.begin(), legacy_intid.end(), exception_intid_stack_.begin());

    if (version >= 3) {
      std::array<std::uint16_t, kLegacyExceptionStackCapacity> legacy_prev_prio{};
      std::array<bool, kLegacyExceptionStackCapacity> legacy_prio_dropped{};
      if (!snapshot_io::read_array(in, legacy_prev_prio) ||
          !snapshot_io::read_array(in, legacy_prio_dropped)) {
        return false;
      }
      std::copy(legacy_prev_prio.begin(), legacy_prev_prio.end(), exception_prev_prio_stack_.begin());
      std::copy(legacy_prio_dropped.begin(), legacy_prio_dropped.end(), exception_prio_dropped_stack_.begin());
    } else {
      exception_prev_prio_stack_.fill(0x100);
      exception_prio_dropped_stack_.fill(false);
    }
  }
  if (version >= 21) {
    if (!snapshot_io::read(in, manual_irq_depth_) ||
        manual_irq_depth_ > manual_irq_intid_stack_.size() ||
        !snapshot_io::read_array(in, manual_irq_intid_stack_) ||
        !snapshot_io::read_array(in, manual_irq_prev_prio_stack_) ||
        !snapshot_io::read_array(in, manual_irq_prio_dropped_stack_)) {
      return false;
    }
  } else {
    manual_irq_depth_ = 0;
    manual_irq_intid_stack_.fill(0);
    manual_irq_prev_prio_stack_.fill(0x100);
    manual_irq_prio_dropped_stack_.fill(false);
  }
  if (!snapshot_io::read_bool(in, sync_reported_) ||
      !snapshot_io::read_bool(in, waiting_for_interrupt_) ||
      !snapshot_io::read_bool(in, waiting_for_event_) ||
      !snapshot_io::read_bool(in, event_register_) ||
      !snapshot_io::read(in, icc_pmr_el1_)) {
    return false;
  }
  if (version >= 3) {
    if (!snapshot_io::read(in, running_priority_)) {
      return false;
    }
  } else {
    running_priority_ = 0x100;
  }
  refresh_irq_threshold_cache();
  clear_exclusive_monitor();
  if (!snapshot_io::read(in, icc_ctlr_el1_) ||
      !snapshot_io::read(in, icc_sre_el1_) ||
      !snapshot_io::read(in, icc_bpr1_el1_) ||
      !snapshot_io::read(in, icc_igrpen1_el1_) ||
      !snapshot_io::read_array(in, icc_ap0r_el1_) ||
      !snapshot_io::read_array(in, icc_ap1r_el1_)) {
    return false;
  }
  if (version >= 17) {
    if (!snapshot_io::read(in, dcc_rx_data_) ||
        !snapshot_io::read(in, dcc_tx_data_) ||
        !((version >= 20) ? snapshot_io::read(in, dcc_int_enable_) : ((dcc_int_enable_ = 0), true)) ||
        !snapshot_io::read_bool(in, dcc_rx_full_) ||
        !snapshot_io::read_bool(in, dcc_tx_full_)) {
      return false;
    }
  } else {
    dcc_rx_data_ = 0;
    dcc_tx_data_ = 0;
    dcc_int_enable_ = 0;
    dcc_rx_full_ = false;
    dcc_tx_full_ = false;
  }
  if (!snapshot_io::read_bool(in, exclusive_valid_) ||
      !snapshot_io::read(in, exclusive_addr_) ||
      !snapshot_io::read(in, exclusive_size_)) {
    return false;
  }
  if (version >= 14) {
    if (!snapshot_io::read(in, exclusive_pa_count_) ||
        !snapshot_io::read_array(in, exclusive_phys_addrs_) ||
        exclusive_pa_count_ > exclusive_phys_addrs_.size()) {
      return false;
    }
    if (!exclusive_valid_) {
      clear_exclusive_monitor();
    }
  } else {
    clear_exclusive_monitor();
  }
  std::uint64_t tlb_size = 0;
  if (!snapshot_io::read(in, tlb_size) || tlb_size > (1ull << 20)) {
    return false;
  }
  tlb_flush_all();
  invalidate_decode_all();
  invalidate_ram_page_caches();
  for (std::uint64_t i = 0; i < tlb_size; ++i) {
    std::uint64_t va_page = 0;
    TlbEntry entry{};
    if (!snapshot_io::read(in, va_page) || !read_tlb_entry(entry)) {
      return false;
    }
    tlb_insert_entry(va_page, entry);
  }
  bool has_last_fault = false;
  if (!snapshot_io::read_bool(in, has_last_fault)) {
    return false;
  }
  if (has_last_fault) {
    TranslationFault fault{};
    if (!read_translation_fault(fault)) {
      return false;
    }
    last_translation_fault_ = fault;
  } else {
    last_translation_fault_.reset();
  }
  last_data_abort_iss_override_.reset();
  last_data_fault_va_.reset();
  if (!snapshot_io::read(in, pc_) || !snapshot_io::read(in, steps_) || !snapshot_io::read_bool(in, halted_)) {
    return false;
  }
  parse_pc_watch_list();
  trace_va_hit_ = false;
  trace_svc_count_ = 0;
  trace_eret_lower_count_ = 0;
  trace_lower_sync_count_ = 0;
  for (auto& [_, hit] : pc_watch_hits_) {
    hit = false;
  }
  return true;
}


} // namespace aarchvm
