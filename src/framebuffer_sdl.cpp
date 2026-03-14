#include "aarchvm/framebuffer_sdl.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace aarchvm {

struct FramebufferSdl::Impl {
  const std::uint8_t* pixels = nullptr;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t stride = 0;
  std::size_t frame_bytes = 0;
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_Texture* texture = nullptr;
  bool available = false;
  std::uint64_t last_present_steps = 0;
  std::chrono::steady_clock::time_point last_present_host{};
  std::chrono::steady_clock::time_point last_event_pump_host{};
  std::shared_ptr<FramebufferDirtyTracker> dirty_tracker;
  std::vector<FramebufferDirtyTracker::RowSpan> dirty_rows;
};

FramebufferSdl::FramebufferSdl(std::span<const std::uint8_t> pixels,
                               std::uint32_t width,
                               std::uint32_t height,
                               std::uint32_t stride,
                               std::shared_ptr<FramebufferDirtyTracker> dirty_tracker)
    : impl_(std::make_unique<Impl>()) {
  impl_->pixels = pixels.data();
  impl_->width = width;
  impl_->height = height;
  impl_->stride = stride;
  impl_->frame_bytes = static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
  impl_->dirty_tracker = std::move(dirty_tracker);

  if (pixels.size() < impl_->frame_bytes) {
    std::cerr << "SDL framebuffer disabled: VRAM region is smaller than the configured surface\n";
    return;
  }

  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL framebuffer disabled: " << SDL_GetError() << '\n';
    return;
  }

  impl_->window = SDL_CreateWindow("aarchvm framebuffer",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   static_cast<int>(width),
                                   static_cast<int>(height),
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (impl_->window == nullptr) {
    std::cerr << "SDL framebuffer disabled: " << SDL_GetError() << '\n';
    return;
  }

  impl_->renderer = SDL_CreateRenderer(impl_->window, -1, SDL_RENDERER_ACCELERATED);
  if (impl_->renderer == nullptr) {
    impl_->renderer = SDL_CreateRenderer(impl_->window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (impl_->renderer == nullptr) {
    std::cerr << "SDL framebuffer disabled: " << SDL_GetError() << '\n';
    return;
  }

  impl_->texture = SDL_CreateTexture(impl_->renderer,
                                     SDL_PIXELFORMAT_XRGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     static_cast<int>(width),
                                     static_cast<int>(height));
  if (impl_->texture == nullptr) {
    std::cerr << "SDL framebuffer disabled: " << SDL_GetError() << '\n';
    return;
  }

  if (SDL_UpdateTexture(impl_->texture, nullptr, impl_->pixels, static_cast<int>(impl_->stride)) != 0) {
    std::cerr << "SDL framebuffer disabled: " << SDL_GetError() << '\n';
    return;
  }
  SDL_RenderCopy(impl_->renderer, impl_->texture, nullptr, nullptr);
  SDL_RenderPresent(impl_->renderer);
  if (impl_->dirty_tracker != nullptr) {
    impl_->dirty_tracker->consume_dirty_rows(impl_->dirty_rows);
  }

  impl_->available = true;
  impl_->last_present_host = std::chrono::steady_clock::now();
  impl_->last_event_pump_host = impl_->last_present_host;
}

FramebufferSdl::~FramebufferSdl() {
  if (!impl_) {
    return;
  }
  if (impl_->texture != nullptr) {
    SDL_DestroyTexture(impl_->texture);
  }
  if (impl_->renderer != nullptr) {
    SDL_DestroyRenderer(impl_->renderer);
  }
  if (impl_->window != nullptr) {
    SDL_DestroyWindow(impl_->window);
  }
}

bool FramebufferSdl::available() const {
  return impl_ != nullptr && impl_->available;
}

void FramebufferSdl::present(std::uint64_t step_count) {
  if (!available()) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (now - impl_->last_event_pump_host >= std::chrono::milliseconds(4)) {
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        impl_->available = false;
        return;
      }
    }
    impl_->last_event_pump_host = now;
  }

  if (impl_->dirty_tracker == nullptr || !impl_->dirty_tracker->has_dirty()) {
    return;
  }

  constexpr std::uint64_t kMinStepDelta = 200000;
  constexpr auto kMinHostDelta = std::chrono::milliseconds(33);
  if (step_count - impl_->last_present_steps < kMinStepDelta && now - impl_->last_present_host < kMinHostDelta) {
    return;
  }

  impl_->dirty_tracker->consume_dirty_rows(impl_->dirty_rows);
  if (impl_->dirty_rows.empty()) {
    return;
  }

  for (const auto& span : impl_->dirty_rows) {
    const SDL_Rect dirty = {
        .x = 0,
        .y = static_cast<int>(span.first),
        .w = static_cast<int>(impl_->width),
        .h = static_cast<int>(span.last - span.first + 1u),
    };
    const std::uint8_t* dirty_base = impl_->pixels +
                                     static_cast<std::size_t>(span.first) * static_cast<std::size_t>(impl_->stride);
    if (SDL_UpdateTexture(impl_->texture, &dirty, dirty_base, static_cast<int>(impl_->stride)) != 0) {
      return;
    }
  }

  SDL_RenderCopy(impl_->renderer, impl_->texture, nullptr, nullptr);
  SDL_RenderPresent(impl_->renderer);
  impl_->last_present_steps = step_count;
  impl_->last_present_host = now;
}

} // namespace aarchvm
