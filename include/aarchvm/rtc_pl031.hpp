#pragma once

#include "aarchvm/device.hpp"

#include <cstdint>
#include <iosfwd>

namespace aarchvm {

class RtcPl031 final : public Device {
public:
  RtcPl031();

  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void reset();
  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

private:
  [[nodiscard]] static std::int64_t host_now_seconds();
  [[nodiscard]] bool clock_enabled() const { return (cr_ & 0x1u) != 0u; }
  [[nodiscard]] std::uint32_t current_seconds() const;
  void refresh_alarm_state();
  void update_alarm_state(std::uint32_t current_seconds);

  std::int64_t offset_seconds_ = 0;
  std::uint32_t match_seconds_ = 0;
  std::uint32_t lr_shadow_ = 0;
  std::uint32_t cr_ = 0x1u;
  std::uint32_t imsc_ = 0;
  std::uint32_t frozen_seconds_ = 0;
  bool raw_pending_ = false;
  bool alarm_armed_ = false;
};

} // namespace aarchvm
