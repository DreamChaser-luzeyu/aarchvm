#pragma once

#include "aarchvm/device.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <optional>

namespace aarchvm {

class GicV3 final : public Device {
public:
  struct PerfCounters {
    std::uint64_t has_pending_calls = 0;
    std::uint64_t acknowledge_calls = 0;
    std::uint64_t recompute_calls = 0;
    std::uint64_t set_level_calls = 0;
  };

  GicV3();
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void set_pending(std::uint32_t intid);
  void set_level(std::uint32_t intid, bool asserted);
  [[nodiscard]] bool has_pending() const;
  [[nodiscard]] bool has_pending(std::uint8_t pmr) const;
  [[nodiscard]] std::optional<std::uint32_t> acknowledge();
  [[nodiscard]] std::optional<std::uint32_t> acknowledge(std::uint8_t pmr);
  void eoi(std::uint32_t intid);
  [[nodiscard]] bool pending(std::uint32_t intid) const;
  [[nodiscard]] bool enabled(std::uint32_t intid) const;
  [[nodiscard]] std::uint8_t priority(std::uint32_t intid) const;
  [[nodiscard]] std::uint32_t gicd_ctlr() const { return gicd_ctlr_; }

  [[nodiscard]] const PerfCounters& perf_counters() const { return perf_counters_; }
  void reset_perf_counters() const { perf_counters_ = {}; }

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in, std::uint32_t version = 4);

private:
  static constexpr std::uint64_t kDistSize = 0x10000;
  static constexpr std::uint64_t kRedistBase = 0x0A0000;
  static constexpr std::uint64_t kRedistRdSize = 0x10000;
  static constexpr std::uint64_t kRedistSgiBase = kRedistBase + kRedistRdSize;
  static constexpr std::uint64_t kRedistSgiSize = 0x10000;
  static constexpr std::uint32_t kNumIntIds = 1024;
  static constexpr std::uint32_t kWords = kNumIntIds / 64u;

  [[nodiscard]] std::uint32_t read_dist_reg32(std::uint64_t offset) const;
  [[nodiscard]] std::uint32_t read_redist_reg32(std::uint64_t offset) const;
  void write_dist_reg32(std::uint64_t offset, std::uint32_t value);
  void write_redist_reg32(std::uint64_t offset, std::uint32_t value);
  void set_enable_range(std::uint32_t first_intid, std::uint32_t bits, bool enable);
  [[nodiscard]] std::uint32_t get_enable_range(std::uint32_t first_intid) const;
  void set_pending_range(std::uint32_t first_intid, std::uint32_t bits, bool value);
  [[nodiscard]] std::uint32_t get_pending_range(std::uint32_t first_intid) const;
  void set_active_range(std::uint32_t first_intid, std::uint32_t bits, bool value);
  [[nodiscard]] std::uint32_t get_active_range(std::uint32_t first_intid) const;
  [[nodiscard]] std::uint32_t get_priority_range(std::uint32_t first_intid) const;
  void set_priority_range(std::uint32_t first_intid, std::uint32_t value, std::uint32_t start_byte, std::uint32_t count);
  void rebuild_bitmaps();
  void recompute_best_pending();
  [[nodiscard]] bool is_candidate(std::uint32_t intid) const;
  void consider_best_pending(std::uint32_t intid);
  void refresh_word(std::uint32_t word_index);
  void set_pending_bit(std::uint32_t intid, bool value);
  void set_enabled_bit(std::uint32_t intid, bool value);
  void set_active_bit(std::uint32_t intid, bool value);
  [[nodiscard]] static std::uint32_t word_index(std::uint32_t intid) { return intid >> 6; }
  [[nodiscard]] static std::uint64_t bit_mask(std::uint32_t intid) { return 1ull << (intid & 63u); }

  std::array<bool, kNumIntIds> pending_{};
  std::array<bool, kNumIntIds> enabled_{};
  std::array<bool, kNumIntIds> active_{};
  std::array<bool, kNumIntIds> line_level_{};
  std::array<std::uint8_t, kNumIntIds> priorities_{};
  std::array<std::uint64_t, kWords> pending_bits_{};
  std::array<std::uint64_t, kWords> enabled_bits_{};
  std::array<std::uint64_t, kWords> active_bits_{};
  std::array<std::uint64_t, kWords> pending_enabled_bits_{};
  bool best_pending_valid_ = false;
  std::uint32_t best_pending_intid_ = 1023;
  std::uint8_t best_pending_priority_ = 0xFFu;
  std::uint32_t gicd_ctlr_ = 0;
  std::uint32_t gicr_waker_ = 0;
  std::uint32_t gicr_igroupr0_ = 0xFFFFFFFFu;
  std::uint32_t gicr_icfgr0_ = 0xAAAAAAAAu;
  std::uint32_t gicr_icfgr1_ = 0;
  mutable PerfCounters perf_counters_{};
};

} // namespace aarchvm
