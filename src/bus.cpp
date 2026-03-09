#include "aarchvm/bus.hpp"

namespace aarchvm {

void Bus::map(std::uint64_t base, std::uint64_t size, std::shared_ptr<Device> device) {
  mappings_.push_back(Mapping{base, size, std::move(device)});
}

std::optional<std::uint64_t> Bus::read(std::uint64_t addr, std::size_t size) const {
  const Mapping* mapping = find(addr, size);
  if (mapping == nullptr) {
    return std::nullopt;
  }
  return mapping->device->read(addr - mapping->base, size);
}

bool Bus::write(std::uint64_t addr, std::uint64_t value, std::size_t size) const {
  const Mapping* mapping = find(addr, size);
  if (mapping == nullptr) {
    return false;
  }
  mapping->device->write(addr - mapping->base, value, size);
  return true;
}

const Bus::Mapping* Bus::find(std::uint64_t addr, std::size_t size) const {
  for (const Mapping& m : mappings_) {
    if (addr >= m.base && addr + size <= m.base + m.size) {
      return &m;
    }
  }
  return nullptr;
}

} // namespace aarchvm
