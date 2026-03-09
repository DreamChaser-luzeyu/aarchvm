#pragma once

#include "aarchvm/device.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace aarchvm {

class Bus {
public:
  void map(std::uint64_t base, std::uint64_t size, std::shared_ptr<Device> device);

  std::optional<std::uint64_t> read(std::uint64_t addr, std::size_t size) const;
  bool write(std::uint64_t addr, std::uint64_t value, std::size_t size) const;

private:
  struct Mapping {
    std::uint64_t base;
    std::uint64_t size;
    std::shared_ptr<Device> device;
  };

  [[nodiscard]] const Mapping* find(std::uint64_t addr, std::size_t size) const;

  std::vector<Mapping> mappings_;
};

} // namespace aarchvm
