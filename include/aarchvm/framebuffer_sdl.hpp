#pragma once

#include "aarchvm/framebuffer_dirty_tracker.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace aarchvm {

class FramebufferSdl {
public:
  using KeyboardSink = std::function<void(std::uint8_t)>;

  FramebufferSdl(std::span<const std::uint8_t> pixels,
                 std::uint32_t width,
                 std::uint32_t height,
                 std::uint32_t stride,
                 std::shared_ptr<FramebufferDirtyTracker> dirty_tracker,
                 KeyboardSink keyboard_sink);
  ~FramebufferSdl();

  FramebufferSdl(const FramebufferSdl&) = delete;
  FramebufferSdl& operator=(const FramebufferSdl&) = delete;

  [[nodiscard]] bool available() const;
  void present(std::uint64_t step_count);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace aarchvm
