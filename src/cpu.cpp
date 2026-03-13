#include "aarchvm/cpu.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace aarchvm {

namespace {

bool irq_take_trace_enabled() {
  return std::getenv("AARCHVM_TRACE_IRQ_TAKE") != nullptr;
}

bool branch_zero_trace_enabled() {
  return std::getenv("AARCHVM_TRACE_BRANCH_ZERO") != nullptr;
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
constexpr std::uint64_t kFpsrIxc = 1ull << 4;

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

} // namespace

Cpu::Cpu(Bus& bus, GicV3& gic, GenericTimer& timer) : bus_(bus), gic_(gic), timer_(timer) {}

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
  event_register_ = false;
  icc_pmr_el1_ = 0xFF;
  running_priority_ = 0x100;
  icc_ctlr_el1_ = 0;
  icc_sre_el1_ = 0;
  icc_bpr1_el1_ = 0;
  icc_igrpen1_el1_ = 0;
  icc_ap0r_el1_.fill(0);
  icc_ap1r_el1_.fill(0);
  exclusive_valid_ = false;
  exclusive_addr_ = 0;
  exclusive_size_ = 0;
  tlb_flush_all();
  last_translation_fault_.reset();
  pc_ = pc;
  steps_ = 0;
  halted_ = false;
}

void Cpu::set_cntvct(std::uint64_t value) {
  sysregs_.set_cntvct(value);
}

void Cpu::perf_flush_tlb_all() {
  tlb_flush_all();
}

bool Cpu::step() {
  if (halted_) {
    return false;
  }

  if (waiting_for_interrupt_) {
    if (gic_.has_pending()) {
      waiting_for_interrupt_ = false;
      if (try_take_irq()) {
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
    } else if (try_take_irq()) {
      waiting_for_event_ = false;
      ++steps_;
      return true;
    }
    ++steps_;
    return true;
  }

  if (try_take_irq()) {
    ++steps_;
    return true;
  }

  if (pc_watch_enabled_) {
    auto pc_watch = pc_watch_hits_.find(pc_);
    if (pc_watch != pc_watch_hits_.end() && !pc_watch->second) {
    std::cerr << "PC-HIT pc=0x" << std::hex << pc_
              << " steps=" << std::dec << steps_
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

  const auto fetch_result = translate_address(pc_, AccessType::Fetch, true);
  if (!fetch_result.has_value()) {
    const std::uint32_t iss = last_translation_fault_.has_value() ? fault_status_code(*last_translation_fault_) : 0u;
    enter_sync_exception(pc_, sysregs_.in_el0() ? 0x20u : 0x21u, iss, true, pc_);
    return true;
  }

  const auto fetch = bus_.read(fetch_result->pa, 4);
  if (!fetch.has_value()) {
    enter_sync_exception(pc_, sysregs_.in_el0() ? 0x20u : 0x21u, 0u, true, pc_);
    return true;
  }

  const std::uint32_t insn = static_cast<std::uint32_t>(*fetch);
  const std::uint64_t this_pc = pc_;
  pc_ += 4;
  ++steps_;

  if (insn == 0xD503201Fu) {
    return true; // NOP
  }
  if ((insn & 0xFFFFFC1Fu) == 0xD65F0000u) {
    const std::uint32_t rn = (insn >> 5) & 0x1Fu;
    const std::uint64_t target = reg(rn);
    if (branch_zero_trace_enabled() && target == 0) {
      std::cerr << std::dec << "BRANCH-ZERO kind=RET rn=" << rn
                << " from=0x" << std::hex << this_pc
                << " el=" << std::dec << static_cast<std::uint32_t>(sysregs_.current_el())
                << " depth=" << exception_depth_
                << " x30=0x" << std::hex << reg(30)
                << " sp=0x" << regs_[31]
                << " steps=" << std::dec << steps_ << '\n';
    }
    pc_ = target; // RET Xn
    return true;
  }
  if ((insn & 0xFFFFFC1Fu) == 0xD61F0000u) {
    const std::uint32_t rn = (insn >> 5) & 0x1Fu;
    const std::uint64_t target = reg(rn);
    if (branch_zero_trace_enabled() && target == 0) {
      std::cerr << std::dec << "BRANCH-ZERO kind=BR rn=" << rn
                << " from=0x" << std::hex << this_pc
                << " el=" << std::dec << static_cast<std::uint32_t>(sysregs_.current_el())
                << " depth=" << exception_depth_
                << " x30=0x" << std::hex << reg(30)
                << " sp=0x" << regs_[31]
                << " steps=" << std::dec << steps_ << '\n';
    }
    pc_ = target; // BR Xn
    return true;
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
                << " x30=0x" << std::hex << reg(30)
                << " sp=0x" << regs_[31]
                << " steps=" << std::dec << steps_ << '\n';
    }
    pc_ = target;
    return true;
  }
  if (insn == 0xD69F03E0u) { // ERET
    save_current_sp_to_bank();
    pc_ = sysregs_.exception_return();
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
      }
      --exception_depth_;
    }
    return true;
  }
  if ((insn & 0xFFE0001Fu) == 0xD4200000u) {
    if (trace_brk_) {
      std::cerr << "BRK: pc=0x" << std::hex << this_pc;
      for (std::uint32_t i = 0; i < 31; ++i) {
        std::cerr << " x" << i << "=0x" << regs_[i];
      }
      std::cerr << " sp=0x" << regs_[31];
      const auto x6_pa = translate_address(regs_[6], AccessType::Read, false, false);
      if (x6_pa.has_value()) {
        std::cerr << " x6_pa=0x" << x6_pa->pa;
      }
      const auto x12_pa = translate_address(regs_[12], AccessType::Read, false, false);
      if (x12_pa.has_value()) {
        std::cerr << " x12_pa=0x" << x12_pa->pa;
      }
      std::cerr << std::dec << '\n';
    }
    halted_ = true; // BRK
    return false;
  }
  if ((insn & 0xFFE0001Fu) == 0xD4000001u) { // SVC #imm16
    enter_sync_exception(this_pc, 0x15u, (insn >> 5) & 0xFFFFu, false, 0);
    return true;
  }

  if (exec_branch(insn)) {
    return true;
  }
  if (exec_system(insn)) {
    return true;
  }
  if (exec_data_processing(insn)) {
    return true;
  }
  if (exec_load_store(insn)) {
    return true;
  }

  if (trace_exceptions_) {
    std::cerr << "UNIMPL: pc=0x" << std::hex << this_pc
              << " insn=0x" << insn
              << " pstate=0x" << sysregs_.pstate_bits()
              << " sp=0x" << regs_[31]
              << std::dec << '\n';
  }
  enter_sync_exception(this_pc, 0x00u, 0u, false, 0);
  return true;
}

bool Cpu::try_take_irq() {
  if (sysregs_.irq_masked() || icc_igrpen1_el1_ == 0) {
    return false;
  }
  const std::uint16_t irq_threshold = std::min<std::uint16_t>(static_cast<std::uint16_t>(icc_pmr_el1_ & 0xFFu), running_priority_);
  const std::uint64_t irq_epoch = gic_.state_epoch();
  if (irq_query_negative_valid_ && irq_query_epoch_ == irq_epoch && irq_query_threshold_ == irq_threshold) {
    return false;
  }
  const auto intid = gic_.acknowledge(static_cast<std::uint8_t>(irq_threshold & 0xFFu));
  if (!intid.has_value()) {
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
  exception_intid_stack_[exception_depth_] = *intid;
  exception_prev_prio_stack_[exception_depth_] = running_priority_;
  exception_prio_dropped_stack_[exception_depth_] = false;
  const std::uint8_t new_prio = gic_.priority(*intid);
  running_priority_ = new_prio;
  if (irq_take_trace_enabled()) {
    std::cerr << std::dec << "IRQ-TAKE intid=" << *intid
              << " prio=0x" << std::hex << static_cast<std::uint32_t>(new_prio)
              << " pmr=0x" << static_cast<std::uint32_t>(icc_pmr_el1_ & 0xFFu)
              << " depth=" << std::dec << (exception_depth_ + 1)
              << " pc=0x" << std::hex << pc_ << '\n';
  }
  ++exception_depth_;
  sysregs_.exception_enter_irq(pc_);
  load_current_sp_from_bank();
  pc_ = sysregs_.vbar_el1() + (from_lower_el ? 0x480u : (from_spx ? 0x280u : 0x80u));
  return true;
}

void Cpu::enter_sync_exception(std::uint64_t fault_pc,
                               std::uint32_t ec,
                               std::uint32_t iss,
                               bool far_valid,
                               std::uint64_t far) {
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
    const auto trace_fetch = translate_address(fault_pc, AccessType::Fetch, false, false);
    if (trace_fetch.has_value()) {
      const auto trace_insn = bus_.read(trace_fetch->pa, 4);
      if (trace_insn.has_value()) {
        std::cerr << " insn=0x" << (*trace_insn & 0xFFFFFFFFu);
      }
    }
    std::cerr << std::dec << '\n';
    sync_reported_ = true;
  }
  sysregs_.exception_enter_sync(return_pc, ec, iss, far_valid, far);
  load_current_sp_from_bank();
  pc_ = sysregs_.vbar_el1() + (from_lower_el ? 0x400u : (from_spx ? 0x200u : 0x0u));
}

std::optional<Cpu::TranslationResult> Cpu::translate_address(std::uint64_t va,
                                                             AccessType access,
                                                             bool allow_tlb_fill,
                                                             bool use_tlb) {
  ++perf_counters_.translate_calls;
  last_translation_fault_.reset();

  if (!sysregs_.mmu_enabled()) {
    TranslationResult result{};
    result.pa = va;
    result.level = 3;
    result.attr_index = 0;
    result.mair_attr = 0;
    result.writable = true;
    result.executable = true;
    result.pxn = false;
    result.uxn = false;
    result.memory_type = MemoryType::Normal;
    result.leaf_shareability = Shareability::InnerShareable;
    result.walk_attrs = decode_walk_attributes(false);
    return result;
  }

  const std::uint64_t page = (va >> 12) & tlb_page_mask();
  const std::uint64_t off = va & 0xFFFull;

  if (use_tlb) {
    const TlbEntry* hit = nullptr;
    TlbEntry* hot = (access == AccessType::Fetch) ? &tlb_last_fetch_ : &tlb_last_data_;
    if (hot->valid && hot->va_page == page) {
      hit = hot;
    } else {
      ++perf_counters_.tlb_lookups;
      hit = tlb_lookup(page);
      if (hit != nullptr) {
        *hot = *hit;
      }
    }
    if (hit != nullptr) {
      ++perf_counters_.tlb_hits;
      TranslationResult result{};
      result.pa = (hit->pa_page << 12) | off;
      result.level = hit->level;
      result.attr_index = hit->attr_index;
      result.mair_attr = hit->mair_attr;
      result.writable = hit->writable;
      result.executable = hit->executable;
      result.pxn = hit->pxn;
      result.uxn = hit->uxn;
      result.memory_type = hit->memory_type;
      result.leaf_shareability = hit->leaf_shareability;
      result.walk_attrs = hit->walk_attrs;
      TranslationFault fault{};
      if (!access_permitted(result, access, result.level, &fault)) {
        last_translation_fault_ = fault;
        return std::nullopt;
      }
      return result;
    }
    ++perf_counters_.tlb_misses;
  }

  TranslationFault fault{};
  const auto result = walk_page_tables(va, access, &fault);
  if (!result.has_value()) {
    last_translation_fault_ = fault;
    return std::nullopt;
  }

  if (allow_tlb_fill) {
    tlb_insert(page, *result);
  }
  return result;
}

std::optional<Cpu::TranslationResult> Cpu::walk_page_tables(std::uint64_t va,
                                                            AccessType access,
                                                            TranslationFault* fault) {
  ++perf_counters_.page_walks;
  const bool trace_va = trace_va_.has_value() && *trace_va_ == va && !trace_va_hit_;
  const auto trace_va_log = [&](const std::string& msg) {
    if (trace_va && trace_exceptions_) {
      std::cerr << msg << std::dec << '\n';
    }
  };
  const std::uint64_t tcr = sysregs_.tcr_el1();
  const bool va_upper = (va >> 63) != 0;
  const WalkAttributes walk_attrs = decode_walk_attributes(va_upper);

  const std::uint32_t txsz = va_upper
      ? static_cast<std::uint32_t>((tcr >> 16) & 0x3Fu)
      : static_cast<std::uint32_t>(tcr & 0x3Fu);
  if (txsz > 39) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = access == AccessType::Write};
    }
    return std::nullopt;
  }

  const std::uint32_t va_bits = 64u - txsz;
  if (va_bits < 12u || va_bits > 48u) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = access == AccessType::Write};
    }
    return std::nullopt;
  }

  const std::uint32_t tg = va_upper
      ? static_cast<std::uint32_t>((tcr >> 30) & 0x3u)
      : static_cast<std::uint32_t>((tcr >> 14) & 0x3u);
  // 4KB granule only: TG0==00, TG1==10.
  if ((!va_upper && tg != 0u) || (va_upper && tg != 2u)) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::Translation, .level = 0, .write = access == AccessType::Write};
    }
    return std::nullopt;
  }

  const bool epd = va_upper ? (((tcr >> 23) & 0x1u) != 0) : (((tcr >> 7) & 0x1u) != 0);
  if (epd) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::Translation, .level = 0, .write = access == AccessType::Write};
    }
    return std::nullopt;
  }

  const std::uint64_t low_limit = (va_bits == 64u) ? ~0ull : ((1ull << va_bits) - 1ull);
  if (!va_upper && va > low_limit) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = access == AccessType::Write};
    }
    return std::nullopt;
  }
  if (va_upper && va_bits < 64u) {
    const std::uint64_t upper_tag_mask = ~low_limit;
    if ((va & upper_tag_mask) != upper_tag_mask) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = access == AccessType::Write};
      }
      return std::nullopt;
    }
  }

  std::uint64_t table_base = va_upper ? sysregs_.ttbr1_el1() : sysregs_.ttbr0_el1();
  table_base &= 0x0000FFFFFFFFF000ull;
  if (!pa_within_ips(table_base, walk_attrs.ips_bits)) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize, .level = 0, .write = access == AccessType::Write};
    }
    return std::nullopt;
  }

  const std::uint32_t levels = (va_bits <= 12u) ? 1u : ((va_bits - 12u + 8u) / 9u);
  const std::uint32_t start_level = 4u - levels;
  if (trace_va) {
    trace_va_hit_ = true;
    std::ostringstream oss;
    oss << "TRACE-VA: va=0x" << std::hex << va
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
      *fault = TranslationFault{.kind = TranslationFault::Kind::Translation, .level = 0, .write = access == AccessType::Write};
    }
    return std::nullopt;
  }

  const std::uint64_t idx[4] = {
      (va >> 39) & 0x1FFu,
      (va >> 30) & 0x1FFu,
      (va >> 21) & 0x1FFu,
      (va >> 12) & 0x1FFu,
  };

  TableAttrs inherited{};
  for (std::uint32_t level = start_level; level < 4u; ++level) {
    const std::uint64_t desc_addr = table_base + idx[level] * 8u;
    ++perf_counters_.page_walk_desc_reads;
    const auto desc_opt = bus_.read(desc_addr, 8);
    if (trace_va) {
      std::ostringstream oss;
      oss << "TRACE-VA-L" << level
          << ": idx=0x" << std::hex << idx[level]
          << " table_base=0x" << table_base
          << " desc_addr=0x" << desc_addr;
      if (desc_opt.has_value()) {
        oss << " desc=0x" << *desc_opt;
      } else {
        oss << " desc=<bus-fail>";
      }
      trace_va_log(oss.str());
    }
    if (!desc_opt.has_value()) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::Translation,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = access == AccessType::Write};
      }
      return std::nullopt;
    }

    const std::uint64_t desc = *desc_opt;
    const bool valid = (desc & 1u) != 0;
    const bool bit1 = (desc & 2u) != 0;
    if (!valid) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::Translation,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = access == AccessType::Write};
      }
      return std::nullopt;
    }

    if (level == 3u) {
      if (!bit1) {
        if (fault != nullptr) {
          *fault = TranslationFault{.kind = TranslationFault::Kind::Translation,
                                    .level = static_cast<std::uint8_t>(level),
                                    .write = access == AccessType::Write};
        }
        return std::nullopt;
      }

      const bool af = ((desc >> 10) & 0x1u) != 0;
      const bool leaf_ro = ((desc >> 7) & 0x1u) != 0;
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
                                    .write = access == AccessType::Write};
        }
        return std::nullopt;
      }

      TranslationResult result{};
      result.pa = page_base | (va & 0xFFFull);
      result.level = static_cast<std::uint8_t>(level);
      result.attr_index = attr_index;
      result.mair_attr = mair_attr;
      result.writable = !(inherited.write_protect || (leaf_ro && !dbm));
      result.pxn = pxn || inherited.pxn;
      result.uxn = uxn || inherited.uxn;
      result.executable = !result.pxn;
      result.memory_type = decode_memory_type(mair_attr);
      result.leaf_shareability = static_cast<Shareability>((desc >> 8) & 0x3u);
      result.walk_attrs = walk_attrs;
      if (!af) {
        if (fault != nullptr) {
          *fault = TranslationFault{.kind = TranslationFault::Kind::AccessFlag,
                                    .level = static_cast<std::uint8_t>(level),
                                    .write = access == AccessType::Write};
        }
        return std::nullopt;
      }
      if (!access_permitted(result, access, static_cast<std::uint8_t>(level), fault)) {
        return std::nullopt;
      }
      return result;
    }

    if (bit1) {
      inherited.write_protect = inherited.write_protect || (((desc >> 62) & 0x1u) != 0);
      inherited.uxn = inherited.uxn || (((desc >> 60) & 0x1u) != 0);
      inherited.pxn = inherited.pxn || (((desc >> 59) & 0x1u) != 0);
      table_base = desc & 0x0000FFFFFFFFF000ull;
      if (!pa_within_ips(table_base, walk_attrs.ips_bits)) {
        if (fault != nullptr) {
          *fault = TranslationFault{.kind = TranslationFault::Kind::AddressSize,
                                    .level = static_cast<std::uint8_t>(level),
                                    .write = access == AccessType::Write};
        }
        return std::nullopt;
      }
      continue;
    }

    if (level == 0u) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::Translation,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = access == AccessType::Write};
      }
      return std::nullopt;
    }

    const bool af = ((desc >> 10) & 0x1u) != 0;
    const bool leaf_ro = ((desc >> 7) & 0x1u) != 0;
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
                                  .write = access == AccessType::Write};
      }
      return std::nullopt;
    }

    TranslationResult result{};
    result.pa = pa_base | (va & block_mask);
    result.level = static_cast<std::uint8_t>(level);
    result.attr_index = attr_index;
    result.mair_attr = mair_attr;
    result.writable = !(inherited.write_protect || (leaf_ro && !dbm));
    result.pxn = pxn || inherited.pxn;
    result.uxn = uxn || inherited.uxn;
    result.executable = !result.pxn;
    result.memory_type = decode_memory_type(mair_attr);
    result.leaf_shareability = static_cast<Shareability>((desc >> 8) & 0x3u);
    result.walk_attrs = walk_attrs;
    if (!af) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::AccessFlag,
                                  .level = static_cast<std::uint8_t>(level),
                                  .write = access == AccessType::Write};
      }
      return std::nullopt;
    }
    if (!access_permitted(result, access, static_cast<std::uint8_t>(level), fault)) {
      if (trace_va) {
        trace_va_hit_ = true;
      }
      return std::nullopt;
    }
    if (trace_va) {
      trace_va_hit_ = true;
    }
    return result;
  }

  if (trace_va) {
    trace_va_hit_ = true;
  }
  return std::nullopt;
}

bool Cpu::access_permitted(const TranslationResult& result,
                           AccessType access,
                           std::uint8_t level,
                           TranslationFault* fault) const {
  if (access == AccessType::Write && !result.writable) {
    if (fault != nullptr) {
      *fault = TranslationFault{.kind = TranslationFault::Kind::Permission, .level = level, .write = true};
    }
    return false;
  }
  if (access == AccessType::Fetch) {
    const bool execute_blocked = sysregs_.in_el0() ? result.uxn : result.pxn;
    if (execute_blocked) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::Permission, .level = level, .write = false};
      }
      return false;
    }
  }
  return true;
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
  }
  return fsc | (fault.write ? (1u << 6) : 0u);
}

void Cpu::set_par_el1_for_fault(const TranslationFault& fault) {
  sysregs_.set_par_el1(1ull | (static_cast<std::uint64_t>(fault_status_code(fault) & 0x7Fu) << 1));
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

const Cpu::TlbEntry* Cpu::tlb_lookup(std::uint64_t va_page) const {
  const auto& set = tlb_entries_[tlb_set_index(va_page)];
  for (const TlbEntry& entry : set) {
    if (entry.valid && entry.va_page == va_page) {
      return &entry;
    }
  }
  return nullptr;
}

void Cpu::tlb_insert(std::uint64_t va_page, const TranslationResult& result) {
  ++perf_counters_.tlb_inserts;
  TlbEntry entry{};
  entry.valid = true;
  entry.va_page = va_page;
  entry.pa_page = result.pa >> 12;
  entry.level = result.level;
  entry.attr_index = result.attr_index;
  entry.mair_attr = result.mair_attr;
  entry.writable = result.writable;
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
    if (slot.valid && slot.va_page == va_page) {
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

void Cpu::tlb_invalidate_page(std::uint64_t va_page) {
  if (tlb_last_fetch_.valid && tlb_last_fetch_.va_page == va_page) {
    tlb_last_fetch_.valid = false;
  }
  if (tlb_last_data_.valid && tlb_last_data_.va_page == va_page) {
    tlb_last_data_.valid = false;
  }
  auto& set = tlb_entries_[tlb_set_index(va_page)];
  for (TlbEntry& entry : set) {
    if (entry.valid && entry.va_page == va_page) {
      entry.valid = false;
      return;
    }
  }
}
void Cpu::tlb_flush_va(std::uint64_t va) {
  ++perf_counters_.tlb_flush_va;
  // Linux feeds TLBI-by-VA operands in architected VA>>12 form, while some
  // local tests historically passed raw VAs. Accept both encodings here.
  tlb_invalidate_page((va >> 12) & tlb_page_mask());
  tlb_invalidate_page(va & tlb_page_mask());
}

std::uint64_t Cpu::reg(std::uint32_t idx) const {
  if (idx >= 31) {
    return 0;
  }
  return regs_[idx];
}

std::uint32_t Cpu::reg32(std::uint32_t idx) const {
  return static_cast<std::uint32_t>(reg(idx) & 0xFFFFFFFFu);
}

void Cpu::set_reg(std::uint32_t idx, std::uint64_t value) {
  if (idx >= 31) {
    return;
  }
  regs_[idx] = value;
}

void Cpu::set_reg32(std::uint32_t idx, std::uint32_t value) {
  set_reg(idx, value);
}

std::uint64_t Cpu::sp_or_reg(std::uint32_t idx) const {
  if (idx == 31) {
    return regs_[31];
  }
  return reg(idx);
}

void Cpu::set_sp_or_reg(std::uint32_t idx, std::uint64_t value, bool is_32bit) {
  if (idx == 31) {
    regs_[31] = is_32bit ? static_cast<std::uint32_t>(value) : value;
    return;
  }
  if (is_32bit) {
    set_reg32(idx, static_cast<std::uint32_t>(value));
  } else {
    set_reg(idx, value);
  }
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
  // but otherwise treat hint-space operations such as BTI/PAC/AUT as no-ops
  // for the current minimal Linux bring-up model.
  if ((insn & 0xFFFFF01Fu) == 0xD503201Fu) {
    switch ((insn >> 5) & 0x7Fu) {
    case 0x02u: // WFE
      if (event_register_) {
        event_register_ = false;
      } else {
        waiting_for_event_ = true;
      }
      return true;
    case 0x03u: // WFI
      waiting_for_interrupt_ = true;
      return true;
    case 0x04u: // SEV
    case 0x05u: // SEVL
      event_register_ = true;
      return true;
    default:
      return true;
    }
  }

  // DMB <option>
  if ((insn & 0xFFFFF0FFu) == 0xD50330BFu) {
    return true;
  }

  // DSB <option>
  if ((insn & 0xFFFFF0FFu) == 0xD503309Fu) {
    return true;
  }

  // ISB <option>
  if ((insn & 0xFFFFF0FFu) == 0xD50330DFu) {
    return true;
  }

  // CLREX <imm4>
  if ((insn & 0xFFFFF0FFu) == 0xD503305Fu) {
    exclusive_valid_ = false;
    exclusive_addr_ = 0;
    exclusive_size_ = 0;
    return true;
  }

  // TLBI VMALLE1 / VMALLE1IS
  if (insn == 0xD508871Fu || insn == 0xD508831Fu) {
    tlb_flush_all();
    return true;
  }

  // TLBI VAE1/VAE1IS/VALE1/VALE1IS/VAAE1/VAAE1IS/VAALE1/VAALE1IS, Xt.
  // ASID tagging is not modelled yet, so the ASID-aware variants collapse to
  // the same per-VA invalidation behaviour.
  if ((insn & 0xFFFFFFE0u) == 0xD5088720u ||
      (insn & 0xFFFFFFE0u) == 0xD5088320u ||
      (insn & 0xFFFFFFE0u) == 0xD50887A0u ||
      (insn & 0xFFFFFFE0u) == 0xD50883A0u ||
      (insn & 0xFFFFFFE0u) == 0xD5088760u ||
      (insn & 0xFFFFFFE0u) == 0xD5088360u ||
      (insn & 0xFFFFFFE0u) == 0xD50887E0u ||
      (insn & 0xFFFFFFE0u) == 0xD50883E0u) {
    const std::uint32_t rt = insn & 0x1Fu;
    tlb_flush_va(reg(rt));
    return true;
  }

  // TLBI ASIDE1 / ASIDE1IS, Xt.
  // ASID tagging is not modelled yet, so invalidate the whole software TLB.
  if ((insn & 0xFFFFFFE0u) == 0xD5088740u ||
      (insn & 0xFFFFFFE0u) == 0xD5088340u) {
    tlb_flush_all();
    return true;
  }

  // IC IALLU
  if (insn == 0xD508751Fu) {
    return true;
  }

  // IC IALLUIS
  if (insn == 0xD508711Fu) {
    return true;
  }

  // IC IVAU, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7520u) {
    return true;
  }

  // DC IVAC, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087620u) {
    return true;
  }

  // DC CVAC, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7A20u) {
    return true;
  }

  // DC CVAU / CVAP / CVADP, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7B20u ||
      (insn & 0xFFFFFFE0u) == 0xD50B7C20u ||
      (insn & 0xFFFFFFE0u) == 0xD50B7D20u) {
    return true;
  }

  // DC ZVA, Xt.
  // Model a 64-byte zeroing block, matching DCZID_EL0 BS=4.
  if ((insn & 0xFFFFFFE0u) == 0xD50B7420u) {
    const std::uint32_t rt = insn & 0x1Fu;
    const std::uint64_t base = reg(rt) & ~0x3Full;
    for (std::uint64_t off = 0; off < 64u; off += 8u) {
      const std::uint64_t va = base + off;
      const auto result = translate_address(va, AccessType::Write, true);
      if (!result.has_value() || !bus_.write(result->pa, 0, 8)) {
        const std::uint32_t iss = last_translation_fault_.has_value() ? fault_status_code(*last_translation_fault_) : 0u;
        enter_sync_exception(pc_ - 4, sysregs_.in_el0() ? 0x24u : 0x25u, iss, true, va);
        return true;
      }
    }
    exclusive_valid_ = false;
    exclusive_addr_ = 0;
    exclusive_size_ = 0;
    return true;
  }

  // DC CIVAC, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD50B7E20u) {
    return true;
  }

  // DC ISW, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087640u) {
    return true;
  }

  // DC CISW, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087E40u) {
    return true;
  }

  // AT S1E1R, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087800u) {
    const std::uint32_t rt = insn & 0x1Fu;
    const auto result = translate_address(reg(rt), AccessType::Read, false, false);
    if (result.has_value()) {
      sysregs_.set_par_el1(result->pa & 0x0000FFFFFFFFF000ull);
    } else if (last_translation_fault_.has_value()) {
      set_par_el1_for_fault(*last_translation_fault_);
    } else {
      sysregs_.set_par_el1(1ull);
    }
    return true;
  }

  // AT S1E1W, Xt
  if ((insn & 0xFFFFFFE0u) == 0xD5087820u) {
    const std::uint32_t rt = insn & 0x1Fu;
    const auto result = translate_address(reg(rt), AccessType::Write, false, false);
    if (result.has_value()) {
      sysregs_.set_par_el1(result->pa & 0x0000FFFFFFFFF000ull);
    } else if (last_translation_fault_.has_value()) {
      set_par_el1_for_fault(*last_translation_fault_);
    } else {
      sysregs_.set_par_el1(1ull);
    }
    return true;
  }
  // MSR SPSel, #imm
  if ((insn & 0xFFFFF0FFu) == 0xD50040BFu) {
    sysregs_.set_spsel((insn >> 8) & 0x1u);
    return true;
  }

  // MSR DAIFSet, #imm4
  if ((insn & 0xFFFFF0FFu) == 0xD50340DFu) {
    sysregs_.daif_set(static_cast<std::uint8_t>((insn >> 8) & 0xFu));
    return true;
  }

  // MSR DAIFClr, #imm4
  if ((insn & 0xFFFFF0FFu) == 0xD50340FFu) {
    sysregs_.daif_clr(static_cast<std::uint8_t>((insn >> 8) & 0xFu));
    return true;
  }

  // MSR PAN, #imm
  if ((insn & 0xFFFFF0FFu) == 0xD500409Fu) {
    sysregs_.set_pan(((insn >> 8) & 0x1u) != 0);
    return true;
  }

  // MRS Xt, sysreg
  if ((insn & 0xFFE00000u) == 0xD5200000u) {
    const std::uint32_t rt = insn & 0x1Fu;
    const std::uint32_t op0 = (insn >> 19) & 0x3u;
    const std::uint32_t op1 = (insn >> 16) & 0x7u;
    const std::uint32_t crn = (insn >> 12) & 0xFu;
    const std::uint32_t crm = (insn >> 8) & 0xFu;
    const std::uint32_t op2 = (insn >> 5) & 0x7u;
    const std::uint32_t key = (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;

    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 0u)) { // ICC_IAR1_EL1
      set_reg(rt, (exception_depth_ != 0 && exception_is_irq_stack_[exception_depth_ - 1]) ? exception_intid_stack_[exception_depth_ - 1] : 1023u);
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
        key == ((3u << 14) | (3u << 11) | (14u << 7) | (0u << 3) | 2u)) { // CNTVCT_EL0/CNTPCT_EL0 minimal alias
      set_reg(rt, timer_.counter_at_steps(steps_));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 1u)) { // CNTV_CTL_EL0
      set_reg(rt, timer_.read_cntv_ctl_el0(steps_));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 2u)) { // CNTV_CVAL_EL0
      set_reg(rt, timer_.read_cntv_cval_el0());
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 0u)) { // CNTV_TVAL_EL0
      set_reg(rt, timer_.read_cntv_tval_el0(steps_));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 1u)) { // CNTP_CTL_EL0
      set_reg(rt, timer_.read_cntp_ctl_el0(steps_));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 2u)) { // CNTP_CVAL_EL0
      set_reg(rt, timer_.read_cntp_cval_el0());
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 0u)) { // CNTP_TVAL_EL0
      set_reg(rt, timer_.read_cntp_tval_el0(steps_));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (2u << 7) | (5u << 3) | 1u)) { // GCSPR_EL0
      set_reg(rt, 0);
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL0
      set_reg(rt, sysregs_.current_uses_sp_el0() ? regs_[31] : sysregs_.sp_el0());
      return true;
    }
    if (key == ((3u << 14) | (4u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL1
      set_reg(rt, (!sysregs_.current_uses_sp_el0()) ? regs_[31] : sysregs_.sp_el1());
      return true;
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
    const std::uint32_t key = (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;

    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (12u << 3) | 1u)) { // ICC_EOIR1_EL1
      if (exception_depth_ != 0) {
        const std::uint32_t idx = exception_depth_ - 1;
        if (exception_is_irq_stack_[idx] && exception_intid_stack_[idx] == static_cast<std::uint32_t>(reg(rt))) {
          running_priority_ = exception_prev_prio_stack_[idx];
          exception_prio_dropped_stack_[idx] = true;
          const bool eoi_drop_dir = ((icc_ctlr_el1_ >> 1) & 0x1u) != 0u;
          if (!eoi_drop_dir) {
            gic_.eoi(exception_intid_stack_[idx]);
          }
        }
      }
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (6u << 3) | 0u)) { // ICC_PMR_EL1
      icc_pmr_el1_ = reg(rt);
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
    if (key == ((3u << 14) | (0u << 11) | (12u << 7) | (11u << 3) | 1u)) { // ICC_DIR_EL1
      gic_.eoi(static_cast<std::uint32_t>(reg(rt)));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 1u)) { // CNTV_CTL_EL0
      timer_.write_cntv_ctl_el0(steps_, reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 2u)) { // CNTV_CVAL_EL0
      timer_.write_cntv_cval_el0(steps_, reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (3u << 3) | 0u)) { // CNTV_TVAL_EL0
      timer_.write_cntv_tval_el0(steps_, reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 1u)) { // CNTP_CTL_EL0
      timer_.write_cntp_ctl_el0(steps_, reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 2u)) { // CNTP_CVAL_EL0
      timer_.write_cntp_cval_el0(steps_, reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (3u << 11) | (14u << 7) | (2u << 3) | 0u)) { // CNTP_TVAL_EL0
      timer_.write_cntp_tval_el0(steps_, reg(rt));
      return true;
    }
    if (key == ((3u << 14) | (0u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL0
      sysregs_.set_sp_el0(reg(rt));
      if (sysregs_.current_uses_sp_el0()) {
        regs_[31] = reg(rt);
      }
      return true;
    }
    if (key == ((3u << 14) | (4u << 11) | (4u << 7) | (1u << 3) | 0u)) { // SP_EL1
      sysregs_.set_sp_el1(reg(rt));
      if (!sysregs_.current_uses_sp_el0()) {
        regs_[31] = reg(rt);
      }
      return true;
    }

    const bool ok = sysregs_.write(op0, op1, crn, crm, op2, reg(rt));
    if (!ok) {
      return false;
    }

    // Flush software TLB on translation regime changes.
    if (key == ((3u << 14) | (0u << 11) | (1u << 7) | (0u << 3) | 0u) || // SCTLR_EL1
        key == ((3u << 14) | (0u << 11) | (2u << 7) | (0u << 3) | 0u) || // TTBR0_EL1
        key == ((3u << 14) | (0u << 11) | (2u << 7) | (0u << 3) | 1u) || // TTBR1_EL1
        key == ((3u << 14) | (0u << 11) | (2u << 7) | (0u << 3) | 2u)) { // TCR_EL1
      tlb_flush_all();
    }
    return true;
  }

  return false;
}

bool Cpu::exec_data_processing(std::uint32_t insn) {
  const std::uint32_t rd = insn & 0x1Fu;
  const std::uint32_t rn = (insn >> 5) & 0x1Fu;

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

  // Minimal AdvSIMD/FP support for libc/busybox string routines.
  if ((insn & 0xBF20FC00u) == 0x0E003C00u) { // UMOV/MOV (lane to general)
    const std::uint32_t imm5 = (insn >> 16) & 0x1Fu;
    if (imm5 == 0u) {
      return false;
    }
    const std::uint32_t size_shift = static_cast<std::uint32_t>(__builtin_ctz(imm5));
    const std::uint32_t esize_bits = 8u << size_shift;
    const std::uint32_t lane = imm5 >> (size_shift + 1u);
    const std::uint64_t elem = vector_get_elem(qregs_[rn], esize_bits, lane);
    if (esize_bits < 64u) {
      set_reg32(rd, static_cast<std::uint32_t>(elem));
    } else {
      set_reg(rd, elem);
    }
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E000400u) { // DUP (element)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t imm5 = (insn >> 16) & 0x1Fu;
    const std::uint32_t imm4 = (insn >> 11) & 0xFu;
    if (imm5 == 0u || imm4 != 0u) {
      return false;
    }
    const std::uint32_t size_shift = static_cast<std::uint32_t>(__builtin_ctz(imm5));
    const std::uint32_t esize_bits = 8u << size_shift;
    if (esize_bits > (q ? 128u : 64u)) {
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
    const std::uint32_t lane = imm5 >> (size_shift + 1u);
    const std::uint64_t elem = (esize_bits == 64u) ? reg(rn) : (static_cast<std::uint64_t>(reg32(rn)) & ones(esize_bits));
    vector_set_elem(qregs_[rd], esize_bits, lane, elem);
    return true;
  }

  if ((insn & 0xBF80FC00u) == 0x0F00A400u || (insn & 0xBF80FC00u) == 0x2F00A400u) { // SSHLL/USHLL lower half, including SXTL/UXTL aliases
    const bool is_unsigned = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t immh = (insn >> 19) & 0xFu;
    const std::uint32_t immb = (insn >> 16) & 0x7u;
    if (immh == 0u) {
      return false;
    }
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
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      std::uint64_t value = vector_get_elem(src, src_esize_bits, lane);
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

  if ((insn & 0xBF808400u) == 0x2F000400u || (insn & 0xBF808400u) == 0x0F000400u) { // USHR/SSHR
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

  if ((insn & 0xBF80FC00u) == 0x2F000400u) { // MVNI (2S/4S immediate)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool op = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t cmode = (insn >> 12) & 0xFu;
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    const std::uint64_t imm64 = advsimd_expand_imm(op, cmode, imm8);
    auto& dst = qregs_[rd];
    dst[0] = ~imm64;
    dst[1] = q ? ~imm64 : 0u;
    return true;
  }

  if ((insn & 0xBF80FC00u) == 0x2F001400u) { // BIC (2S/4S immediate)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool op = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t cmode = (insn >> 12) & 0xFu;
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    const std::uint64_t imm64 = advsimd_expand_imm(op, cmode, imm8);
    const auto old = qregs_[rd];
    qregs_[rd][0] = old[0] & ~imm64;
    qregs_[rd][1] = q ? (old[1] & ~imm64) : 0u;
    return true;
  }

  if ((insn & 0xBF80FC00u) == 0x2F009400u) { // BIC (4H/8H immediate, low byte)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const auto old = qregs_[rd];
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    const std::uint64_t mask64 = replicate(static_cast<std::uint64_t>(imm8), 16u, 64u);
    qregs_[rd][0] = old[0] & ~mask64;
    qregs_[rd][1] = q ? (old[1] & ~mask64) : 0u;
    return true;
  }

  if ((insn & 0xBF80FC00u) == 0x2F00B400u) { // BIC (4H/8H immediate, high byte)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const auto old = qregs_[rd];
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    const std::uint64_t mask64 = replicate(static_cast<std::uint64_t>(imm8) << 8u, 16u, 64u);
    qregs_[rd][0] = old[0] & ~mask64;
    qregs_[rd][1] = q ? (old[1] & ~mask64) : 0u;
    return true;
  }

  if ((insn & 0xBF80FC00u) == 0x2F008400u) { // MVNI (4H/8H immediate)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const bool op = ((insn >> 29) & 1u) != 0u;
    const std::uint32_t cmode = (insn >> 12) & 0xFu;
    const std::uint8_t imm8 = static_cast<std::uint8_t>((((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu));
    const std::uint64_t imm64 = advsimd_expand_imm(op, cmode, imm8);
    auto& dst = qregs_[rd];
    dst[0] = ~imm64;
    dst[1] = q ? ~imm64 : 0u;
    return true;
  }

  if ((insn & 0xBF80FC00u) == 0x0F000400u) { // MOVI (2S/4S immediate)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint64_t imm8 = (((insn >> 16) & 0x7u) << 5u) | ((insn >> 5) & 0x1Fu);
    auto& dst = qregs_[rd];
    dst = {0, 0};
    const std::uint32_t lanes = q ? 4u : 2u;
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      vector_set_elem(dst, 32u, lane, imm8);
    }
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E209800u) { // CMEQ (zero)
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
      vector_set_elem(dst, esize_bits, lane, elem == 0u ? ones(esize_bits) : 0u);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0x9F20FC00u) == 0x0E208C00u) { // CMEQ (register)
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

  if ((insn & 0xBF208400u) == 0x2E000000u) { // EXT (vector extract)
    const bool q = ((insn >> 30) & 1u) != 0u;
    if (!q) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t imm4 = (insn >> 11) & 0xFu;
    std::array<std::uint8_t, 32> bytes{};
    for (std::uint32_t i = 0; i < 16u; ++i) {
      bytes[i] = static_cast<std::uint8_t>(vector_get_elem(qregs_[rn], 8u, i));
      bytes[16u + i] = static_cast<std::uint8_t>(vector_get_elem(qregs_[rm], 8u, i));
    }
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t i = 0; i < 16u; ++i) {
      vector_set_elem(dst, 8u, i, bytes[imm4 + i]);
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

  if ((insn & 0x9F20FC00u) == 0x0E203C00u) { // CMHS (register)
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
      const bool ge = vector_get_elem(lhs, esize_bits, lane) >= vector_get_elem(rhs, esize_bits, lane);
      vector_set_elem(dst, esize_bits, lane, ge ? ones(esize_bits) : 0u);
    }
    qregs_[rd] = dst;
    return true;
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

  if ((insn & 0xBF20FC00u) == 0x2E20AC00u) { // UMINP (vector, byte elements)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size != 0u) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t lanes = q ? 16u : 8u;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes / 2u; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, 8u, lane * 2u);
      const std::uint64_t b = vector_get_elem(lhs, 8u, lane * 2u + 1u);
      vector_set_elem(dst, 8u, lane, a < b ? a : b);
    }
    for (std::uint32_t lane = 0; lane < lanes / 2u; ++lane) {
      const std::uint64_t a = vector_get_elem(rhs, 8u, lane * 2u);
      const std::uint64_t b = vector_get_elem(rhs, 8u, lane * 2u + 1u);
      vector_set_elem(dst, 8u, lane + (lanes / 2u), a < b ? a : b);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x2E20A400u) { // UMAXP (vector, byte elements)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size != 0u) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t lanes = q ? 16u : 8u;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes / 2u; ++lane) {
      const std::uint64_t a = vector_get_elem(lhs, 8u, lane * 2u);
      const std::uint64_t b = vector_get_elem(lhs, 8u, lane * 2u + 1u);
      vector_set_elem(dst, 8u, lane, a > b ? a : b);
    }
    for (std::uint32_t lane = 0; lane < lanes / 2u; ++lane) {
      const std::uint64_t a = vector_get_elem(rhs, 8u, lane * 2u);
      const std::uint64_t b = vector_get_elem(rhs, 8u, lane * 2u + 1u);
      vector_set_elem(dst, 8u, lane + (lanes / 2u), a > b ? a : b);
    }
    qregs_[rd] = dst;
    return true;
  }

  if ((insn & 0xBF20FC00u) == 0x0E20BC00u) { // ADDP (vector, byte elements)
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::uint32_t size = (insn >> 22) & 0x3u;
    if (size != 0u) {
      return false;
    }
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t lanes = q ? 16u : 8u;
    const auto lhs = qregs_[rn];
    const auto rhs = qregs_[rm];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes / 2u; ++lane) {
      const std::uint64_t sum = (vector_get_elem(lhs, 8u, lane * 2u) + vector_get_elem(lhs, 8u, lane * 2u + 1u)) & 0xFFu;
      vector_set_elem(dst, 8u, lane, sum);
    }
    for (std::uint32_t lane = 0; lane < lanes / 2u; ++lane) {
      const std::uint64_t sum = (vector_get_elem(rhs, 8u, lane * 2u) + vector_get_elem(rhs, 8u, lane * 2u + 1u)) & 0xFFu;
      vector_set_elem(dst, 8u, lane + (lanes / 2u), sum);
    }
    qregs_[rd] = dst;
    return true;
  }


  if ((insn & 0xBF80FC00u) == 0x0F008400u) { // SHRN
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
    const auto src = qregs_[rn];
    std::array<std::uint64_t, 2> dst = {0, 0};
    for (std::uint32_t lane = 0; lane < lanes; ++lane) {
      const std::uint64_t narrowed = (vector_get_elem(src, src_esize_bits, lane) >> shift) & ones(dst_esize_bits);
      vector_set_elem(dst, dst_esize_bits, lane, narrowed);
    }
    qregs_[rd] = dst;
    return true;
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
    if (ftype == 0u) {
      write_fp32(rd, read_fp32(rn) * read_fp32(rm));
      return true;
    }
    if (ftype == 1u) {
      write_fp64(rd, read_fp64(rn) * read_fp64(rm));
      return true;
    }
    return false;
  }

  if ((insn & 0xFF20FC00u) == 0x1E201800u) { // FDIV (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      write_fp32(rd, read_fp32(rn) / read_fp32(rm));
      return true;
    }
    if (ftype == 1u) {
      write_fp64(rd, read_fp64(rn) / read_fp64(rm));
      return true;
    }
    return false;
  }

  if ((insn & 0xFF20FC00u) == 0x1E202800u) { // FADD (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      write_fp32(rd, read_fp32(rn) + read_fp32(rm));
      return true;
    }
    if (ftype == 1u) {
      write_fp64(rd, read_fp64(rn) + read_fp64(rm));
      return true;
    }
    return false;
  }

  if ((insn & 0xFF20FC00u) == 0x1E203800u) { // FSUB (scalar)
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      write_fp32(rd, read_fp32(rn) - read_fp32(rm));
      return true;
    }
    if (ftype == 1u) {
      write_fp64(rd, read_fp64(rn) - read_fp64(rm));
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
    if (ftype == 0u) {
      const float lhs = negate_product ? -read_fp32(rn) : read_fp32(rn);
      const float addend = negate_addend ? -read_fp32(ra) : read_fp32(ra);
      write_fp32(rd, std::fma(lhs, read_fp32(rm), addend));
      return true;
    }
    if (ftype == 1u) {
      const double lhs = negate_product ? -read_fp64(rn) : read_fp64(rn);
      const double addend = negate_addend ? -read_fp64(ra) : read_fp64(ra);
      write_fp64(rd, std::fma(lhs, read_fp64(rm), addend));
      return true;
    }
    return false;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E20C000u) { // FABS Sd, Sn
    write_fp32(rd, std::fabs(read_fp32(rn)));
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E60C000u) { // FABS Dd, Dn
    write_fp64(rd, std::fabs(read_fp64(rn)));
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E214000u) { // FNEG Sd, Sn
    write_fp32(rd, -read_fp32(rn));
    return true;
  }

  if ((insn & 0xFFFFFC00u) == 0x1E614000u) { // FNEG Dd, Dn
    write_fp64(rd, -read_fp64(rn));
    return true;
  }


  if ((insn & 0xFF3FFC00u) == 0x5E21B800u) { // FCVTZS Sd|Dd, Sn|Dn
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    std::uint64_t fpsr_bits = 0;
    if (ftype == 2u) {
      const std::int32_t out = fp_to_signed_rtz<std::int32_t>(static_cast<long double>(read_fp32(rn)), fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = static_cast<std::uint32_t>(out);
      qregs_[rd][1] = 0u;
      return true;
    }
    if (ftype == 3u) {
      const std::int64_t out = fp_to_signed_rtz<std::int64_t>(static_cast<long double>(read_fp64(rn)), fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      qregs_[rd][0] = static_cast<std::uint64_t>(out);
      qregs_[rd][1] = 0u;
      return true;
    }
    return false;
  }

  if ((insn & 0xFF3FFC00u) == 0x5E21D800u) { // SCVTF Sd|Dd, Sn|Dn
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      write_fp32(rd, static_cast<float>(static_cast<std::int32_t>(qregs_[rn][0] & 0xFFFFFFFFu)));
      return true;
    }
    if (ftype == 1u) {
      write_fp64(rd, static_cast<double>(static_cast<std::int64_t>(qregs_[rn][0])));
      return true;
    }
    return false;
  }

  if ((insn & 0xFF3FFC00u) == 0x7E21D800u) { // UCVTF Sd|Dd, Sn|Dn
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      write_fp32(rd, static_cast<float>(static_cast<std::uint32_t>(qregs_[rn][0] & 0xFFFFFFFFu)));
      return true;
    }
    if (ftype == 1u) {
      write_fp64(rd, static_cast<double>(qregs_[rn][0]));
      return true;
    }
    return false;
  }

  if ((insn & 0xFF3FFC00u) == 0x1E220000u || (insn & 0xFF3FFC00u) == 0x9E220000u) { // SCVTF Sd|Dd, Wn|Xn
    const bool from_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      const std::int32_t value = static_cast<std::int32_t>(reg32(rn));
      write_fp32(rd, static_cast<float>(value));
      return true;
    }
    if (ftype == 1u) {
      const std::int64_t value = from_x ? static_cast<std::int64_t>(reg(rn))
                                        : static_cast<std::int64_t>(static_cast<std::int32_t>(reg32(rn)));
      write_fp64(rd, static_cast<double>(value));
      return true;
    }
    return false;
  }

  if ((insn & 0xFF3FFC00u) == 0x1E230000u || (insn & 0xFF3FFC00u) == 0x9E230000u) { // UCVTF Sd|Dd, Wn|Xn
    const bool from_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      write_fp32(rd, static_cast<float>(reg32(rn)));
      return true;
    }
    if (ftype == 1u) {
      const std::uint64_t value = from_x ? reg(rn) : static_cast<std::uint64_t>(reg32(rn));
      write_fp64(rd, static_cast<double>(value));
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

  if (((insn & 0xFF20FC1Fu) == 0x1E202000u) || ((insn & 0xFF20FC1Fu) == 0x1E202010u)) { // FCMP/FCMPE Sn, Sm / Dn, Dm
    const std::uint32_t rm = (insn >> 16) & 0x1Fu;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      const float lhs = read_fp32(rn);
      const float rhs = read_fp32(rm);
      set_fp_compare_flags(std::isnan(lhs) || std::isnan(rhs), lhs < rhs, lhs == rhs);
      return true;
    }
    if (ftype == 1u) {
      const double lhs = read_fp64(rn);
      const double rhs = read_fp64(rm);
      set_fp_compare_flags(std::isnan(lhs) || std::isnan(rhs), lhs < rhs, lhs == rhs);
      return true;
    }
    return false;
  }

  if (((insn & 0xFF20FC1Fu) == 0x1E202008u) || ((insn & 0xFF20FC1Fu) == 0x1E202018u)) { // FCMP/FCMPE Sn|Dn, #0.0
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    if (ftype == 0u) {
      const float lhs = read_fp32(rn);
      set_fp_compare_flags(std::isnan(lhs), lhs < 0.0f, lhs == 0.0f);
      return true;
    }
    if (ftype == 1u) {
      const double lhs = read_fp64(rn);
      set_fp_compare_flags(std::isnan(lhs), lhs < 0.0, lhs == 0.0);
      return true;
    }
    return false;
  }

  if ((insn & 0xFF3FFC00u) == 0x1E380000u || (insn & 0xFF3FFC00u) == 0x9E380000u) { // FCVTZS
    const bool to_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    long double value = 0.0L;
    if (ftype == 0u) {
      value = static_cast<long double>(read_fp32(rn));
    } else if (ftype == 1u) {
      value = static_cast<long double>(read_fp64(rn));
    } else {
      return false;
    }
    std::uint64_t fpsr_bits = 0;
    if (to_x) {
      const std::int64_t out = fp_to_signed_rtz<std::int64_t>(value, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      set_reg(rd, static_cast<std::uint64_t>(out));
    } else {
      const std::int32_t out = fp_to_signed_rtz<std::int32_t>(value, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      set_reg32(rd, static_cast<std::uint32_t>(out));
    }
    return true;
  }

  if ((insn & 0xFF3FFC00u) == 0x1E390000u || (insn & 0xFF3FFC00u) == 0x9E390000u) { // FCVTZU
    const bool to_x = (insn >> 31) != 0u;
    const std::uint32_t ftype = (insn >> 22) & 0x3u;
    long double value = 0.0L;
    if (ftype == 0u) {
      value = static_cast<long double>(read_fp32(rn));
    } else if (ftype == 1u) {
      value = static_cast<long double>(read_fp64(rn));
    } else {
      return false;
    }
    std::uint64_t fpsr_bits = 0;
    if (to_x) {
      const std::uint64_t out = fp_to_unsigned_rtz<std::uint64_t>(value, fpsr_bits);
      sysregs_.fp_or_fpsr(fpsr_bits);
      set_reg(rd, out);
    } else {
      const std::uint32_t out = fp_to_unsigned_rtz<std::uint32_t>(value, fpsr_bits);
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
  const auto data_abort = [this](std::uint64_t va) {
    const std::uint32_t iss = last_translation_fault_.has_value() ? fault_status_code(*last_translation_fault_) : 0u;
    enter_sync_exception(pc_ - 4, sysregs_.in_el0() ? 0x24u : 0x25u, iss, true, va);
  };
  const auto mmu_read = [this](std::uint64_t va, std::size_t size) -> std::optional<std::uint64_t> {
    const auto result = translate_address(va, AccessType::Read, true);
    if (!result.has_value()) {
      return std::nullopt;
    }
    return bus_.read(result->pa, size);
  };
  const auto mmu_write = [this, insn, &mmu_read](std::uint64_t va, std::uint64_t value, std::size_t size) -> bool {
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
    const auto result = translate_address(va, AccessType::Write, true);
    if (!result.has_value()) {
      return false;
    }
    if (trace_write_va.has_value() && *trace_write_va >= va && *trace_write_va < (va + size)) {
      const std::uint64_t current_task = sysregs_.sp_el0();
      const auto task_stack = mmu_read(current_task + 32u, 8u);
      std::cerr << std::dec << "WRITE-HIT va=0x" << std::hex << va
                << " pa=0x" << result->pa
                << " size=" << std::dec << size
                << " value=0x" << std::hex << value
                << " pc=0x" << (pc_ - 4)
                << " insn=0x" << insn
                << " sp=0x" << regs_[31]
                << " x29=0x" << reg(29)
                << " x30=0x" << reg(30)
                << " sp_el0=0x" << current_task;
      if (task_stack.has_value()) {
        std::cerr << " task_stack=0x" << *task_stack;
      }
      std::cerr << '\n';
    }
    const bool ok = bus_.write(result->pa, value, size);
    if (ok) {
      exclusive_valid_ = false;
      exclusive_addr_ = 0;
      exclusive_size_ = 0;
    }
    return ok;
  };
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

  // Minimal SIMD&FP memory subset used by the current libc/busybox path.
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
    const std::uint64_t addr = sp_or_reg(rn) + (((insn >> 10) & 0xFFFu) * size);
    const bool ok = is_load ? load_vec(addr, rt, size) : store_vec(addr, rt, size);
    if (!ok) {
      data_abort(addr);
    }
    return true;
  }

  if ((insn & 0xFFC00C00u) == 0x3C800C00u || (insn & 0xFFC00C00u) == 0x3CC00C00u ||
      (insn & 0xFFC00C00u) == 0x3C800400u || (insn & 0xFFC00C00u) == 0x3CC00400u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    const std::size_t size = 16u;
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const bool pre_index = ((insn & 0xFFC00C00u) == 0x3C800C00u) || ((insn & 0xFFC00C00u) == 0x3CC00C00u);
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

  if ((insn & 0xFFE00C00u) == 0x3CA00800u || (insn & 0xFFE00C00u) == 0x3CE00800u ||
      (insn & 0xFFE00C00u) == 0xBC200800u || (insn & 0xFFE00C00u) == 0xBC600800u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    const std::size_t size = (insn & 0x80000000u) != 0u ? 4u : 16u;
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
    off <<= (s ? (size == 16u ? 4u : 2u) : 0u);
    const std::uint64_t addr = sp_or_reg(rn) + off;
    const bool ok = is_load ? load_vec(addr, rt, size) : store_vec(addr, rt, size);
    if (!ok) {
      data_abort(addr);
    }
    return true;
  }

  if ((insn & 0xFFC00C00u) == 0x3C800000u || (insn & 0xFFC00C00u) == 0x3CC00000u ||
      (insn & 0xFFC00C00u) == 0xBC000000u || (insn & 0xFFC00C00u) == 0xBC400000u) {
    const bool is_load = (insn & 0x00400000u) != 0u;
    const std::size_t size = (insn & 0x80000000u) != 0u ? 4u : 16u;
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const bool ok = is_load ? load_vec(addr, rt, size) : store_vec(addr, rt, size);
    if (!ok) {
      data_abort(addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C407000u) { // LD1 {Vt.8B/16B}, [Xn]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t size = q ? 16u : 8u;
    const std::uint64_t addr = reg(rn);
    if (!load_vec(addr, rt, size)) {
      data_abort(addr);
    }
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0CDF7000u) { // LD1 {Vt.8B/16B}, [Xn], #imm
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t size = q ? 16u : 8u;
    const std::uint64_t addr = reg(rn);
    if (!load_vec(addr, rt, size)) {
      data_abort(addr);
      return true;
    }
    set_reg(rn, addr + size);
    return true;
  }

  if ((insn & 0xBFFFFC00u) == 0x0C40A000u) { // LD1 {Vt.8B,Vt2.8B}/{Vt.16B,Vt2.16B}, [Xn]
    const bool q = ((insn >> 30) & 1u) != 0u;
    const std::size_t size = q ? 16u : 8u;
    const std::uint32_t rt2 = (rt + 1u) & 0x1Fu;
    const std::uint64_t addr = reg(rn);
    if (!load_vec(addr, rt, size) || !load_vec(addr + size, rt2, size)) {
      data_abort(addr);
    }
    return true;
  }

  // LDTR/STTR family (unprivileged immediate). Model access the same as normal
  // EL1 data accesses for the current single-core Linux bring-up path.
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
      if (is_load) {
        const auto value = mmu_read(addr, size);
        if (!value.has_value()) {
          data_abort(addr);
          return true;
        }
        switch (extend) {
        case LoadExtend::Zero:
          if (size == 8u) {
            set_reg(rt, *value);
          } else {
            set_reg32(rt, static_cast<std::uint32_t>(*value));
          }
          break;
        case LoadExtend::Sign32:
          if (size == 1u) {
            set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(*value & 0xFFu))));
          } else {
            set_reg32(rt, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(*value & 0xFFFFu))));
          }
          break;
        case LoadExtend::Sign64:
          if (size == 1u) {
            set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int8_t>(*value & 0xFFu))));
          } else if (size == 2u) {
            set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int16_t>(*value & 0xFFFFu))));
          } else {
            set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*value & 0xFFFFFFFFu))));
          }
          break;
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
  }

  // PRFM (literal / unsigned immediate / register offset).
  // Model as a no-op hint for the current single-core Linux bring-up path.
  if ((insn & 0xFF000000u) == 0xD8000000u ||
      (insn & 0xFFC00000u) == 0xF9800000u ||
      (insn & 0xFFE00C00u) == 0xF8A00800u) {
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
      const auto value = mmu_read(addr, size);
      if (!value.has_value()) {
        data_abort(addr);
        return true;
      }
      if (size == 8u) {
        set_reg(rt, *value);
      } else {
        set_reg32(rt, static_cast<std::uint32_t>(*value));
      }
      exclusive_valid_ = true;
      exclusive_addr_ = addr;
      exclusive_size_ = static_cast<std::uint8_t>(size);
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
      const bool match = exclusive_valid_ && exclusive_addr_ == addr && exclusive_size_ == size;
      exclusive_valid_ = false;
      exclusive_addr_ = 0;
      exclusive_size_ = 0;
      if (!match) {
        set_reg32(rs, 1u);
        return true;
      }
      const std::uint64_t value = (size == 8u) ? reg(rt) : static_cast<std::uint64_t>(reg32(rt));
      if (!mmu_write(addr, value, size)) {
        data_abort(addr);
        return true;
      }
      set_reg32(rs, 0u);
      return true;
    }
  }

  // LSE CASP/CASPA/CASPL/CASPAL. Ordering variants are modelled with
  // identical single-core semantics for now.
  if ((insn & 0x3FA07C00u) == 0x08207C00u) {
    const std::size_t elem_size = (((insn >> 30) & 0x1u) != 0u) ? 8u : 4u;
    const std::uint32_t rs = (insn >> 16) & 0x1Fu;
    const std::uint32_t rt_pair = rt;
    const std::uint64_t mask = (elem_size == 8u) ? ~0ull : 0xFFFFFFFFull;
    const std::uint64_t addr = sp_or_reg(rn);
    const auto old_lo = mmu_read(addr, elem_size);
    const auto old_hi = mmu_read(addr + elem_size, elem_size);
    if (!old_lo.has_value() || !old_hi.has_value()) {
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
      if (!mmu_write(addr, desired_lo, elem_size) || !mmu_write(addr + elem_size, desired_hi, elem_size)) {
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

  // LSE atomic CAS/CASA/CASL/CASAL. Ordering variants are modelled with
  // identical single-core semantics for now.
  if ((insn & 0x3FA07C00u) == 0x08A07C00u) {
    const std::uint32_t size_code = (insn >> 30) & 0x3u;
    const std::size_t size = (size_code == 0u) ? 1u : (size_code == 1u) ? 2u : (size_code == 2u) ? 4u : 8u;
    if (size != 0u) {
      const std::uint32_t rs = (insn >> 16) & 0x1Fu;
      const std::uint64_t mask = (size == 8u) ? ~0ull : 0xFFFFFFFFull;
      const std::uint64_t addr = sp_or_reg(rn);
      const auto old = mmu_read(addr, size);
      if (!old.has_value()) {
        data_abort(addr);
        return true;
      }
      const std::uint64_t old_value = *old & mask;
      const std::uint64_t compare_value = ((size == 8u) ? reg(rs) : static_cast<std::uint64_t>(reg32(rs))) & mask;
      const std::uint64_t desired_value = ((size == 8u) ? reg(rt) : static_cast<std::uint64_t>(reg32(rt))) & mask;
      if (old_value == compare_value) {
        if (!mmu_write(addr, desired_value, size)) {
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

  // LSE SWP/LDADD/LDCLR/LDEOR/LDSET. Ordering variants are treated as
  // no-ops beyond the read-modify-write result on the current single core.
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
        const auto old = mmu_read(addr, size);
        if (!old.has_value()) {
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
        if (!mmu_write(addr, new_value, size)) {
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
        const auto value = mmu_read(addr, size);
        if (!value.has_value()) {
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
        if (!mmu_write(addr, value, size)) {
          data_abort(addr);
          return true;
        }
      }
      return true;
    }
  }

  // LDXP / LDAXP / STXP / STLXP (64-bit pair exclusive).
  {
    const std::uint32_t tag = insn & 0xFFE08000u;
    if (tag == 0xC8600000u || tag == 0xC8608000u ||
        tag == 0xC8200000u || tag == 0xC8208000u) {
      const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
      const std::uint64_t addr = sp_or_reg(rn);
      if (tag == 0xC8600000u || tag == 0xC8608000u) {
        const auto v1 = mmu_read(addr, 8);
        const auto v2 = mmu_read(addr + 8, 8);
        if (!v1.has_value() || !v2.has_value()) {
          data_abort(addr);
          return true;
        }
        set_reg(rt, *v1);
        set_reg(rt2, *v2);
        exclusive_valid_ = true;
        exclusive_addr_ = addr;
        exclusive_size_ = 16u;
        return true;
      }

      const std::uint32_t rs = (insn >> 16) & 0x1Fu;
      const bool match = exclusive_valid_ && exclusive_addr_ == addr && exclusive_size_ == 16u;
      exclusive_valid_ = false;
      exclusive_addr_ = 0;
      exclusive_size_ = 0;
      if (!match) {
        set_reg32(rs, 1u);
        return true;
      }
      if (!mmu_write(addr, reg(rt), 8) || !mmu_write(addr + 8, reg(rt2), 8)) {
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
    const auto value = mmu_read(addr, 8);
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
    const auto value = mmu_read(addr, 4);
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
    const auto value = mmu_read(addr, 4);
    if (!value.has_value()) {
      data_abort(addr);
      return true;
    }
    set_reg(rt, static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*value & 0xFFFFFFFFu))));
    return true;
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
    const auto value = mmu_read(addr, 8);
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
    const auto value = mmu_read(addr, 1);
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
    const auto value = mmu_read(addr, 1);
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
    const auto value = mmu_read(addr, 1);
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
    if (!mmu_write(addr, reg32(rt) & 0xFFu, 1)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDURH Wt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0x78400000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    const auto value = mmu_read(addr, 2);
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
    const auto value = mmu_read(addr, 2);
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
    const auto value = mmu_read(addr, 2);
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
    if (!mmu_write(addr, reg32(rt) & 0xFFFFu, 2)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // STUR Xt, [Xn|SP, #simm9]
  if ((insn & 0xFFC00C00u) == 0xF8000000u) {
    const std::int64_t simm9 = sign_extend((insn >> 12) & 0x1FFu, 9);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + simm9);
    if (!mmu_write(addr, reg(rt), 8)) {
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

  // STP/LDP Dt1, Dt2 pair transfers.
  if ((insn & 0xFFC00000u) == 0x6D800000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    if (!mmu_write(addr, q_reg_lane(rt, 0), 8) || !mmu_write(addr + 8, q_reg_lane(rt2, 0), 8)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0x6C800000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = sp_or_reg(rn);
    if (!mmu_write(addr, q_reg_lane(rt, 0), 8) || !mmu_write(addr + 8, q_reg_lane(rt2, 0), 8)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0x6D000000u || (insn & 0xFFC00000u) == 0x6C000000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    if (!mmu_write(addr, q_reg_lane(rt, 0), 8) || !mmu_write(addr + 8, q_reg_lane(rt2, 0), 8)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  if ((insn & 0xFFC00000u) == 0x6DC00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    const auto v0 = mmu_read(addr, 8);
    const auto v1 = mmu_read(addr + 8, 8);
    if (!v0.has_value() || !v1.has_value()) {
      data_abort(addr);
      return true;
    }
    set_q_reg_lane(rt, 0, *v0);
    set_q_reg_lane(rt, 1, 0u);
    set_q_reg_lane(rt2, 0, *v1);
    set_q_reg_lane(rt2, 1, 0u);
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0x6CC00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = sp_or_reg(rn);
    const auto v0 = mmu_read(addr, 8);
    const auto v1 = mmu_read(addr + 8, 8);
    if (!v0.has_value() || !v1.has_value()) {
      data_abort(addr);
      return true;
    }
    set_q_reg_lane(rt, 0, *v0);
    set_q_reg_lane(rt, 1, 0u);
    set_q_reg_lane(rt2, 0, *v1);
    set_q_reg_lane(rt2, 1, 0u);
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0x6D400000u || (insn & 0xFFC00000u) == 0x6C400000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 3u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    const auto v0 = mmu_read(addr, 8);
    const auto v1 = mmu_read(addr + 8, 8);
    if (!v0.has_value() || !v1.has_value()) {
      data_abort(addr);
      return true;
    }
    set_q_reg_lane(rt, 0, *v0);
    set_q_reg_lane(rt, 1, 0u);
    set_q_reg_lane(rt2, 0, *v1);
    set_q_reg_lane(rt2, 1, 0u);
    return true;
  }

  // STP/LDP Qt1, Qt2 pair transfers.
  if ((insn & 0xFFC00000u) == 0xAD800000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 4u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    if (!mmu_write(addr, q_reg_lane(rt, 0), 8) || !mmu_write(addr + 8, q_reg_lane(rt, 1), 8) ||
        !mmu_write(addr + 16, q_reg_lane(rt2, 0), 8) || !mmu_write(addr + 24, q_reg_lane(rt2, 1), 8)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0xAC800000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 4u;
    const std::uint64_t addr = sp_or_reg(rn);
    if (!mmu_write(addr, q_reg_lane(rt, 0), 8) || !mmu_write(addr + 8, q_reg_lane(rt, 1), 8) ||
        !mmu_write(addr + 16, q_reg_lane(rt2, 0), 8) || !mmu_write(addr + 24, q_reg_lane(rt2, 1), 8)) {
      data_abort(addr);
      return true;
    }
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0xAD000000u || (insn & 0xFFC00000u) == 0xAC000000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 4u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    if (!mmu_write(addr, q_reg_lane(rt, 0), 8) || !mmu_write(addr + 8, q_reg_lane(rt, 1), 8) ||
        !mmu_write(addr + 16, q_reg_lane(rt2, 0), 8) || !mmu_write(addr + 24, q_reg_lane(rt2, 1), 8)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  if ((insn & 0xFFC00000u) == 0xADC00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 4u;
    const std::uint64_t base = sp_or_reg(rn);
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + imm7);
    const auto v00 = mmu_read(addr, 8);
    const auto v01 = mmu_read(addr + 8, 8);
    const auto v10 = mmu_read(addr + 16, 8);
    const auto v11 = mmu_read(addr + 24, 8);
    if (!v00.has_value() || !v01.has_value() || !v10.has_value() || !v11.has_value()) {
      data_abort(addr);
      return true;
    }
    set_q_reg_lane(rt, 0, *v00);
    set_q_reg_lane(rt, 1, *v01);
    set_q_reg_lane(rt2, 0, *v10);
    set_q_reg_lane(rt2, 1, *v11);
    set_sp_or_reg(rn, addr, false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0xACC00000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 4u;
    const std::uint64_t addr = sp_or_reg(rn);
    const auto v00 = mmu_read(addr, 8);
    const auto v01 = mmu_read(addr + 8, 8);
    const auto v10 = mmu_read(addr + 16, 8);
    const auto v11 = mmu_read(addr + 24, 8);
    if (!v00.has_value() || !v01.has_value() || !v10.has_value() || !v11.has_value()) {
      data_abort(addr);
      return true;
    }
    set_q_reg_lane(rt, 0, *v00);
    set_q_reg_lane(rt, 1, *v01);
    set_q_reg_lane(rt2, 0, *v10);
    set_q_reg_lane(rt2, 1, *v11);
    set_sp_or_reg(rn, static_cast<std::uint64_t>(static_cast<std::int64_t>(addr) + imm7), false);
    return true;
  }

  if ((insn & 0xFFC00000u) == 0xAD400000u || (insn & 0xFFC00000u) == 0xAC400000u) {
    const std::uint32_t rt2 = (insn >> 10) & 0x1Fu;
    const std::int64_t imm7 = sign_extend((insn >> 15) & 0x7Fu, 7) << 4u;
    const std::uint64_t addr = static_cast<std::uint64_t>(static_cast<std::int64_t>(sp_or_reg(rn)) + imm7);
    const auto v00 = mmu_read(addr, 8);
    const auto v01 = mmu_read(addr + 8, 8);
    const auto v10 = mmu_read(addr + 16, 8);
    const auto v11 = mmu_read(addr + 24, 8);
    if (!v00.has_value() || !v01.has_value() || !v10.has_value() || !v11.has_value()) {
      data_abort(addr);
      return true;
    }
    set_q_reg_lane(rt, 0, *v00);
    set_q_reg_lane(rt, 1, *v01);
    set_q_reg_lane(rt2, 0, *v10);
    set_q_reg_lane(rt2, 1, *v11);
    return true;
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
    const auto value = mmu_read(addr, 8);
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
    if (!mmu_write(addr, reg(rt), 8)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDR Wt, [Xn, #imm12]
  if ((insn & 0xFFC00000u) == 0xB9400000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 2u);
    const auto value = mmu_read(addr, 4);
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
    const auto value = mmu_read(addr, 4);
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
    const auto value = mmu_read(addr, 4);
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
    const auto value = mmu_read(addr, 4);
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
    if (!mmu_write(addr, reg(rt) & 0xFFFFFFFFu, 4)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDRSB Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x39C00000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + imm12;
    const auto value = mmu_read(addr, 1);
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
    const auto value = mmu_read(addr, 1);
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
    const auto value = mmu_read(addr, 2);
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
    const auto value = mmu_read(addr, 2);
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
    const auto value = mmu_read(addr, 1);
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
    if (!mmu_write(addr, reg32(rt) & 0xFFu, 1)) {
      data_abort(addr);
      return true;
    }
    return true;
  }

  // LDRH Wt, [Xn|SP, #imm12]
  if ((insn & 0xFFC00000u) == 0x79400000u) {
    const std::uint64_t imm12 = (insn >> 10) & 0xFFFu;
    const std::uint64_t addr = sp_or_reg(rn) + (imm12 << 1u);
    const auto value = mmu_read(addr, 2);
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
    if (!mmu_write(addr, reg32(rt) & 0xFFFFu, 2)) {
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
  const std::uint64_t sign = 1ull << (bits - 1);
  const std::uint64_t mask = (1ull << bits) - 1;
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
           snapshot_io::write(out, entry.level) &&
           snapshot_io::write(out, entry.attr_index) &&
           snapshot_io::write(out, entry.mair_attr) &&
           snapshot_io::write_bool(out, entry.writable) &&
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
      !snapshot_io::write_bool(out, exclusive_valid_) ||
      !snapshot_io::write(out, exclusive_addr_) ||
      !snapshot_io::write(out, exclusive_size_)) {
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
  const auto read_translation_fault = [&](TranslationFault& fault) {
    std::uint8_t kind = 0;
    if (!snapshot_io::read(in, kind) || kind > static_cast<std::uint8_t>(TranslationFault::Kind::Permission) ||
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
    if (!snapshot_io::read(in, entry.pa_page) || !snapshot_io::read(in, entry.level) || !snapshot_io::read(in, entry.attr_index) || !snapshot_io::read(in, entry.mair_attr) ||
        !snapshot_io::read_bool(in, entry.writable) || !snapshot_io::read_bool(in, entry.executable) || !snapshot_io::read_bool(in, entry.pxn) ||
        !snapshot_io::read_bool(in, entry.uxn) || !snapshot_io::read(in, memory_type) || !snapshot_io::read(in, leaf_shareability) ||
        !snapshot_io::read(in, walk_inner) || !snapshot_io::read(in, walk_outer) || !snapshot_io::read(in, walk_shareability) ||
        !snapshot_io::read(in, entry.walk_attrs.ips_bits)) {
      return false;
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
  if (!sysregs_.load_state(in) ||
      !snapshot_io::read(in, exception_depth_) ||
      exception_depth_ > exception_is_irq_stack_.size() ||
      !snapshot_io::read_array(in, exception_is_irq_stack_) ||
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
  if (!snapshot_io::read(in, icc_ctlr_el1_) ||
      !snapshot_io::read(in, icc_sre_el1_) ||
      !snapshot_io::read(in, icc_bpr1_el1_) ||
      !snapshot_io::read(in, icc_igrpen1_el1_) ||
      !snapshot_io::read_array(in, icc_ap0r_el1_) ||
      !snapshot_io::read_array(in, icc_ap1r_el1_) ||
      !snapshot_io::read_bool(in, exclusive_valid_) ||
      !snapshot_io::read(in, exclusive_addr_) ||
      !snapshot_io::read(in, exclusive_size_)) {
    return false;
  }
  std::uint64_t tlb_size = 0;
  if (!snapshot_io::read(in, tlb_size) || tlb_size > (1ull << 20)) {
    return false;
  }
  tlb_flush_all();
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
  if (!snapshot_io::read(in, pc_) || !snapshot_io::read(in, steps_) || !snapshot_io::read_bool(in, halted_)) {
    return false;
  }
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
