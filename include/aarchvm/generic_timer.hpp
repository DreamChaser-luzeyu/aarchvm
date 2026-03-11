#pragma once

#include "aarchvm/device.hpp"

#include <cstdint>
#include <iosfwd>

namespace aarchvm {

class GenericTimer final : public Device {
public:
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void tick(std::uint64_t cycles);
  [[nodiscard]] std::uint64_t counter() const { return counter_; }
  [[nodiscard]] bool irq_pending() const { return irq_pending_; }
  void clear_irq() { irq_pending_ = false; }

  [[nodiscard]] std::uint64_t read_cntv_ctl_el0() const;
  void write_cntv_ctl_el0(std::uint64_t value);
  [[nodiscard]] std::uint64_t read_cntv_cval_el0() const { return cntv_cval_el0_; }
  void write_cntv_cval_el0(std::uint64_t value);
  [[nodiscard]] std::uint64_t read_cntv_tval_el0() const;
  void write_cntv_tval_el0(std::uint64_t value);

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

private:
  std::uint64_t counter_ = 0;
  std::uint64_t compare_ = 0;
  bool enabled_ = false;
  bool irq_pending_ = false;
  bool fired_ = false;
  std::uint64_t cntv_cval_el0_ = 0;
  bool cntv_enable_ = false;
  bool cntv_imask_ = false;
  bool cntv_fired_ = false;
};

} // namespace aarchvm
