#pragma once

#include "aarchvm/device.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace aarchvm {

class GenericTimer final : public Device {
public:
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void set_cpu_count(std::size_t cpu_count);
  [[nodiscard]] std::size_t cpu_count() const { return cpu_count_; }

  void tick(std::uint64_t cycles);
  void set_cycles_per_step(std::uint64_t value) { cycles_per_step_ = value; }
  void rebase_to_steps(std::uint64_t steps) { step_anchor_ = steps; }
  void sync_to_steps(std::uint64_t steps);
  [[nodiscard]] std::uint64_t counter() const { return counter_; }
  [[nodiscard]] std::uint64_t counter_at_steps(std::uint64_t steps) const;
  [[nodiscard]] std::uint64_t steps_until_irq(std::uint64_t steps, std::uint64_t max_steps) const;
  [[nodiscard]] std::uint64_t steps_until_irq(std::size_t cpu_index,
                                              std::uint64_t steps,
                                              std::uint64_t max_steps) const;

  [[nodiscard]] bool irq_pending() const { return mmio_irq_pending_; }
  void clear_irq() { mmio_irq_pending_ = false; }
  [[nodiscard]] bool irq_pending_virtual(std::size_t cpu_index) const;
  void clear_virtual_irq(std::size_t cpu_index);
  [[nodiscard]] bool irq_pending_physical(std::size_t cpu_index) const;
  void clear_physical_irq(std::size_t cpu_index);

  [[nodiscard]] std::uint64_t read_cntv_ctl_el0(std::size_t cpu_index, std::uint64_t steps) const;
  void write_cntv_ctl_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value);
  [[nodiscard]] std::uint64_t read_cntv_cval_el0(std::size_t cpu_index) const;
  void write_cntv_cval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value);
  [[nodiscard]] std::uint64_t read_cntv_tval_el0(std::size_t cpu_index, std::uint64_t steps) const;
  void write_cntv_tval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value);

  [[nodiscard]] std::uint64_t read_cntp_ctl_el0(std::size_t cpu_index, std::uint64_t steps) const;
  void write_cntp_ctl_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value);
  [[nodiscard]] std::uint64_t read_cntp_cval_el0(std::size_t cpu_index) const;
  void write_cntp_cval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value);
  [[nodiscard]] std::uint64_t read_cntp_tval_el0(std::size_t cpu_index, std::uint64_t steps) const;
  void write_cntp_tval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value);

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in, std::uint32_t version = 3);

private:
  struct TimerChannel {
    std::uint64_t cval = 0;
    bool enable = false;
    bool imask = false;
    bool fired = false;
    bool irq_pending = false;
  };

  struct CpuState {
    TimerChannel cntv{};
    TimerChannel cntp{};
  };

  static std::uint64_t read_ctl(const TimerChannel& channel, std::uint64_t current);
  static std::uint64_t read_tval(const TimerChannel& channel, std::uint64_t current);
  static void write_ctl(TimerChannel& channel, std::uint64_t value);
  static void write_cval(TimerChannel& channel, std::uint64_t value);
  static void write_tval(TimerChannel& channel, std::uint64_t current, std::uint64_t value);
  static std::uint64_t steps_until_channel_irq(const TimerChannel& channel,
                                               std::uint64_t current,
                                               std::uint64_t cycles_per_step,
                                               std::uint64_t max_steps);
  static void update_channel_irq(TimerChannel& channel, std::uint64_t current);
  void update_irq_state();
  [[nodiscard]] CpuState& cpu_state(std::size_t cpu_index);
  [[nodiscard]] const CpuState& cpu_state(std::size_t cpu_index) const;

  std::size_t cpu_count_ = 1;
  std::vector<CpuState> cpu_states_{1};
  std::uint64_t counter_ = 0;
  std::uint64_t step_anchor_ = 0;
  std::uint64_t cycles_per_step_ = 1;

  std::uint64_t compare_ = 0;
  bool enabled_ = false;
  bool mmio_irq_pending_ = false;
  bool fired_ = false;
};

} // namespace aarchvm
