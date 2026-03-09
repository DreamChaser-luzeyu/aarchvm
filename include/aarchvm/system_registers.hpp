#pragma once

#include <cstdint>

namespace aarchvm {

class SystemRegisters {
public:
  struct PState {
    bool n = false;
    bool z = false;
    bool c = false;
    bool v = false;
    bool d = false;
    bool a = false;
    bool i = false;
    bool f = false;
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
  [[nodiscard]] bool mmu_enabled() const { return (sctlr_el1_ & 1u) != 0; }
  [[nodiscard]] std::uint64_t ttbr0_el1() const { return ttbr0_el1_; }
  [[nodiscard]] std::uint64_t ttbr1_el1() const { return ttbr1_el1_; }
  [[nodiscard]] std::uint64_t tcr_el1() const { return tcr_el1_; }
  void set_par_el1(std::uint64_t value) { par_el1_ = value; }
  [[nodiscard]] std::uint64_t elr_el1() const { return elr_el1_; }
  void set_elr_el1(std::uint64_t value) { elr_el1_ = value; }
  [[nodiscard]] std::uint64_t spsr_el1() const { return spsr_el1_; }
  [[nodiscard]] std::uint64_t esr_el1() const { return esr_el1_; }
  [[nodiscard]] std::uint64_t far_el1() const { return far_el1_; }

  void set_cntvct(std::uint64_t value) { cntvct_el0_ = value; }
  [[nodiscard]] std::uint64_t vbar_el1() const { return vbar_el1_; }
  [[nodiscard]] bool irq_masked() const { return pstate_.i; }
  [[nodiscard]] bool use_sp_elx() const { return spsel_ != 0; }
  void set_spsel(std::uint64_t value) { spsel_ = value & 1u; }

  void exception_enter_irq(std::uint64_t return_pc);
  void exception_enter_sync(std::uint64_t return_pc, std::uint32_t ec, std::uint32_t iss, bool far_valid, std::uint64_t far);
  [[nodiscard]] std::uint64_t exception_return();
  void daif_set(std::uint8_t imm4);
  void daif_clr(std::uint8_t imm4);

private:
  static std::uint32_t make_key(std::uint32_t op0,
                                std::uint32_t op1,
                                std::uint32_t crn,
                                std::uint32_t crm,
                                std::uint32_t op2);

  std::uint64_t sctlr_el1_ = 0x30D00800;
  std::uint64_t cpacr_el1_ = 0;
  std::uint64_t midr_el1_ = 0x00000000410FD034ull; // Cortex-A53 r0p4-like default
  std::uint64_t clidr_el1_ = 0;
  std::uint64_t ctr_el0_ = 0x8444C004ull;
  std::uint64_t id_aa64mmfr2_el1_ = 0;
  std::uint64_t csselr_el1_ = 0;
  std::uint64_t ccsidr_el1_ = 0;
  std::uint64_t ttbr0_el1_ = 0;
  std::uint64_t ttbr1_el1_ = 0;
  std::uint64_t tcr_el1_ = 0;
  std::uint64_t mair_el1_ = 0;
  std::uint64_t vbar_el1_ = 0;
  std::uint64_t elr_el1_ = 0;
  std::uint64_t spsr_el1_ = 0;
  std::uint64_t sp_el0_ = 0;
  std::uint64_t sp_el1_ = 0;
  std::uint64_t spsel_ = 1;
  std::uint64_t par_el1_ = 0;
  std::uint64_t esr_el1_ = 0;
  std::uint64_t far_el1_ = 0;
  std::uint64_t cntfrq_el0_ = 100000000;
  std::uint64_t cntvct_el0_ = 0;
  PState pstate_{};
};

} // namespace aarchvm
