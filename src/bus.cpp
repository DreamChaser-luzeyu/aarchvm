#include "aarchvm/bus.hpp"

#include "aarchvm/bus_fast_path.hpp"

namespace aarchvm {

void Bus::map(std::uint64_t base, std::uint64_t size, std::shared_ptr<Device> device) {
  mappings_.push_back(Mapping{base, size, std::move(device)});
}

void Bus::set_fast_path(std::shared_ptr<BusFastPath> fast_path) {
  fast_path_ = std::move(fast_path);
  fast_path_raw_ = fast_path_.get();
}

bool Bus::read(std::uint64_t addr, std::size_t size, std::uint64_t& value) const {
  ++perf_counters_.read_ops;
  perf_counters_.read_bytes += size;
  if (fast_path_raw_ != nullptr && fast_path_raw_->read(addr, size, value)) {
    return true;
  }

  const Mapping* mapping = find(addr, size);
  if (mapping == nullptr) {
    value = 0;
    return false;
  }
  ++perf_counters_.device_reads;
  value = mapping->device->read(addr - mapping->base, size);
  return true;
}

std::optional<std::uint64_t> Bus::read(std::uint64_t addr, std::size_t size) const {
  std::uint64_t value = 0;
  if (!read(addr, size, value)) {
    return std::nullopt;
  }
  return value;
}

bool Bus::write(std::uint64_t addr, std::uint64_t value, std::size_t size) const {
  ++perf_counters_.write_ops;
  perf_counters_.write_bytes += size;
  if (fast_path_raw_ != nullptr && fast_path_raw_->write(addr, value, size)) {
    return true;
  }

  const Mapping* mapping = find(addr, size);
  if (mapping == nullptr) {
    return false;
  }
  ++perf_counters_.device_writes;
  mapping->device->write(addr - mapping->base, value, size);
  return true;
}

bool Bus::read_ram_fast(std::uint64_t addr, std::size_t size, std::uint64_t& value) const {
  if (fast_path_raw_ == nullptr || !fast_path_raw_->read_ram_only(addr, size, value)) {
    return false;
  }
  ++perf_counters_.read_ops;
  perf_counters_.read_bytes += size;
  return true;
}

bool Bus::write_ram_fast(std::uint64_t addr, std::uint64_t value, std::size_t size) const {
  if (fast_path_raw_ == nullptr || !fast_path_raw_->write_ram_only(addr, value, size)) {
    return false;
  }
  ++perf_counters_.write_ops;
  perf_counters_.write_bytes += size;
  return true;
}

const std::uint8_t* Bus::ram_ptr(std::uint64_t addr, std::size_t size) const {
  return (fast_path_raw_ == nullptr) ? nullptr : fast_path_raw_->ram_ptr(addr, size);
}

std::uint8_t* Bus::ram_mut_ptr(std::uint64_t addr, std::size_t size) const {
  return (fast_path_raw_ == nullptr) ? nullptr : fast_path_raw_->ram_mut_ptr(addr, size);
}

const Bus::Mapping* Bus::find(std::uint64_t addr, std::size_t size) const {
  ++perf_counters_.find_calls;
  for (const Mapping& m : mappings_) {
    if (addr >= m.base && addr + size <= m.base + m.size) {
      return &m;
    }
  }
  return nullptr;
}

} // namespace aarchvm
