#pragma once

#include <array>
#include <cstdint>
#include <iosfwd>

namespace aarchvm {

class SystemRegisters {
public:
  struct PState {
    bool n = false;
    bool z = false;
    bool c = false;
    bool v = false;
    bool il = false;
    bool d = false;
    bool a = false;
    bool i = false;
    bool f = false;
    bool pan = false;
    std::uint8_t mode = 0x5; // PSR_MODE_EL1h
  };

  void reset();

  [[nodiscard]] bool read(std::uint32_t op0,
                          std::uint32_t op1,
                          std::uint32_t crn,
                          std::uint32_t crm,
                          std::uint32_t op2,
                          std::uint64_t& value) const;

  [[nodiscard]] bool write(std::uint32_t op0,
                           std::uint32_t op1,
                           std::uint32_t crn,
                           std::uint32_t crm,
                           std::uint32_t op2,
                           std::uint64_t value);

  [[nodiscard]] PState pstate() const { return pstate_; }
  void set_pstate(PState value) { pstate_ = value; }

  [[nodiscard]] std::uint64_t nzcv() const;
  void set_nzcv(std::uint64_t value);
  [[nodiscard]] std::uint64_t daif() const;
  void set_daif(std::uint64_t value);
  [[nodiscard]] std::uint64_t pstate_bits() const;
  void set_pstate_bits(std::uint64_t value);
  [[nodiscard]] bool pan() const { return pstate_.pan; }
  void set_pan(bool value) { pstate_.pan = value; }
  [[nodiscard]] bool mmu_enabled() const { return (sctlr_el1_ & 1u) != 0; }
  [[nodiscard]] std::uint64_t sctlr_el1() const { return sctlr_el1_; }
  [[nodiscard]] std::uint64_t cpacr_el1() const { return cpacr_el1_; }
  [[nodiscard]] std::uint64_t ttbr0_el1() const { return ttbr0_el1_; }
  [[nodiscard]] std::uint64_t ttbr1_el1() const { return ttbr1_el1_; }
  [[nodiscard]] std::uint64_t tcr_el1() const { return tcr_el1_; }
  [[nodiscard]] std::uint64_t mair_el1() const { return mair_el1_; }
  [[nodiscard]] std::uint64_t contextidr_el1() const { return contextidr_el1_; }
  [[nodiscard]] std::uint64_t mpidr_el1() const { return mpidr_el1_; }
  void set_mpidr_el1(std::uint64_t value) { mpidr_el1_ = value; }
  [[nodiscard]] std::uint64_t id_aa64pfr0_el1() const { return id_aa64pfr0_el1_; }
  [[nodiscard]] std::uint64_t id_aa64isar0_el1() const { return id_aa64isar0_el1_; }
  [[nodiscard]] std::uint64_t id_aa64mmfr0_el1() const { return id_aa64mmfr0_el1_; }
  void set_par_el1(std::uint64_t value) { par_el1_ = value; }
  [[nodiscard]] std::uint64_t elr_el1() const { return elr_el1_; }
  void set_elr_el1(std::uint64_t value) { elr_el1_ = value; }
  [[nodiscard]] std::uint64_t spsr_el1() const { return spsr_el1_; }
  [[nodiscard]] std::uint64_t esr_el1() const { return esr_el1_; }
  [[nodiscard]] std::uint64_t far_el1() const { return far_el1_; }
  [[nodiscard]] std::uint64_t mdscr_el1() const { return mdscr_el1_; }
  [[nodiscard]] std::uint64_t tpidr_el0() const { return tpidr_el0_; }
  [[nodiscard]] std::uint64_t tpidr2_el0() const { return tpidr2_el0_; }
  [[nodiscard]] std::uint64_t tpidrro_el0() const { return tpidrro_el0_; }
  [[nodiscard]] std::uint64_t tpidr_el1() const { return tpidr_el1_; }
  [[nodiscard]] std::uint64_t tpidr_el2() const { return tpidr_el2_; }
  [[nodiscard]] std::uint64_t fpcr() const { return fpcr_; }
  [[nodiscard]] std::uint64_t fpsr() const { return fpsr_; }
  void set_fpcr(std::uint64_t value) { fpcr_ = value & 0xFFFFFFFFu; }
  void set_fpsr(std::uint64_t value) { fpsr_ = value & 0xFFFFFFFFu; }
  void fp_or_fpsr(std::uint64_t bits) { fpsr_ = (fpsr_ | bits) & 0xFFFFFFFFu; }

  void set_cntvct(std::uint64_t value) { cntvct_el0_ = value; }
  [[nodiscard]] std::uint64_t vbar_el1() const { return vbar_el1_; }
  [[nodiscard]] bool irq_masked() const { return pstate_.i; }
  [[nodiscard]] bool illegal_execution_state() const { return pstate_.il; }
  [[nodiscard]] std::uint8_t current_el() const { return static_cast<std::uint8_t>((pstate_.mode >> 2) & 0x3u); }
  [[nodiscard]] bool in_el0() const { return current_el() == 0u; }
  [[nodiscard]] bool current_uses_sp_el0() const { return pstate_.mode == 0x0u || pstate_.mode == 0x4u; }
  [[nodiscard]] bool use_sp_elx() const { return !current_uses_sp_el0(); }
  [[nodiscard]] std::uint64_t sp_el0() const { return sp_el0_; }
  void set_sp_el0(std::uint64_t value) { sp_el0_ = value; }
  [[nodiscard]] std::uint64_t sp_el1() const { return sp_el1_; }
  void set_sp_el1(std::uint64_t value) { sp_el1_ = value; }
  void set_spsel(std::uint64_t value);

  void exception_enter_irq(std::uint64_t return_pc);
  void exception_enter_sync(std::uint64_t return_pc, std::uint32_t ec, std::uint32_t iss, bool far_valid, std::uint64_t far);
  [[nodiscard]] bool illegal_exception_return() const;
  [[nodiscard]] std::uint64_t exception_return(bool illegal_psr_state);
  void daif_set(std::uint8_t imm4);
  void daif_clr(std::uint8_t imm4);

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in, std::uint32_t version = 15);

private:
  static std::uint32_t make_key(std::uint32_t op0,
                                std::uint32_t op1,
                                std::uint32_t crn,
                                std::uint32_t crm,
                                std::uint32_t op2);

  std::uint64_t sctlr_el1_ = 0x30D00800;
  std::uint64_t cpacr_el1_ = 0;
  std::uint64_t midr_el1_ = 0x00000000410FD034ull; // Cortex-A53 r0p4-like default
  std::uint64_t mpidr_el1_ = 0x0000000080000000ull;
  std::uint64_t revidr_el1_ = 0;
  std::uint64_t clidr_el1_ = 0;
  std::uint64_t ctr_el0_ = 0x8444C004ull;
  std::uint64_t dczid_el0_ = 0x4ull;
  std::uint64_t id_aa64pfr0_el1_ = 0x0000000000000011ull;
  std::uint64_t id_aa64pfr1_el1_ = 0;
  std::uint64_t id_aa64dfr0_el1_ = 0;
  std::uint64_t id_aa64dfr1_el1_ = 0;
  std::uint64_t id_aa64isar0_el1_ = 0x0000000000210000ull;
  std::uint64_t id_aa64isar1_el1_ = 0;
  std::uint64_t id_aa64isar2_el1_ = 0;
  std::uint64_t id_aa64isar3_el1_ = 0;
  std::uint64_t id_aa64zfr0_el1_ = 0;
  std::uint64_t id_aa64mmfr0_el1_ = 0x000001110F000005ull;
  std::uint64_t id_aa64mmfr1_el1_ = 0;
  std::uint64_t id_aa64mmfr2_el1_ = 0;
  std::uint64_t id_aa64mmfr3_el1_ = 0;
  std::uint64_t csselr_el1_ = 0;
  std::uint64_t ccsidr_el1_ = 0;
  std::uint64_t ttbr0_el1_ = 0;
  std::uint64_t ttbr1_el1_ = 0;
  std::uint64_t tcr_el1_ = 0;
  std::uint64_t mair_el1_ = 0;
  std::uint64_t contextidr_el1_ = 0;
  std::uint64_t osdlr_el1_ = 0;
  std::uint64_t oslar_el1_ = 0;
  std::array<std::uint64_t, 16> dbgbvr_el1_{};
  std::array<std::uint64_t, 16> dbgbcr_el1_{};
  std::array<std::uint64_t, 16> dbgwvr_el1_{};
  std::array<std::uint64_t, 16> dbgwcr_el1_{};
  std::uint64_t vbar_el1_ = 0;
  std::uint64_t elr_el1_ = 0;
  std::uint64_t spsr_el1_ = 0;
  std::uint64_t sp_el0_ = 0;
  std::uint64_t sp_el1_ = 0;
  std::uint64_t spsel_ = 1;
  std::uint64_t par_el1_ = 0;
  std::uint64_t esr_el1_ = 0;
  std::uint64_t far_el1_ = 0;
  std::uint64_t mdscr_el1_ = 0;
  std::uint64_t pmuserenr_el0_ = 0;
  std::uint64_t amuserenr_el0_ = 0;
  std::uint64_t tpidr_el0_ = 0;
  std::uint64_t tpidr2_el0_ = 0;
  std::uint64_t tpidrro_el0_ = 0;
  std::uint64_t tpidr_el1_ = 0;
  std::uint64_t tpidr_el2_ = 0;
  std::uint64_t cntfrq_el0_ = 100000000;
  std::uint64_t cntvct_el0_ = 0;
  std::uint64_t cntkctl_el1_ = 0;
  std::uint64_t fpcr_ = 0;
  std::uint64_t fpsr_ = 0;
  PState pstate_{};
};

} // namespace aarchvm
