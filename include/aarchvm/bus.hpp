#pragma once

#include "aarchvm/device.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
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
  [[nodiscard]] bool is_range_free(std::uint64_t base, std::uint64_t size) const;
  void set_fast_path(std::shared_ptr<BusFastPath> fast_path);
  void set_ram_write_observer(std::function<void(std::uint64_t, std::size_t)> observer) {
    ram_write_observer_ = std::move(observer);
  }

  bool read(std::uint64_t addr, std::size_t size, std::uint64_t& value) const;
  std::optional<std::uint64_t> read(std::uint64_t addr, std::size_t size) const;
  bool write(std::uint64_t addr, std::uint64_t value, std::size_t size) const;
  bool read_ram_fast(std::uint64_t addr, std::size_t size, std::uint64_t& value) const;
  bool write_ram_fast(std::uint64_t addr, std::uint64_t value, std::size_t size) const;
  bool write_ram_buffer(std::uint64_t addr, const void* src, std::size_t size) const;
  [[nodiscard]] const std::uint8_t* ram_ptr(std::uint64_t addr, std::size_t size) const;
  [[nodiscard]] std::uint8_t* ram_mut_ptr(std::uint64_t addr, std::size_t size) const;

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
  BusFastPath* fast_path_raw_ = nullptr;
  std::function<void(std::uint64_t, std::size_t)> ram_write_observer_;
  mutable PerfCounters perf_counters_{};
};

} // namespace aarchvm
