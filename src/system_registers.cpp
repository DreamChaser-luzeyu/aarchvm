#include "aarchvm/system_registers.hpp"

namespace aarchvm {

namespace {

constexpr std::uint32_t SysReg(std::uint32_t op0,
                               std::uint32_t op1,
                               std::uint32_t crn,
                               std::uint32_t crm,
                               std::uint32_t op2) {
  return (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;
}

} // namespace

void SystemRegisters::reset() {
  sctlr_el1_ = 0x30D00800;
  cpacr_el1_ = 0;
  midr_el1_ = 0x00000000410FD034ull;
  clidr_el1_ = 0;
  ctr_el0_ = 0x8444C004ull;
  id_aa64mmfr2_el1_ = 0;
  csselr_el1_ = 0;
  ccsidr_el1_ = 0;
  ttbr0_el1_ = 0;
  ttbr1_el1_ = 0;
  tcr_el1_ = 0;
  mair_el1_ = 0;
  vbar_el1_ = 0;
  elr_el1_ = 0;
  spsr_el1_ = 0;
  sp_el0_ = 0;
  sp_el1_ = 0;
  spsel_ = 1;
  par_el1_ = 0;
  esr_el1_ = 0;
  far_el1_ = 0;
  cntfrq_el0_ = 100000000;
  cntvct_el0_ = 0;
  pstate_ = {};
}

bool SystemRegisters::read(std::uint32_t op0,
                           std::uint32_t op1,
                           std::uint32_t crn,
                           std::uint32_t crm,
                           std::uint32_t op2,
                           std::uint64_t& value) const {
  switch (make_key(op0, op1, crn, crm, op2)) {
  case SysReg(3, 0, 0, 0, 0): value = midr_el1_; return true;
  case SysReg(3, 1, 0, 0, 0): value = ccsidr_el1_; return true;
  case SysReg(3, 0, 1, 0, 0): value = sctlr_el1_; return true;
  case SysReg(3, 0, 1, 0, 2): value = cpacr_el1_; return true;
  case SysReg(3, 2, 0, 0, 0): value = csselr_el1_; return true;
  case SysReg(3, 0, 2, 0, 0): value = ttbr0_el1_; return true;
  case SysReg(3, 0, 2, 0, 1): value = ttbr1_el1_; return true;
  case SysReg(3, 0, 2, 0, 2): value = tcr_el1_; return true;
  case SysReg(3, 1, 0, 0, 1): value = clidr_el1_; return true;
  case SysReg(3, 0, 0, 7, 2): value = id_aa64mmfr2_el1_; return true;
  case SysReg(3, 0, 10, 2, 0): value = mair_el1_; return true;
  case SysReg(3, 0, 12, 0, 0): value = vbar_el1_; return true;
  case SysReg(3, 0, 4, 0, 1): value = elr_el1_; return true;
  case SysReg(3, 0, 4, 0, 0): value = spsr_el1_; return true;
  case SysReg(3, 0, 5, 2, 0): value = esr_el1_; return true;
  case SysReg(3, 0, 6, 0, 0): value = far_el1_; return true;
  case SysReg(3, 0, 4, 1, 0): value = sp_el0_; return true;
  case SysReg(3, 4, 4, 1, 0): value = sp_el1_; return true;
  case SysReg(3, 0, 7, 4, 0): value = par_el1_; return true;
  case SysReg(3, 3, 4, 2, 1): value = daif(); return true;
  case SysReg(3, 0, 4, 2, 0): value = spsel_; return true;
  case SysReg(3, 3, 0, 0, 1): value = ctr_el0_; return true;
  case SysReg(3, 3, 14, 0, 0): value = cntfrq_el0_; return true;
  case SysReg(3, 3, 14, 0, 1): value = cntvct_el0_; return true; // CNTPCT_EL0 (minimal alias)
  case SysReg(3, 3, 14, 0, 2): value = cntvct_el0_; return true;
  case SysReg(3, 3, 4, 2, 0): value = nzcv(); return true;
  case SysReg(3, 0, 4, 2, 2): value = 0x4ull; return true; // CURRENTEL = EL1
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
  switch (make_key(op0, op1, crn, crm, op2)) {
  case SysReg(3, 0, 1, 0, 0): sctlr_el1_ = value; return true;
  case SysReg(3, 0, 1, 0, 2): cpacr_el1_ = value; return true;
  case SysReg(3, 2, 0, 0, 0): csselr_el1_ = value; return true;
  case SysReg(3, 0, 2, 0, 0): ttbr0_el1_ = value; return true;
  case SysReg(3, 0, 2, 0, 1): ttbr1_el1_ = value; return true;
  case SysReg(3, 0, 2, 0, 2): tcr_el1_ = value; return true;
  case SysReg(3, 0, 10, 2, 0): mair_el1_ = value; return true;
  case SysReg(3, 0, 12, 0, 0): vbar_el1_ = value; return true;
  case SysReg(3, 0, 4, 0, 1): elr_el1_ = value; return true;
  case SysReg(3, 0, 4, 0, 0): spsr_el1_ = value; return true;
  case SysReg(3, 0, 5, 2, 0): esr_el1_ = value; return true;
  case SysReg(3, 0, 6, 0, 0): far_el1_ = value; return true;
  case SysReg(3, 0, 4, 1, 0): sp_el0_ = value; return true;
  case SysReg(3, 4, 4, 1, 0): sp_el1_ = value; return true;
  case SysReg(3, 0, 7, 4, 0): par_el1_ = value; return true;
  case SysReg(3, 3, 4, 2, 1): set_daif(value); return true;
  case SysReg(3, 0, 4, 2, 0): set_spsel(value); return true;
  case SysReg(3, 3, 4, 2, 0): set_nzcv(value); return true;
  default:
    return false;
  }
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

void SystemRegisters::set_daif(std::uint64_t value) {
  pstate_.d = ((value >> 9) & 1u) != 0;
  pstate_.a = ((value >> 8) & 1u) != 0;
  pstate_.i = ((value >> 7) & 1u) != 0;
  pstate_.f = ((value >> 6) & 1u) != 0;
}

void SystemRegisters::exception_enter_irq(std::uint64_t return_pc) {
  elr_el1_ = return_pc;
  spsr_el1_ = nzcv() | daif();
  esr_el1_ = 0;
  pstate_.i = true;
}

void SystemRegisters::exception_enter_sync(std::uint64_t return_pc,
                                           std::uint32_t ec,
                                           std::uint32_t iss,
                                           bool far_valid,
                                           std::uint64_t far) {
  elr_el1_ = return_pc;
  spsr_el1_ = nzcv() | daif();
  esr_el1_ = (static_cast<std::uint64_t>(ec & 0x3Fu) << 26) | (iss & 0x1FFFFFFu);
  far_el1_ = far_valid ? far : 0;
  pstate_.i = true;
}

std::uint64_t SystemRegisters::exception_return() {
  set_nzcv(spsr_el1_);
  set_daif(spsr_el1_);
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

} // namespace aarchvm
