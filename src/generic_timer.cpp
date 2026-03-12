#include "aarchvm/generic_timer.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <iostream>
#include <limits>

namespace aarchvm {

namespace {

bool timer_trace_enabled() {
  return std::getenv("AARCHVM_TRACE_TIMER") != nullptr;
}

void trace_timer(const char* tag, std::uint64_t a = 0, std::uint64_t b = 0, std::uint64_t c = 0) {
  if (!timer_trace_enabled()) {
    return;
  }
  std::cerr << tag
            << " a=0x" << std::hex << a
            << " b=0x" << b
            << " c=0x" << c
            << std::dec << '\n';
}

} // namespace

std::uint64_t GenericTimer::read(std::uint64_t offset, std::size_t size) {
  if (size != 8 && size != 4) {
    return 0;
  }

  if (offset == 0x00) {
    return counter_;
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
    fired_ = false;
    update_irq_state();
  } else if (offset == 0x10) {
    enabled_ = (value & 1u) != 0;
    if (!enabled_) {
      fired_ = false;
    }
    if ((value & 2u) != 0) {
      mmio_irq_pending_ = false;
    }
    update_irq_state();
  }
}

void GenericTimer::tick(std::uint64_t cycles) {
  counter_ += cycles;
  update_irq_state();
}

std::uint64_t GenericTimer::counter_at_steps(std::uint64_t steps) const {
  return counter_ + (steps - step_anchor_) * cycles_per_step_;
}

void GenericTimer::sync_to_steps(std::uint64_t steps) {
  counter_ = counter_at_steps(steps);
  step_anchor_ = steps;
  update_irq_state();
}

std::uint64_t GenericTimer::read_ctl(const TimerChannel& channel, std::uint64_t current) {
  const bool istatus = channel.enable && (current >= channel.cval);
  return (channel.enable ? 1ull : 0ull) |
         (channel.imask ? 2ull : 0ull) |
         (istatus ? 4ull : 0ull);
}

std::uint64_t GenericTimer::read_tval(const TimerChannel& channel, std::uint64_t current) {
  const std::int64_t delta = static_cast<std::int64_t>(channel.cval - current);
  return static_cast<std::uint32_t>(delta);
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
  if (max_steps == 0u) {
    return 0u;
  }
  if (mmio_irq_pending_ || cntv_irq_pending_ || cntp_irq_pending_) {
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

  best = std::min(best, steps_until_channel_irq(cntv_, current, cycles_per_step_, max_steps));
  best = std::min(best, steps_until_channel_irq(cntp_, current, cycles_per_step_, max_steps));
  return best;
}

std::uint64_t GenericTimer::read_cntv_ctl_el0(std::uint64_t steps) const {
  return read_ctl(cntv_, counter_at_steps(steps));
}

void GenericTimer::write_cntv_ctl_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  write_ctl(cntv_, value);
  trace_timer("TIMER-V-CTL", counter_, value, cntv_.cval);
  cntv_enable_ = cntv_.enable;
  cntv_imask_ = cntv_.imask;
  cntv_fired_ = cntv_.fired;
  cntv_irq_pending_ = cntv_.irq_pending;
  update_irq_state();
}

void GenericTimer::write_cntv_cval_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  write_cval(cntv_, value);
  trace_timer("TIMER-V-CVAL", counter_, value, 0);
  cntv_cval_el0_ = cntv_.cval;
  cntv_fired_ = cntv_.fired;
  cntv_irq_pending_ = cntv_.irq_pending;
  update_irq_state();
}

std::uint64_t GenericTimer::read_cntv_tval_el0(std::uint64_t steps) const {
  return read_tval(cntv_, counter_at_steps(steps));
}

void GenericTimer::write_cntv_tval_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  write_tval(cntv_, counter_, value);
  trace_timer("TIMER-V-TVAL", counter_, value, cntv_.cval);
  cntv_cval_el0_ = cntv_.cval;
  cntv_fired_ = cntv_.fired;
  cntv_irq_pending_ = cntv_.irq_pending;
  update_irq_state();
}

std::uint64_t GenericTimer::read_cntp_ctl_el0(std::uint64_t steps) const {
  return read_ctl(cntp_, counter_at_steps(steps));
}

void GenericTimer::write_cntp_ctl_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  write_ctl(cntp_, value);
  trace_timer("TIMER-P-CTL", counter_, value, cntp_.cval);
  cntp_enable_ = cntp_.enable;
  cntp_imask_ = cntp_.imask;
  cntp_fired_ = cntp_.fired;
  cntp_irq_pending_ = cntp_.irq_pending;
  update_irq_state();
}

void GenericTimer::write_cntp_cval_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  write_cval(cntp_, value);
  trace_timer("TIMER-P-CVAL", counter_, value, 0);
  cntp_cval_el0_ = cntp_.cval;
  cntp_fired_ = cntp_.fired;
  cntp_irq_pending_ = cntp_.irq_pending;
  update_irq_state();
}

std::uint64_t GenericTimer::read_cntp_tval_el0(std::uint64_t steps) const {
  return read_tval(cntp_, counter_at_steps(steps));
}

void GenericTimer::write_cntp_tval_el0(std::uint64_t steps, std::uint64_t value) {
  sync_to_steps(steps);
  write_tval(cntp_, counter_, value);
  trace_timer("TIMER-P-TVAL", counter_, value, cntp_.cval);
  cntp_cval_el0_ = cntp_.cval;
  cntp_fired_ = cntp_.fired;
  cntp_irq_pending_ = cntp_.irq_pending;
  update_irq_state();
}

void GenericTimer::update_irq_state() {
  if (enabled_ && !mmio_irq_pending_ && !fired_ && counter_ >= compare_) {
    mmio_irq_pending_ = true;
    fired_ = true;
    trace_timer("TIMER-MMIO-IRQ", counter_, compare_, 0);
  }

  cntv_.cval = cntv_cval_el0_;
  cntv_.enable = cntv_enable_;
  cntv_.imask = cntv_imask_;
  cntv_.fired = cntv_fired_;
  cntv_.irq_pending = cntv_irq_pending_;
  const bool old_cntv_pending = cntv_.irq_pending;
  update_channel_irq(cntv_, counter_);
  if (!old_cntv_pending && cntv_.irq_pending) {
    trace_timer("TIMER-V-IRQ", counter_, cntv_.cval, read_ctl(cntv_, counter_));
  }
  cntv_cval_el0_ = cntv_.cval;
  cntv_enable_ = cntv_.enable;
  cntv_imask_ = cntv_.imask;
  cntv_fired_ = cntv_.fired;
  cntv_irq_pending_ = cntv_.irq_pending;

  cntp_.cval = cntp_cval_el0_;
  cntp_.enable = cntp_enable_;
  cntp_.imask = cntp_imask_;
  cntp_.fired = cntp_fired_;
  cntp_.irq_pending = cntp_irq_pending_;
  const bool old_cntp_pending = cntp_.irq_pending;
  update_channel_irq(cntp_, counter_);
  if (!old_cntp_pending && cntp_.irq_pending) {
    trace_timer("TIMER-P-IRQ", counter_, cntp_.cval, read_ctl(cntp_, counter_));
  }
  cntp_cval_el0_ = cntp_.cval;
  cntp_enable_ = cntp_.enable;
  cntp_imask_ = cntp_.imask;
  cntp_fired_ = cntp_.fired;
  cntp_irq_pending_ = cntp_.irq_pending;
}

bool GenericTimer::save_state(std::ostream& out) const {
  return snapshot_io::write(out, counter_) &&
         snapshot_io::write(out, compare_) &&
         snapshot_io::write_bool(out, enabled_) &&
         snapshot_io::write_bool(out, mmio_irq_pending_) &&
         snapshot_io::write_bool(out, fired_) &&
         snapshot_io::write(out, cntv_cval_el0_) &&
         snapshot_io::write_bool(out, cntv_enable_) &&
         snapshot_io::write_bool(out, cntv_imask_) &&
         snapshot_io::write_bool(out, cntv_fired_) &&
         snapshot_io::write_bool(out, cntv_irq_pending_) &&
         snapshot_io::write(out, cntp_cval_el0_) &&
         snapshot_io::write_bool(out, cntp_enable_) &&
         snapshot_io::write_bool(out, cntp_imask_) &&
         snapshot_io::write_bool(out, cntp_fired_) &&
         snapshot_io::write_bool(out, cntp_irq_pending_);
}

bool GenericTimer::load_state(std::istream& in, std::uint32_t version) {
  step_anchor_ = 0;
  cycles_per_step_ = 1;
  cntp_cval_el0_ = 0;
  cntp_enable_ = false;
  cntp_imask_ = false;
  cntp_fired_ = false;
  cntp_irq_pending_ = false;

  if (!snapshot_io::read(in, counter_) ||
      !snapshot_io::read(in, compare_) ||
      !snapshot_io::read_bool(in, enabled_) ||
      !snapshot_io::read_bool(in, mmio_irq_pending_) ||
      !snapshot_io::read_bool(in, fired_) ||
      !snapshot_io::read(in, cntv_cval_el0_) ||
      !snapshot_io::read_bool(in, cntv_enable_) ||
      !snapshot_io::read_bool(in, cntv_imask_) ||
      !snapshot_io::read_bool(in, cntv_fired_)) {
    return false;
  }

  if (version >= 2) {
    if (!snapshot_io::read_bool(in, cntv_irq_pending_) ||
        !snapshot_io::read(in, cntp_cval_el0_) ||
        !snapshot_io::read_bool(in, cntp_enable_) ||
        !snapshot_io::read_bool(in, cntp_imask_) ||
        !snapshot_io::read_bool(in, cntp_fired_) ||
        !snapshot_io::read_bool(in, cntp_irq_pending_)) {
      return false;
    }
  } else {
    cntv_irq_pending_ = false;
    cntp_cval_el0_ = 0;
    cntp_enable_ = false;
    cntp_imask_ = false;
    cntp_fired_ = false;
    cntp_irq_pending_ = false;
  }

  cntv_.cval = cntv_cval_el0_;
  cntv_.enable = cntv_enable_;
  cntv_.imask = cntv_imask_;
  cntv_.fired = cntv_fired_;
  cntv_.irq_pending = cntv_irq_pending_;
  cntp_.cval = cntp_cval_el0_;
  cntp_.enable = cntp_enable_;
  cntp_.imask = cntp_imask_;
  cntp_.fired = cntp_fired_;
  cntp_.irq_pending = cntp_irq_pending_;
  update_irq_state();
  return true;
}

} // namespace aarchvm
