#pragma once

#include <cstddef>
#include <cstdint>

namespace aarchvm {

class Device {
public:
  virtual ~Device() = default;

  virtual std::uint64_t read(std::uint64_t offset, std::size_t size) = 0;
  virtual void write(std::uint64_t offset, std::uint64_t value, std::size_t size) = 0;
};

} // namespace aarchvm
