#pragma once

#include "aarchvm/device.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <optional>

namespace aarchvm {

class GicV3 final : public Device {
public:
  GicV3();
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void set_pending(std::uint32_t intid);
  [[nodiscard]] bool has_pending() const;
  [[nodiscard]] std::optional<std::uint32_t> acknowledge();
  void eoi(std::uint32_t intid);
  [[nodiscard]] bool pending(std::uint32_t intid) const { return intid < pending_.size() ? pending_[intid] : false; }
  [[nodiscard]] bool enabled(std::uint32_t intid) const { return intid < enabled_.size() ? enabled_[intid] : false; }
  [[nodiscard]] std::uint32_t gicd_ctlr() const { return gicd_ctlr_; }

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

private:
  static constexpr std::uint64_t kDistSize = 0x10000;
  static constexpr std::uint64_t kRedistBase = 0x0A0000;
  static constexpr std::uint64_t kRedistRdSize = 0x10000;
  static constexpr std::uint64_t kRedistSgiBase = kRedistBase + kRedistRdSize;
  static constexpr std::uint64_t kRedistSgiSize = 0x10000;
  static constexpr std::uint32_t kNumIntIds = 1024;

  [[nodiscard]] std::uint32_t read_dist_reg32(std::uint64_t offset) const;
  [[nodiscard]] std::uint32_t read_redist_reg32(std::uint64_t offset) const;
  void write_dist_reg32(std::uint64_t offset, std::uint32_t value);
  void write_redist_reg32(std::uint64_t offset, std::uint32_t value);
  void set_enable_range(std::uint32_t first_intid, std::uint32_t bits, bool enable);
  [[nodiscard]] std::uint32_t get_enable_range(std::uint32_t first_intid) const;

  std::array<bool, kNumIntIds> pending_{};
  std::array<bool, kNumIntIds> enabled_{};
  std::uint32_t gicd_ctlr_ = 0;
  std::uint32_t gicr_waker_ = 0;
};

} // namespace aarchvm
