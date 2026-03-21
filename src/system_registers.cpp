#include "aarchvm/system_registers.hpp"

#include "aarchvm/snapshot_io.hpp"

namespace aarchvm {

namespace {

constexpr std::uint32_t SysReg(std::uint32_t op0,
                               std::uint32_t op1,
                               std::uint32_t crn,
                               std::uint32_t crm,
                               std::uint32_t op2) {
  return (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;
}

constexpr std::uint64_t kSctlrEl1Span = 1ull << 23;
constexpr std::uint64_t kIdAa64Dfr0El1DebugMinimal =
    (0x0ull << 28) |  // CTX_CMPs: 1 context-aware breakpoint.
    (0x1ull << 20) |  // WRPs: 2 watchpoints.
    (0x1ull << 12) |  // BRPs: 2 breakpoints.
    0x6ull;           // DebugVer: Armv8.0 debug architecture.

constexpr std::uint32_t decode_debug_resource_count(std::uint64_t dfr0,
                                                    unsigned dfr0_shift,
                                                    std::uint64_t dfr1,
                                                    unsigned dfr1_shift) {
  const std::uint32_t extended = static_cast<std::uint32_t>((dfr1 >> dfr1_shift) & 0xFFu);
  if (extended != 0u) {
    return extended + 1u;
  }
  const std::uint32_t legacy = static_cast<std::uint32_t>((dfr0 >> dfr0_shift) & 0xFu);
  return legacy == 0xFu ? 16u : legacy + 1u;
}

} // namespace

void SystemRegisters::reset() {
  sctlr_el1_ = 0x30D00800;
  cpacr_el1_ = 0;
  midr_el1_ = 0x00000000410FD034ull;
  mpidr_el1_ = 0x0000000080000000ull;
  revidr_el1_ = 0;
  clidr_el1_ = 0;
  ctr_el0_ = 0x8444C004ull;
  dczid_el0_ = 0x4ull;
  id_aa64pfr0_el1_ = 0x0000000001000011ull;
  id_aa64pfr1_el1_ = 0;
  id_aa64dfr0_el1_ = kIdAa64Dfr0El1DebugMinimal;
  id_aa64dfr1_el1_ = 0;
  id_aa64isar0_el1_ = 0x0000000000210000ull;
  id_aa64isar1_el1_ = 0;
  id_aa64isar2_el1_ = 0;
  id_aa64isar3_el1_ = 0;
  id_aa64zfr0_el1_ = 0;
  id_aa64mmfr0_el1_ = 0x000000000F000005ull;
  id_aa64mmfr1_el1_ = 0x0000000000100000ull;
  id_aa64mmfr2_el1_ = 0;
  id_aa64mmfr3_el1_ = 0;
  csselr_el1_ = 0;
  ccsidr_el1_ = 0;
  ttbr0_el1_ = 0;
  ttbr1_el1_ = 0;
  tcr_el1_ = 0;
  mair_el1_ = 0;
  contextidr_el1_ = 0;
  osdlr_el1_ = 0;
  oslar_el1_ = 0;
  dbgbvr_el1_.fill(0);
  dbgbcr_el1_.fill(0);
  dbgwvr_el1_.fill(0);
  dbgwcr_el1_.fill(0);
  vbar_el1_ = 0;
  elr_el1_ = 0;
  spsr_el1_ = 0;
  sp_el0_ = 0;
  sp_el1_ = 0;
  spsel_ = 1;
  par_el1_ = 0;
  esr_el1_ = 0;
  far_el1_ = 0;
  mdscr_el1_ = 0;
  pmuserenr_el0_ = 0;
  amuserenr_el0_ = 0;
  tpidr_el0_ = 0;
  tpidr2_el0_ = 0;
  tpidrro_el0_ = 0;
  tpidr_el1_ = 0;
  tpidr_el2_ = 0;
  cntfrq_el0_ = 100000000;
  cntvct_el0_ = 0;
  cntkctl_el1_ = 0;
  fpcr_ = 0;
  fpsr_ = 0;
  pstate_ = {};
  pstate_.mode = 0x5u;
}

bool SystemRegisters::read(std::uint32_t op0,
                           std::uint32_t op1,
                           std::uint32_t crn,
                           std::uint32_t crm,
                           std::uint32_t op2,
                           std::uint64_t& value) const {
  if (op0 == 2u && op1 == 0u && crn == 0u && crm < 16u) {
    const std::uint32_t limit =
        (op2 == 4u || op2 == 5u) ? breakpoint_resource_count() :
        (op2 == 6u || op2 == 7u) ? watchpoint_resource_count() : 0u;
    if (limit != 0u && crm >= limit) {
      return false;
    }
    switch (op2) {
    case 4u: value = dbgbvr_el1_[crm]; return true;
    case 5u: value = dbgbcr_el1_[crm]; return true;
    case 6u: value = dbgwvr_el1_[crm]; return true;
    case 7u: value = dbgwcr_el1_[crm]; return true;
    default: break;
    }
  }
  switch (make_key(op0, op1, crn, crm, op2)) {
  case SysReg(3, 0, 0, 0, 0): value = midr_el1_; return true;
  case SysReg(3, 0, 0, 0, 5): value = mpidr_el1_; return true;
  case SysReg(3, 0, 0, 0, 6): value = revidr_el1_; return true;
  case SysReg(3, 1, 0, 0, 0): value = ccsidr_el1_; return true;
  case SysReg(3, 0, 1, 0, 0): value = sctlr_el1_; return true;
  case SysReg(3, 0, 1, 0, 2): value = cpacr_el1_; return true;
  case SysReg(3, 2, 0, 0, 0): value = csselr_el1_; return true;
  case SysReg(3, 0, 2, 0, 0): value = ttbr0_el1_; return true;
  case SysReg(3, 0, 2, 0, 1): value = ttbr1_el1_; return true;
  case SysReg(3, 0, 2, 0, 2): value = tcr_el1_; return true;
  case SysReg(3, 1, 0, 0, 1): value = clidr_el1_; return true;
  case SysReg(3, 0, 0, 4, 0): value = id_aa64pfr0_el1_; return true;
  case SysReg(3, 0, 0, 4, 1): value = id_aa64pfr1_el1_; return true;
  case SysReg(3, 0, 0, 5, 0): value = id_aa64dfr0_el1_; return true;
  case SysReg(3, 0, 0, 5, 1): value = id_aa64dfr1_el1_; return true;
  case SysReg(3, 0, 0, 6, 0): value = id_aa64isar0_el1_; return true;
  case SysReg(3, 0, 0, 6, 1): value = id_aa64isar1_el1_; return true;
  case SysReg(3, 0, 0, 6, 2): value = id_aa64isar2_el1_; return true;
  case SysReg(3, 0, 0, 6, 3): value = id_aa64isar3_el1_; return true;
  case SysReg(3, 0, 0, 4, 4): value = id_aa64zfr0_el1_; return true;
  case SysReg(3, 0, 0, 7, 0): value = id_aa64mmfr0_el1_; return true;
  case SysReg(3, 0, 0, 7, 1): value = id_aa64mmfr1_el1_; return true;
  case SysReg(3, 0, 0, 7, 2): value = id_aa64mmfr2_el1_; return true;
  case SysReg(3, 0, 0, 7, 3): value = id_aa64mmfr3_el1_; return true;
  case SysReg(3, 0, 0, 7, 4): value = 0; return true; // ID_AA64MMFR4_EL1
  case SysReg(3, 0, 0, 4, 2): value = 0; return true; // ID_AA64PFR2_EL1
  case SysReg(3, 0, 0, 4, 5): value = 0; return true; // ID_AA64SMFR0_EL1
  case SysReg(3, 0, 0, 4, 7): value = 0; return true; // Reserved/unknown feature ID, keep disabled
  case SysReg(3, 0, 10, 2, 0): value = mair_el1_; return true;
  case SysReg(3, 0, 13, 0, 1): value = contextidr_el1_; return true;
  case SysReg(2, 0, 1, 3, 4): value = osdlr_el1_; return true;
  case SysReg(2, 0, 1, 1, 4): value = 0x8ull | ((oslar_el1_ & 1ull) << 1); return true;
  case SysReg(3, 0, 12, 0, 0): value = vbar_el1_; return true;
  case SysReg(3, 0, 4, 0, 1): value = elr_el1_; return true;
  case SysReg(3, 0, 4, 0, 0): value = spsr_el1_; return true;
  case SysReg(3, 0, 4, 2, 2): value = static_cast<std::uint64_t>(current_el()) << 2; return true;
  case SysReg(3, 0, 5, 2, 0): value = esr_el1_; return true;
  case SysReg(3, 0, 6, 0, 0): value = far_el1_; return true;
  case SysReg(2, 0, 0, 2, 2): value = mdscr_el1_; return true;
  case SysReg(3, 3, 13, 0, 2): value = tpidr_el0_; return true;
  case SysReg(3, 3, 13, 0, 3): value = tpidrro_el0_; return true;
  case SysReg(3, 0, 13, 0, 4): value = tpidr_el1_; return true;
  case SysReg(3, 4, 13, 0, 2): value = tpidr_el2_; return true;
  case SysReg(3, 0, 4, 1, 0): value = sp_el0_; return true;
  case SysReg(3, 4, 4, 1, 0): value = sp_el1_; return true;
  case SysReg(3, 0, 7, 4, 0): value = par_el1_; return true;
  case SysReg(3, 3, 4, 2, 1): value = daif(); return true;
  case SysReg(3, 0, 4, 2, 0): value = spsel_; return true;
  case SysReg(3, 3, 0, 0, 1): value = ctr_el0_; return true;
  case SysReg(3, 3, 0, 0, 7): value = dczid_el0_; return true;
  case SysReg(3, 3, 14, 0, 0): value = cntfrq_el0_; return true;
  case SysReg(3, 3, 14, 0, 1): value = cntvct_el0_; return true; // CNTPCT_EL0 (minimal alias)
  case SysReg(3, 3, 14, 0, 2): value = cntvct_el0_; return true;
  case SysReg(3, 0, 14, 1, 0): value = cntkctl_el1_; return true;
  case SysReg(3, 3, 4, 4, 0): value = fpcr_; return true;
  case SysReg(3, 3, 4, 4, 1): value = fpsr_; return true;
  case SysReg(3, 0, 4, 2, 3): value = pstate_.pan ? 1ull : 0ull; return true;
  case SysReg(3, 3, 4, 2, 0): value = nzcv(); return true;
  default:
    return false;
  }
}

bool SystemRegisters::write(std::uint32_t op0,
                            std::uint32_t op1,
                            std::uint32_t crn,
                            std::uint32_t crm,
                            std::uint32_t op2,
                            std::uint64_t value) {
  if (op0 == 2u && op1 == 0u && crn == 0u && crm < 16u) {
    const std::uint32_t limit =
        (op2 == 4u || op2 == 5u) ? breakpoint_resource_count() :
        (op2 == 6u || op2 == 7u) ? watchpoint_resource_count() : 0u;
    if (limit != 0u && crm >= limit) {
      return false;
    }
    switch (op2) {
    case 4u: dbgbvr_el1_[crm] = value; return true;
    case 5u: dbgbcr_el1_[crm] = value; return true;
    case 6u: dbgwvr_el1_[crm] = value; return true;
    case 7u: dbgwcr_el1_[crm] = value; return true;
    default: break;
    }
  }
  switch (make_key(op0, op1, crn, crm, op2)) {
  case SysReg(3, 0, 1, 0, 0): sctlr_el1_ = value; return true;
  case SysReg(3, 0, 1, 0, 2): cpacr_el1_ = value; return true;
  case SysReg(3, 2, 0, 0, 0): csselr_el1_ = value; return true;
  case SysReg(3, 0, 2, 0, 0): ttbr0_el1_ = value; return true;
  case SysReg(3, 0, 2, 0, 1): ttbr1_el1_ = value; return true;
  case SysReg(3, 0, 2, 0, 2): tcr_el1_ = value; return true;
  case SysReg(3, 0, 10, 2, 0): mair_el1_ = value; return true;
  case SysReg(3, 0, 13, 0, 1): contextidr_el1_ = value; return true;
  case SysReg(2, 0, 1, 3, 4): osdlr_el1_ = value; return true;
  case SysReg(2, 0, 1, 0, 4): oslar_el1_ = value & 1ull; return true;
  case SysReg(3, 0, 12, 0, 0): vbar_el1_ = value; return true;
  case SysReg(3, 0, 4, 0, 1): elr_el1_ = value; return true;
  case SysReg(3, 0, 4, 0, 0): spsr_el1_ = value; return true;
  case SysReg(3, 0, 5, 2, 0): esr_el1_ = value; return true;
  case SysReg(3, 0, 6, 0, 0): far_el1_ = value; return true;
  case SysReg(2, 0, 0, 2, 2): mdscr_el1_ = value; return true;
  case SysReg(3, 3, 13, 0, 2): tpidr_el0_ = value; return true;
  case SysReg(3, 3, 13, 0, 3): tpidrro_el0_ = value; return true;
  case SysReg(3, 0, 13, 0, 4): tpidr_el1_ = value; return true;
  case SysReg(3, 4, 13, 0, 2): tpidr_el2_ = value; return true;
  case SysReg(3, 0, 4, 1, 0): sp_el0_ = value; return true;
  case SysReg(3, 4, 4, 1, 0): sp_el1_ = value; return true;
  case SysReg(3, 0, 7, 4, 0): par_el1_ = value; return true;
  case SysReg(3, 3, 4, 2, 1): set_daif(value); return true;
  case SysReg(3, 0, 4, 2, 0): set_spsel(value); return true;
  case SysReg(3, 0, 14, 1, 0): cntkctl_el1_ = value; return true;
  case SysReg(3, 0, 4, 2, 3): pstate_.pan = (value & 1u) != 0; return true;
  case SysReg(3, 3, 4, 4, 0): fpcr_ = value & 0xFFFFFFFFu; return true;
  case SysReg(3, 3, 4, 4, 1): fpsr_ = value & 0xFFFFFFFFu; return true;
  case SysReg(3, 3, 4, 2, 0): set_nzcv(value); return true;
  default:
    return false;
  }
}

std::uint32_t SystemRegisters::breakpoint_resource_count() const {
  return decode_debug_resource_count(id_aa64dfr0_el1_, 12u, id_aa64dfr1_el1_, 8u);
}

std::uint32_t SystemRegisters::watchpoint_resource_count() const {
  return decode_debug_resource_count(id_aa64dfr0_el1_, 20u, id_aa64dfr1_el1_, 16u);
}

std::uint64_t SystemRegisters::nzcv() const {
  return (static_cast<std::uint64_t>(pstate_.n) << 31) |
         (static_cast<std::uint64_t>(pstate_.z) << 30) |
         (static_cast<std::uint64_t>(pstate_.c) << 29) |
         (static_cast<std::uint64_t>(pstate_.v) << 28);
}

void SystemRegisters::set_nzcv(std::uint64_t value) {
  pstate_.n = ((value >> 31) & 1u) != 0;
  pstate_.z = ((value >> 30) & 1u) != 0;
  pstate_.c = ((value >> 29) & 1u) != 0;
  pstate_.v = ((value >> 28) & 1u) != 0;
}

std::uint64_t SystemRegisters::daif() const {
  return (static_cast<std::uint64_t>(pstate_.d) << 9) |
         (static_cast<std::uint64_t>(pstate_.a) << 8) |
         (static_cast<std::uint64_t>(pstate_.i) << 7) |
         (static_cast<std::uint64_t>(pstate_.f) << 6);
}

std::uint64_t SystemRegisters::pstate_bits() const {
  return nzcv() |
         (pstate_.il ? (1ull << 20) : 0ull) |
         daif() |
         (pstate_.pan ? (1ull << 22) : 0ull) |
         (static_cast<std::uint64_t>(pstate_.mode) & 0xFu);
}

void SystemRegisters::set_pstate_bits(std::uint64_t value) {
  set_nzcv(value);
  pstate_.il = ((value >> 20) & 1u) != 0;
  set_daif(value);
  pstate_.pan = ((value >> 22) & 1u) != 0;
  pstate_.mode = static_cast<std::uint8_t>(value & 0xFu);
  if (current_el() == 1u) {
    spsel_ = pstate_.mode & 0x1u;
  } else {
    spsel_ = 0;
  }
}

void SystemRegisters::set_daif(std::uint64_t value) {
  pstate_.d = ((value >> 9) & 1u) != 0;
  pstate_.a = ((value >> 8) & 1u) != 0;
  pstate_.i = ((value >> 7) & 1u) != 0;
  pstate_.f = ((value >> 6) & 1u) != 0;
}

void SystemRegisters::set_spsel(std::uint64_t value) {
  spsel_ = value & 1u;
  if (current_el() == 1u) {
    pstate_.mode = spsel_ != 0 ? 0x5u : 0x4u;
  }
}

void SystemRegisters::exception_enter_irq(std::uint64_t return_pc) {
  elr_el1_ = return_pc;
  spsr_el1_ = pstate_bits();
  esr_el1_ = 0;
  pstate_.mode = 0x5u;
  spsel_ = 1u;
  pstate_.il = false;
  pstate_.d = true;
  pstate_.a = true;
  if ((sctlr_el1_ & kSctlrEl1Span) == 0u) {
    pstate_.pan = true;
  }
  pstate_.i = true;
  pstate_.f = true;
}

void SystemRegisters::exception_enter_sync(std::uint64_t return_pc,
                                           std::uint32_t ec,
                                           std::uint32_t iss,
                                           bool far_valid,
                                           std::uint64_t far) {
  elr_el1_ = return_pc;
  spsr_el1_ = pstate_bits();
  // This model only supports AArch64 guest execution, so synchronous exceptions
  // are always reported as arising from a 32-bit A64 instruction.
  esr_el1_ = (static_cast<std::uint64_t>(ec & 0x3Fu) << 26) |
             (1ull << 25) |
             (iss & 0x1FFFFFFu);
  far_el1_ = far_valid ? far : 0;
  pstate_.mode = 0x5u;
  spsel_ = 1u;
  pstate_.il = false;
  pstate_.d = true;
  pstate_.a = true;
  if ((sctlr_el1_ & kSctlrEl1Span) == 0u) {
    pstate_.pan = true;
  }
  pstate_.i = true;
  pstate_.f = true;
}

bool SystemRegisters::illegal_exception_return() const {
  if ((spsr_el1_ & (1ull << 4)) != 0u) {
    return true; // AArch32 return state is unsupported in this model.
  }

  switch (static_cast<std::uint8_t>(spsr_el1_ & 0xFu)) {
    case 0x0u: // EL0t
    case 0x4u: // EL1t
    case 0x5u: // EL1h
      return false;
    default:
      return true;
  }
}

std::uint64_t SystemRegisters::exception_return(bool illegal_psr_state) {
  if (illegal_psr_state) {
    set_nzcv(spsr_el1_);
    set_daif(spsr_el1_);
    pstate_.pan = ((spsr_el1_ >> 22) & 1u) != 0;
    pstate_.il = true;
    return elr_el1_;
  }

  set_pstate_bits(spsr_el1_);
  return elr_el1_;
}

void SystemRegisters::daif_set(std::uint8_t imm4) {
  if ((imm4 & 0x8u) != 0) {
    pstate_.d = true;
  }
  if ((imm4 & 0x4u) != 0) {
    pstate_.a = true;
  }
  if ((imm4 & 0x2u) != 0) {
    pstate_.i = true;
  }
  if ((imm4 & 0x1u) != 0) {
    pstate_.f = true;
  }
}

void SystemRegisters::daif_clr(std::uint8_t imm4) {
  if ((imm4 & 0x8u) != 0) {
    pstate_.d = false;
  }
  if ((imm4 & 0x4u) != 0) {
    pstate_.a = false;
  }
  if ((imm4 & 0x2u) != 0) {
    pstate_.i = false;
  }
  if ((imm4 & 0x1u) != 0) {
    pstate_.f = false;
  }
}

std::uint32_t SystemRegisters::make_key(std::uint32_t op0,
                                        std::uint32_t op1,
                                        std::uint32_t crn,
                                        std::uint32_t crm,
                                        std::uint32_t op2) {
  return (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;
}


bool SystemRegisters::save_state(std::ostream& out) const {
  return snapshot_io::write(out, sctlr_el1_) &&
         snapshot_io::write(out, cpacr_el1_) &&
         snapshot_io::write(out, midr_el1_) &&
         snapshot_io::write(out, mpidr_el1_) &&
         snapshot_io::write(out, revidr_el1_) &&
         snapshot_io::write(out, clidr_el1_) &&
         snapshot_io::write(out, ctr_el0_) &&
         snapshot_io::write(out, dczid_el0_) &&
         snapshot_io::write(out, id_aa64pfr0_el1_) &&
         snapshot_io::write(out, id_aa64pfr1_el1_) &&
         snapshot_io::write(out, id_aa64dfr0_el1_) &&
         snapshot_io::write(out, id_aa64dfr1_el1_) &&
         snapshot_io::write(out, id_aa64isar0_el1_) &&
         snapshot_io::write(out, id_aa64isar1_el1_) &&
         snapshot_io::write(out, id_aa64isar2_el1_) &&
         snapshot_io::write(out, id_aa64isar3_el1_) &&
         snapshot_io::write(out, id_aa64zfr0_el1_) &&
         snapshot_io::write(out, id_aa64mmfr0_el1_) &&
         snapshot_io::write(out, id_aa64mmfr1_el1_) &&
         snapshot_io::write(out, id_aa64mmfr2_el1_) &&
         snapshot_io::write(out, id_aa64mmfr3_el1_) &&
         snapshot_io::write(out, csselr_el1_) &&
         snapshot_io::write(out, ccsidr_el1_) &&
         snapshot_io::write(out, ttbr0_el1_) &&
         snapshot_io::write(out, ttbr1_el1_) &&
         snapshot_io::write(out, tcr_el1_) &&
         snapshot_io::write(out, mair_el1_) &&
         snapshot_io::write(out, contextidr_el1_) &&
         snapshot_io::write(out, osdlr_el1_) &&
         snapshot_io::write(out, oslar_el1_) &&
         snapshot_io::write_array(out, dbgbvr_el1_) &&
         snapshot_io::write_array(out, dbgbcr_el1_) &&
         snapshot_io::write_array(out, dbgwvr_el1_) &&
         snapshot_io::write_array(out, dbgwcr_el1_) &&
         snapshot_io::write(out, vbar_el1_) &&
         snapshot_io::write(out, elr_el1_) &&
         snapshot_io::write(out, spsr_el1_) &&
         snapshot_io::write(out, sp_el0_) &&
         snapshot_io::write(out, sp_el1_) &&
         snapshot_io::write(out, spsel_) &&
         snapshot_io::write(out, par_el1_) &&
         snapshot_io::write(out, esr_el1_) &&
         snapshot_io::write(out, far_el1_) &&
         snapshot_io::write(out, mdscr_el1_) &&
         snapshot_io::write(out, pmuserenr_el0_) &&
         snapshot_io::write(out, amuserenr_el0_) &&
         snapshot_io::write(out, tpidr_el0_) &&
         snapshot_io::write(out, tpidr2_el0_) &&
         snapshot_io::write(out, tpidrro_el0_) &&
         snapshot_io::write(out, tpidr_el1_) &&
         snapshot_io::write(out, tpidr_el2_) &&
         snapshot_io::write(out, cntfrq_el0_) &&
         snapshot_io::write(out, cntvct_el0_) &&
         snapshot_io::write(out, cntkctl_el1_) &&
         snapshot_io::write(out, fpcr_) &&
         snapshot_io::write(out, fpsr_) &&
         snapshot_io::write_bool(out, pstate_.n) &&
         snapshot_io::write_bool(out, pstate_.z) &&
         snapshot_io::write_bool(out, pstate_.c) &&
         snapshot_io::write_bool(out, pstate_.v) &&
         snapshot_io::write_bool(out, pstate_.il) &&
         snapshot_io::write_bool(out, pstate_.d) &&
         snapshot_io::write_bool(out, pstate_.a) &&
         snapshot_io::write_bool(out, pstate_.i) &&
         snapshot_io::write_bool(out, pstate_.f) &&
         snapshot_io::write_bool(out, pstate_.pan) &&
         snapshot_io::write(out, pstate_.mode);
}

bool SystemRegisters::load_state(std::istream& in, std::uint32_t version) {
  pstate_.il = false;
  return snapshot_io::read(in, sctlr_el1_) &&
         snapshot_io::read(in, cpacr_el1_) &&
         snapshot_io::read(in, midr_el1_) &&
         snapshot_io::read(in, mpidr_el1_) &&
         snapshot_io::read(in, revidr_el1_) &&
         snapshot_io::read(in, clidr_el1_) &&
         snapshot_io::read(in, ctr_el0_) &&
         snapshot_io::read(in, dczid_el0_) &&
         snapshot_io::read(in, id_aa64pfr0_el1_) &&
         snapshot_io::read(in, id_aa64pfr1_el1_) &&
         snapshot_io::read(in, id_aa64dfr0_el1_) &&
         snapshot_io::read(in, id_aa64dfr1_el1_) &&
         snapshot_io::read(in, id_aa64isar0_el1_) &&
         snapshot_io::read(in, id_aa64isar1_el1_) &&
         snapshot_io::read(in, id_aa64isar2_el1_) &&
         snapshot_io::read(in, id_aa64isar3_el1_) &&
         snapshot_io::read(in, id_aa64zfr0_el1_) &&
         snapshot_io::read(in, id_aa64mmfr0_el1_) &&
         snapshot_io::read(in, id_aa64mmfr1_el1_) &&
         snapshot_io::read(in, id_aa64mmfr2_el1_) &&
         snapshot_io::read(in, id_aa64mmfr3_el1_) &&
         snapshot_io::read(in, csselr_el1_) &&
         snapshot_io::read(in, ccsidr_el1_) &&
         snapshot_io::read(in, ttbr0_el1_) &&
         snapshot_io::read(in, ttbr1_el1_) &&
         snapshot_io::read(in, tcr_el1_) &&
         snapshot_io::read(in, mair_el1_) &&
         snapshot_io::read(in, contextidr_el1_) &&
         snapshot_io::read(in, osdlr_el1_) &&
         snapshot_io::read(in, oslar_el1_) &&
         snapshot_io::read_array(in, dbgbvr_el1_) &&
         snapshot_io::read_array(in, dbgbcr_el1_) &&
         snapshot_io::read_array(in, dbgwvr_el1_) &&
         snapshot_io::read_array(in, dbgwcr_el1_) &&
         snapshot_io::read(in, vbar_el1_) &&
         snapshot_io::read(in, elr_el1_) &&
         snapshot_io::read(in, spsr_el1_) &&
         snapshot_io::read(in, sp_el0_) &&
         snapshot_io::read(in, sp_el1_) &&
         snapshot_io::read(in, spsel_) &&
         snapshot_io::read(in, par_el1_) &&
         snapshot_io::read(in, esr_el1_) &&
         snapshot_io::read(in, far_el1_) &&
         snapshot_io::read(in, mdscr_el1_) &&
         snapshot_io::read(in, pmuserenr_el0_) &&
         snapshot_io::read(in, amuserenr_el0_) &&
         snapshot_io::read(in, tpidr_el0_) &&
         snapshot_io::read(in, tpidr2_el0_) &&
         snapshot_io::read(in, tpidrro_el0_) &&
         snapshot_io::read(in, tpidr_el1_) &&
         snapshot_io::read(in, tpidr_el2_) &&
         snapshot_io::read(in, cntfrq_el0_) &&
         snapshot_io::read(in, cntvct_el0_) &&
         snapshot_io::read(in, cntkctl_el1_) &&
         snapshot_io::read(in, fpcr_) &&
         snapshot_io::read(in, fpsr_) &&
         snapshot_io::read_bool(in, pstate_.n) &&
         snapshot_io::read_bool(in, pstate_.z) &&
         snapshot_io::read_bool(in, pstate_.c) &&
         snapshot_io::read_bool(in, pstate_.v) &&
         ((version < 15) || snapshot_io::read_bool(in, pstate_.il)) &&
         snapshot_io::read_bool(in, pstate_.d) &&
         snapshot_io::read_bool(in, pstate_.a) &&
         snapshot_io::read_bool(in, pstate_.i) &&
         snapshot_io::read_bool(in, pstate_.f) &&
         snapshot_io::read_bool(in, pstate_.pan) &&
         snapshot_io::read(in, pstate_.mode);
}


} // namespace aarchvm
