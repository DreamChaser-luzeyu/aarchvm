#pragma once

#include <cstddef>
#include <cstdint>

namespace aarchvm {

class Ram;

class BusFastPath {
public:
  struct PerfCounters {
    std::uint64_t read_ops = 0;
    std::uint64_t write_ops = 0;
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
  };

  BusFastPath(Ram& boot_ram, Ram& sdram);

  [[nodiscard]] bool read(std::uint64_t addr, std::size_t size, std::uint64_t& value) const;
  [[nodiscard]] bool write(std::uint64_t addr, std::uint64_t value, std::size_t size) const;

  [[nodiscard]] const PerfCounters& perf_counters() const { return perf_counters_; }
  void reset_perf_counters() const { perf_counters_ = {}; }

private:
  Ram* boot_ram_ = nullptr;
  Ram* sdram_ = nullptr;
  mutable PerfCounters perf_counters_{};
};

} // namespace aarchvm
