#include "aarchvm/generic_timer.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <iostream>

namespace aarchvm {

namespace {

bool env_flag_enabled(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && value[0] != '\0';
}

bool timer_trace_enabled() {
  static const bool enabled = env_flag_enabled("AARCHVM_TRACE_TIMER");
  return enabled;
}

void trace_timer(const char* tag,
                 std::uint64_t cpu = 0,
                 std::uint64_t a = 0,
                 std::uint64_t b = 0,
                 std::uint64_t c = 0) {
  if (!timer_trace_enabled()) {
    return;
  }
  std::cerr << tag
            << " cpu=" << std::dec << cpu
            << " a=0x" << std::hex << a
            << " b=0x" << b
            << " c=0x" << c
            << std::dec << '\n';
}

std::uint64_t host_monotonic_ns() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::uint64_t ns_to_cycles(std::uint64_t elapsed_ns, std::uint64_t freq_hz) {
  const std::uint64_t whole_seconds = elapsed_ns / 1000000000ull;
  const std::uint64_t rem_ns = elapsed_ns % 1000000000ull;
  return whole_seconds * freq_hz + ((rem_ns * freq_hz) / 1000000000ull);
}

} // namespace

std::optional<GenericTimer::ClockMode> GenericTimer::parse_clock_mode(std::string_view text) {
  if (text == "step") {
    return ClockMode::GuestStep;
  }
  if (text == "host") {
    return ClockMode::HostMonotonic;
  }
  return std::nullopt;
}

const char* GenericTimer::clock_mode_name(ClockMode mode) {
  switch (mode) {
    case ClockMode::GuestStep:
      return "step";
    case ClockMode::HostMonotonic:
      return "host";
  }
  return "step";
}

void GenericTimer::set_cpu_count(std::size_t cpu_count) {
  cpu_count_ = std::max<std::size_t>(1, cpu_count);
  cpu_states_.resize(cpu_count_);
}

GenericTimer::CpuState& GenericTimer::cpu_state(std::size_t cpu_index) {
  return cpu_states_[std::min(cpu_index, cpu_states_.size() - 1u)];
}

const GenericTimer::CpuState& GenericTimer::cpu_state(std::size_t cpu_index) const {
  return cpu_states_[std::min(cpu_index, cpu_states_.size() - 1u)];
}

std::uint64_t GenericTimer::read(std::uint64_t offset, std::size_t size) {
  if (size != 8 && size != 4) {
    return 0;
  }

  const std::uint64_t current = counter_at_steps(step_anchor_);

  if (offset == 0x00) {
    return current;
  }
  if (offset == 0x08) {
    return compare_;
  }
  if (offset == 0x10) {
    return (enabled_ ? 1ull : 0ull) | (mmio_irq_pending_ ? 2ull : 0ull);
  }
  return 0;
}

void GenericTimer::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (size != 8 && size != 4) {
    return;
  }

  if (offset == 0x08) {
    compare_ = value;
    mmio_irq_pending_ = false;
    fired_ = false;
    update_irq_state();
    if (state_change_observer_) {
      state_change_observer_();
    }
  } else if (offset == 0x10) {
    enabled_ = (value & 1u) != 0;
    if (!enabled_) {
      mmio_irq_pending_ = false;
      fired_ = false;
    }
    if ((value & 2u) != 0) {
      mmio_irq_pending_ = false;
    }
    update_irq_state();
    if (state_change_observer_) {
      state_change_observer_();
    }
  }
}

void GenericTimer::tick(std::uint64_t cycles) {
  counter_ += cycles;
  if (clock_mode_ == ClockMode::HostMonotonic) {
    host_anchor_ns_ = host_monotonic_ns();
  }
  update_irq_state();
}

void GenericTimer::set_clock_mode(ClockMode mode, std::uint64_t steps) {
  counter_ = counter_at_steps(steps);
  step_anchor_ = steps;
  host_anchor_ns_ = host_monotonic_ns();
  clock_mode_ = mode;
  update_irq_state();
  if (state_change_observer_) {
    state_change_observer_();
  }
}

std::uint64_t GenericTimer::counter_at_steps(std::uint64_t steps) const {
  if (clock_mode_ == ClockMode::HostMonotonic) {
    const std::uint64_t now_ns = host_monotonic_ns();
    const std::uint64_t elapsed_ns = now_ns >= host_anchor_ns_ ? (now_ns - host_anchor_ns_) : 0u;
    return counter_ + ns_to_cycles(elapsed_ns, 100000000ull);
  }
  return counter_ + (steps - step_anchor_) * cycles_per_step_;
}

void GenericTimer::sync_to_steps(std::uint64_t steps) {
  counter_ = counter_at_steps(steps);
  step_anchor_ = steps;
  if (clock_mode_ == ClockMode::HostMonotonic) {
    host_anchor_ns_ = host_monotonic_ns();
  }
  update_irq_state();
}

std::uint64_t GenericTimer::read_ctl(const TimerChannel& channel, std::uint64_t current) {
  const bool istatus = channel.enable && (current >= channel.cval);
  return (channel.enable ? 1ull : 0ull) |
         (channel.imask ? 2ull : 0ull) |
         (istatus ? 4ull : 0ull);
}

std::uint64_t GenericTimer::read_tval(const TimerChannel& channel, std::uint64_t current) {
  // CNT{P,V}_TVAL_EL0 expose the low 32 bits of (CVAL - Count), with the
  // architectural 64-bit MRS result zero-extending that 32-bit view.
  return static_cast<std::uint64_t>(static_cast<std::uint32_t>(channel.cval - current));
}

void GenericTimer::write_ctl(TimerChannel& channel, std::uint64_t value) {
  channel.enable = (value & 1u) != 0;
  channel.imask = (value & 2u) != 0;
}

void GenericTimer::write_cval(TimerChannel& channel, std::uint64_t value) {
  channel.cval = value;
}

void GenericTimer::write_tval(TimerChannel& channel, std::uint64_t current, std::uint64_t value) {
  const std::int64_t sval = static_cast<std::int32_t>(value & 0xFFFFFFFFu);
  channel.cval = static_cast<std::uint64_t>(static_cast<std::int64_t>(current) + sval);
}

std::uint64_t GenericTimer::steps_until_channel_irq(const TimerChannel& channel,
                                                    std::uint64_t current,
                                                    std::uint64_t cycles_per_step,
                                                    std::uint64_t max_steps) {
  if (!channel.enable || channel.imask || max_steps == 0u) {
    return max_steps;
  }
  if (current >= channel.cval) {
    return 0u;
  }
  if (cycles_per_step == 0u) {
    return max_steps;
  }
  const std::uint64_t cycles = channel.cval - current;
  const std::uint64_t steps_needed = (cycles + cycles_per_step - 1u) / cycles_per_step;
  return std::min(max_steps, steps_needed);
}

void GenericTimer::update_channel_irq(TimerChannel& channel, std::uint64_t current) {
  channel.irq_pending = channel.enable && !channel.imask && current >= channel.cval;
}

std::uint64_t GenericTimer::steps_until_irq(std::uint64_t steps, std::uint64_t max_steps) const {
  if (clock_mode_ == ClockMode::HostMonotonic) {
    return max_steps;
  }
  std::uint64_t best = max_steps;
  for (std::size_t cpu = 0; cpu < cpu_states_.size(); ++cpu) {
    best = std::min(best, steps_until_irq(cpu, steps, best));
    if (best == 0u) {
      break;
    }
  }
  return best;
}

std::uint64_t GenericTimer::steps_until_irq(std::size_t cpu_index,
                                            std::uint64_t steps,
                                            std::uint64_t max_steps) const {
  if (clock_mode_ == ClockMode::HostMonotonic) {
    return max_steps;
  }
  if (max_steps == 0u) {
    return 0u;
  }
  const auto& state = cpu_state(cpu_index);
  if (mmio_irq_pending_ || state.cntv.irq_pending || state.cntp.irq_pending) {
    return 0u;
  }

  const std::uint64_t current = counter_at_steps(steps);
  std::uint64_t best = max_steps;

  if (enabled_ && !fired_) {
    if (current >= compare_) {
      best = 0u;
    } else if (cycles_per_step_ != 0u) {
      const std::uint64_t cycles = compare_ - current;
      const std::uint64_t steps_needed = (cycles + cycles_per_step_ - 1u) / cycles_per_step_;
      best = std::min(best, steps_needed);
    }
  }

  best = std::min(best, steps_until_channel_irq(state.cntv, current, cycles_per_step_, best));
  best = std::min(best, steps_until_channel_irq(state.cntp, current, cycles_per_step_, best));
  return best;
}

bool GenericTimer::irq_pending_virtual(std::size_t cpu_index) const {
  return cpu_state(cpu_index).cntv.irq_pending;
}

void GenericTimer::clear_virtual_irq(std::size_t cpu_index) {
  cpu_state(cpu_index).cntv.irq_pending = false;
}

bool GenericTimer::irq_pending_physical(std::size_t cpu_index) const {
  return cpu_state(cpu_index).cntp.irq_pending;
}

void GenericTimer::clear_physical_irq(std::size_t cpu_index) {
  cpu_state(cpu_index).cntp.irq_pending = false;
}

std::uint64_t GenericTimer::read_cntv_ctl_el0(std::size_t cpu_index, std::uint64_t steps) const {
  return read_ctl(cpu_state(cpu_index).cntv, counter_at_steps(steps));
}

void GenericTimer::write_cntv_ctl_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  auto& cntv = cpu_state(cpu_index).cntv;
  write_ctl(cntv, value);
  trace_timer("TIMER-V-CTL", cpu_index, counter_, value, cntv.cval);
  update_irq_state();
  if (state_change_observer_) {
    state_change_observer_();
  }
}

std::uint64_t GenericTimer::read_cntv_cval_el0(std::size_t cpu_index) const {
  return cpu_state(cpu_index).cntv.cval;
}

void GenericTimer::write_cntv_cval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  auto& cntv = cpu_state(cpu_index).cntv;
  write_cval(cntv, value);
  trace_timer("TIMER-V-CVAL", cpu_index, counter_, value, 0);
  update_irq_state();
  if (state_change_observer_) {
    state_change_observer_();
  }
}

std::uint64_t GenericTimer::read_cntv_tval_el0(std::size_t cpu_index, std::uint64_t steps) const {
  return read_tval(cpu_state(cpu_index).cntv, counter_at_steps(steps));
}

void GenericTimer::write_cntv_tval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  auto& cntv = cpu_state(cpu_index).cntv;
  write_tval(cntv, counter_, value);
  trace_timer("TIMER-V-TVAL", cpu_index, counter_, value, cntv.cval);
  update_irq_state();
  if (state_change_observer_) {
    state_change_observer_();
  }
}

std::uint64_t GenericTimer::read_cntp_ctl_el0(std::size_t cpu_index, std::uint64_t steps) const {
  return read_ctl(cpu_state(cpu_index).cntp, counter_at_steps(steps));
}

void GenericTimer::write_cntp_ctl_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  auto& cntp = cpu_state(cpu_index).cntp;
  write_ctl(cntp, value);
  trace_timer("TIMER-P-CTL", cpu_index, counter_, value, cntp.cval);
  update_irq_state();
  if (state_change_observer_) {
    state_change_observer_();
  }
}

std::uint64_t GenericTimer::read_cntp_cval_el0(std::size_t cpu_index) const {
  return cpu_state(cpu_index).cntp.cval;
}

void GenericTimer::write_cntp_cval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  auto& cntp = cpu_state(cpu_index).cntp;
  write_cval(cntp, value);
  trace_timer("TIMER-P-CVAL", cpu_index, counter_, value, 0);
  update_irq_state();
  if (state_change_observer_) {
    state_change_observer_();
  }
}

std::uint64_t GenericTimer::read_cntp_tval_el0(std::size_t cpu_index, std::uint64_t steps) const {
  return read_tval(cpu_state(cpu_index).cntp, counter_at_steps(steps));
}

void GenericTimer::write_cntp_tval_el0(std::size_t cpu_index, std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  auto& cntp = cpu_state(cpu_index).cntp;
  write_tval(cntp, counter_, value);
  trace_timer("TIMER-P-TVAL", cpu_index, counter_, value, cntp.cval);
  update_irq_state();
  if (state_change_observer_) {
    state_change_observer_();
  }
}

void GenericTimer::update_irq_state() {
  if (enabled_ && !mmio_irq_pending_ && !fired_ && counter_ >= compare_) {
    mmio_irq_pending_ = true;
    fired_ = true;
    trace_timer("TIMER-MMIO-IRQ", 0, counter_, compare_, 0);
  }

  for (std::size_t cpu = 0; cpu < cpu_states_.size(); ++cpu) {
    auto& state = cpu_states_[cpu];
    const bool old_cntv_pending = state.cntv.irq_pending;
    update_channel_irq(state.cntv, counter_);
    if (!old_cntv_pending && state.cntv.irq_pending) {
      trace_timer("TIMER-V-IRQ", cpu, counter_, state.cntv.cval, read_ctl(state.cntv, counter_));
    }

    const bool old_cntp_pending = state.cntp.irq_pending;
    update_channel_irq(state.cntp, counter_);
    if (!old_cntp_pending && state.cntp.irq_pending) {
      trace_timer("TIMER-P-IRQ", cpu, counter_, state.cntp.cval, read_ctl(state.cntp, counter_));
    }
  }
}

bool GenericTimer::save_state(std::ostream& out) const {
  const std::uint32_t snapshot_cpu_count = static_cast<std::uint32_t>(cpu_states_.size());
  if (!snapshot_io::write(out, counter_) ||
      !snapshot_io::write(out, compare_) ||
      !snapshot_io::write_bool(out, enabled_) ||
      !snapshot_io::write_bool(out, mmio_irq_pending_) ||
      !snapshot_io::write_bool(out, fired_) ||
      !snapshot_io::write(out, snapshot_cpu_count)) {
    return false;
  }
  for (const auto& state : cpu_states_) {
    if (!snapshot_io::write(out, state.cntv.cval) ||
        !snapshot_io::write_bool(out, state.cntv.enable) ||
        !snapshot_io::write_bool(out, state.cntv.imask) ||
        !snapshot_io::write_bool(out, state.cntv.fired) ||
        !snapshot_io::write_bool(out, state.cntv.irq_pending) ||
        !snapshot_io::write(out, state.cntp.cval) ||
        !snapshot_io::write_bool(out, state.cntp.enable) ||
        !snapshot_io::write_bool(out, state.cntp.imask) ||
        !snapshot_io::write_bool(out, state.cntp.fired) ||
        !snapshot_io::write_bool(out, state.cntp.irq_pending)) {
      return false;
    }
  }
  return true;
}

bool GenericTimer::load_state(std::istream& in, std::uint32_t version) {
  step_anchor_ = 0;
  cycles_per_step_ = 1;
  cpu_count_ = 1;
  cpu_states_.assign(1, CpuState{});
  clock_mode_ = ClockMode::GuestStep;
  host_anchor_ns_ = host_monotonic_ns();

  if (!snapshot_io::read(in, counter_) ||
      !snapshot_io::read(in, compare_) ||
      !snapshot_io::read_bool(in, enabled_) ||
      !snapshot_io::read_bool(in, mmio_irq_pending_) ||
      !snapshot_io::read_bool(in, fired_)) {
    return false;
  }

  if (version >= 3) {
    std::uint32_t snapshot_cpu_count = 0;
    if (!snapshot_io::read(in, snapshot_cpu_count) || snapshot_cpu_count == 0u) {
      return false;
    }
    cpu_count_ = snapshot_cpu_count;
    cpu_states_.assign(cpu_count_, CpuState{});
    for (auto& state : cpu_states_) {
      if (!snapshot_io::read(in, state.cntv.cval) ||
          !snapshot_io::read_bool(in, state.cntv.enable) ||
          !snapshot_io::read_bool(in, state.cntv.imask) ||
          !snapshot_io::read_bool(in, state.cntv.fired) ||
          !snapshot_io::read_bool(in, state.cntv.irq_pending) ||
          !snapshot_io::read(in, state.cntp.cval) ||
          !snapshot_io::read_bool(in, state.cntp.enable) ||
          !snapshot_io::read_bool(in, state.cntp.imask) ||
          !snapshot_io::read_bool(in, state.cntp.fired) ||
          !snapshot_io::read_bool(in, state.cntp.irq_pending)) {
        return false;
      }
    }
  } else {
    auto& state = cpu_states_[0];
    if (!snapshot_io::read(in, state.cntv.cval) ||
        !snapshot_io::read_bool(in, state.cntv.enable) ||
        !snapshot_io::read_bool(in, state.cntv.imask) ||
        !snapshot_io::read_bool(in, state.cntv.fired)) {
      return false;
    }

    if (version >= 2) {
      if (!snapshot_io::read_bool(in, state.cntv.irq_pending) ||
          !snapshot_io::read(in, state.cntp.cval) ||
          !snapshot_io::read_bool(in, state.cntp.enable) ||
          !snapshot_io::read_bool(in, state.cntp.imask) ||
          !snapshot_io::read_bool(in, state.cntp.fired) ||
          !snapshot_io::read_bool(in, state.cntp.irq_pending)) {
        return false;
      }
    }
  }

  update_irq_state();
  return true;
}

} // namespace aarchvm
