#pragma once

#include <cstdint>

namespace aarchvm {

struct PerfCounters {
  std::uint64_t steps = 0;
  std::uint64_t bus_reads = 0;
  std::uint64_t bus_writes = 0;
  std::uint64_t bus_read_bytes = 0;
  std::uint64_t bus_write_bytes = 0;
  std::uint64_t bus_find_calls = 0;
  std::uint64_t bus_device_reads = 0;
  std::uint64_t bus_device_writes = 0;
  std::uint64_t ram_fast_reads = 0;
  std::uint64_t ram_fast_writes = 0;
  std::uint64_t ram_fast_read_bytes = 0;
  std::uint64_t ram_fast_write_bytes = 0;
  std::uint64_t translate_calls = 0;
  std::uint64_t tlb_lookups = 0;
  std::uint64_t tlb_hits = 0;
  std::uint64_t tlb_misses = 0;
  std::uint64_t tlb_inserts = 0;
  std::uint64_t tlb_flush_all = 0;
  std::uint64_t tlb_flush_va = 0;
  std::uint64_t page_walks = 0;
  std::uint64_t page_walk_desc_reads = 0;
  std::uint64_t gic_has_pending = 0;
  std::uint64_t gic_acknowledge = 0;
  std::uint64_t gic_recompute = 0;
  std::uint64_t gic_set_level = 0;
  std::uint64_t soc_sync_devices = 0;
  std::uint64_t soc_run_chunks = 0;
};

struct PerfResult {
  std::uint64_t case_id = 0;
  std::uint64_t arg0 = 0;
  std::uint64_t arg1 = 0;
  std::uint64_t host_ns = 0;
  PerfCounters delta{};
};

} // namespace aarchvm
