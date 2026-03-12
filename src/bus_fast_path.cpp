#include "aarchvm/bus_fast_path.hpp"

#include "aarchvm/ram.hpp"

namespace aarchvm {

namespace {

constexpr std::uint64_t kBootRamBase = 0x00000000ull;
constexpr std::uint64_t kBootRamSize = 128ull * 1024ull * 1024ull;
constexpr std::uint64_t kSdramBase = 0x40000000ull;
constexpr std::uint64_t kSdramSize = 128ull * 1024ull * 1024ull;

} // namespace

BusFastPath::BusFastPath(Ram& boot_ram, Ram& sdram)
    : boot_ram_(&boot_ram), sdram_(&sdram) {}

bool BusFastPath::read(std::uint64_t addr, std::size_t size, std::uint64_t& value) const {
  if (addr >= kBootRamBase && addr + size <= kBootRamBase + kBootRamSize) {
    if (boot_ram_->read_fast(addr - kBootRamBase, size, value)) {
      ++perf_counters_.read_ops;
      perf_counters_.read_bytes += size;
      return true;
    }
    return false;
  }
  if (addr >= kSdramBase && addr + size <= kSdramBase + kSdramSize) {
    if (sdram_->read_fast(addr - kSdramBase, size, value)) {
      ++perf_counters_.read_ops;
      perf_counters_.read_bytes += size;
      return true;
    }
    return false;
  }
  return false;
}

bool BusFastPath::write(std::uint64_t addr, std::uint64_t value, std::size_t size) const {
  if (addr >= kBootRamBase && addr + size <= kBootRamBase + kBootRamSize) {
    if (boot_ram_->write_fast(addr - kBootRamBase, value, size)) {
      ++perf_counters_.write_ops;
      perf_counters_.write_bytes += size;
      return true;
    }
    return false;
  }
  if (addr >= kSdramBase && addr + size <= kSdramBase + kSdramSize) {
    if (sdram_->write_fast(addr - kSdramBase, value, size)) {
      ++perf_counters_.write_ops;
      perf_counters_.write_bytes += size;
      return true;
    }
    return false;
  }
  return false;
}

} // namespace aarchvm
