#include "aarchvm/gicv3.hpp"

namespace aarchvm {

std::uint64_t GicV3::read(std::uint64_t offset, std::size_t size) {
  (void)offset;
  (void)size;
  return 0;
}

void GicV3::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  (void)offset;
  (void)value;
  (void)size;
}

void GicV3::set_pending(std::uint32_t intid) {
  if (intid < pending_.size()) {
    pending_[intid] = true;
  }
}

bool GicV3::has_pending() const {
  for (bool p : pending_) {
    if (p) {
      return true;
    }
  }
  return false;
}

std::optional<std::uint32_t> GicV3::acknowledge() {
  for (std::uint32_t i = 0; i < pending_.size(); ++i) {
    if (pending_[i]) {
      pending_[i] = false;
      return i;
    }
  }
  return std::nullopt;
}

void GicV3::eoi(std::uint32_t intid) {
  (void)intid;
}

} // namespace aarchvm
