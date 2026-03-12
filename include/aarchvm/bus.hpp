#pragma once

#include "aarchvm/device.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace aarchvm {

class BusFastPath;

class Bus {
public:
  struct PerfCounters {
    std::uint64_t read_ops = 0;
    std::uint64_t write_ops = 0;
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
    std::uint64_t find_calls = 0;
    std::uint64_t device_reads = 0;
    std::uint64_t device_writes = 0;
  };

  void map(std::uint64_t base, std::uint64_t size, std::shared_ptr<Device> device);
  void set_fast_path(std::shared_ptr<BusFastPath> fast_path);

  std::optional<std::uint64_t> read(std::uint64_t addr, std::size_t size) const;
  bool write(std::uint64_t addr, std::uint64_t value, std::size_t size) const;

  [[nodiscard]] const PerfCounters& perf_counters() const { return perf_counters_; }
  void reset_perf_counters() const { perf_counters_ = {}; }

private:
  struct Mapping {
    std::uint64_t base;
    std::uint64_t size;
    std::shared_ptr<Device> device;
  };

  [[nodiscard]] const Mapping* find(std::uint64_t addr, std::size_t size) const;

  std::vector<Mapping> mappings_;
  std::shared_ptr<BusFastPath> fast_path_;
  mutable PerfCounters perf_counters_{};
};

} // namespace aarchvm
