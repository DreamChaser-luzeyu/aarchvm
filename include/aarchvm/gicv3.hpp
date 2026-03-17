#pragma once

#include "aarchvm/device.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

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
  void set_cpu_count(std::size_t cpu_count);
  void set_cpu_affinity(std::size_t cpu_index, std::uint64_t mpidr);

  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void set_pending(std::uint32_t intid);
  void set_pending(std::size_t cpu_index, std::uint32_t intid);
  void set_level(std::uint32_t intid, bool asserted);
  void set_level(std::size_t cpu_index, std::uint32_t intid, bool asserted);
  void send_sgi(std::size_t source_cpu, std::uint64_t value);
  [[nodiscard]] bool has_pending(std::size_t cpu_index) const;
  [[nodiscard]] bool has_pending(std::size_t cpu_index, std::uint8_t pmr) const;
  [[nodiscard]] bool acknowledge(std::size_t cpu_index, std::uint32_t& intid);
  [[nodiscard]] bool acknowledge(std::size_t cpu_index, std::uint8_t pmr, std::uint32_t& intid);
  void eoi(std::size_t cpu_index, std::uint32_t intid);
  [[nodiscard]] bool pending(std::uint32_t intid) const;
  [[nodiscard]] bool enabled(std::uint32_t intid) const;
  [[nodiscard]] std::uint8_t priority(std::size_t cpu_index, std::uint32_t intid) const;
  [[nodiscard]] std::uint32_t gicd_ctlr() const { return gicd_ctlr_; }

  [[nodiscard]] const PerfCounters& perf_counters() const { return perf_counters_; }
  void reset_perf_counters() const { perf_counters_ = {}; }
  [[nodiscard]] std::uint64_t state_epoch() const { return state_epoch_; }

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in, std::uint32_t version = 5);

private:
  static constexpr std::uint64_t kDistSize = 0x10000;
  static constexpr std::uint64_t kRedistBase = 0x0A0000;
  static constexpr std::uint64_t kRedistRdSize = 0x10000;
  static constexpr std::uint64_t kRedistSgiSize = 0x10000;
  static constexpr std::uint64_t kRedistFrameSize = kRedistRdSize + kRedistSgiSize;
  static constexpr std::uint32_t kNumIntIds = 1024;
  static constexpr std::uint32_t kLocalIntIds = 32;
  static constexpr std::uint32_t kFirstSpiIntId = 32;
  static constexpr std::uint32_t kNumSpiIntIds = kNumIntIds - kFirstSpiIntId;
  static constexpr std::uint8_t kDefaultPriority = 0xC0u;

  struct LocalCpuState {
    std::array<bool, kLocalIntIds> pending{};
    std::array<bool, kLocalIntIds> enabled{};
    std::array<bool, kLocalIntIds> active{};
    std::array<bool, kLocalIntIds> line_level{};
    std::array<std::uint8_t, kLocalIntIds> priorities{};
    std::uint32_t gicr_waker = 0;
    std::uint32_t gicr_igroupr0 = 0xFFFFFFFFu;
    std::uint32_t gicr_icfgr0 = 0xAAAAAAAAu;
    std::uint32_t gicr_icfgr1 = 0;
    std::uint64_t mpidr = 0;
  };

  struct PendingQueryCache {
    bool valid = false;
    bool value = false;
    std::uint8_t pmr = 0xFFu;
    std::uint64_t epoch = 0;
  };

  [[nodiscard]] std::uint32_t read_dist_reg32(std::uint64_t offset) const;
  [[nodiscard]] std::uint32_t read_redist_reg32(std::uint64_t offset) const;
  void write_dist_reg32(std::uint64_t offset, std::uint32_t value);
  void write_redist_reg32(std::uint64_t offset, std::uint32_t value);

  [[nodiscard]] LocalCpuState& local_cpu(std::size_t cpu_index);
  [[nodiscard]] const LocalCpuState& local_cpu(std::size_t cpu_index) const;
  [[nodiscard]] static bool is_local_intid(std::uint32_t intid) { return intid < kLocalIntIds; }
  [[nodiscard]] static std::size_t spi_index(std::uint32_t intid) { return static_cast<std::size_t>(intid - kFirstSpiIntId); }
  void set_spi_enable_range(std::uint32_t first_intid, std::uint32_t bits, bool enable);
  [[nodiscard]] std::uint32_t get_spi_enable_range(std::uint32_t first_intid) const;
  void set_spi_active_range(std::uint32_t first_intid, std::uint32_t bits, bool value);
  [[nodiscard]] std::uint32_t get_spi_active_range(std::uint32_t first_intid) const;
  [[nodiscard]] std::uint32_t get_priority_range(std::uint32_t first_intid) const;
  void set_priority_range(std::uint32_t first_intid,
                          std::uint32_t value,
                          std::uint32_t start_byte,
                          std::uint32_t count,
                          std::size_t cpu_index = 0,
                          bool local = false);
  [[nodiscard]] bool local_candidate(std::size_t cpu_index, std::uint32_t intid, std::uint8_t pmr) const;
  [[nodiscard]] bool spi_candidate(std::uint32_t intid, std::uint8_t pmr) const;
  void set_local_pending_bit(std::size_t cpu_index, std::uint32_t intid, bool value);
  void set_local_enabled_bit(std::size_t cpu_index, std::uint32_t intid, bool value);
  void set_local_active_bit(std::size_t cpu_index, std::uint32_t intid, bool value);
  void set_spi_pending_bit(std::uint32_t intid, bool value);
  void set_spi_enabled_bit(std::uint32_t intid, bool value);
  void set_spi_active_bit(std::uint32_t intid, bool value);
  [[nodiscard]] bool match_affinity(std::size_t cpu_index,
                                    std::uint8_t aff3,
                                    std::uint8_t aff2,
                                    std::uint8_t aff1) const;

  std::size_t cpu_count_ = 1;
  std::vector<LocalCpuState> locals_{1};
  std::array<bool, kNumSpiIntIds> spi_pending_{};
  std::array<bool, kNumSpiIntIds> spi_enabled_{};
  std::array<bool, kNumSpiIntIds> spi_active_{};
  std::array<bool, kNumSpiIntIds> spi_line_level_{};
  std::array<std::uint8_t, kNumSpiIntIds> spi_priorities_{};
  mutable std::vector<PendingQueryCache> pending_query_cache_{1};
  std::uint32_t gicd_ctlr_ = 0;
  std::uint64_t state_epoch_ = 1;
  mutable PerfCounters perf_counters_{};
};

} // namespace aarchvm
