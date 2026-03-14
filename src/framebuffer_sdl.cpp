#include "aarchvm/framebuffer_sdl.hpp"

#include <SDL.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace aarchvm {

namespace {

struct Ps2Key {
  bool extended = false;
  std::uint8_t code = 0;
};

bool lookup_ps2_set2(SDL_Scancode scancode, Ps2Key& out) {
  switch (scancode) {
    case SDL_SCANCODE_A: out = {false, 0x1Cu}; return true;
    case SDL_SCANCODE_B: out = {false, 0x32u}; return true;
    case SDL_SCANCODE_C: out = {false, 0x21u}; return true;
    case SDL_SCANCODE_D: out = {false, 0x23u}; return true;
    case SDL_SCANCODE_E: out = {false, 0x24u}; return true;
    case SDL_SCANCODE_F: out = {false, 0x2Bu}; return true;
    case SDL_SCANCODE_G: out = {false, 0x34u}; return true;
    case SDL_SCANCODE_H: out = {false, 0x33u}; return true;
    case SDL_SCANCODE_I: out = {false, 0x43u}; return true;
    case SDL_SCANCODE_J: out = {false, 0x3Bu}; return true;
    case SDL_SCANCODE_K: out = {false, 0x42u}; return true;
    case SDL_SCANCODE_L: out = {false, 0x4Bu}; return true;
    case SDL_SCANCODE_M: out = {false, 0x3Au}; return true;
    case SDL_SCANCODE_N: out = {false, 0x31u}; return true;
    case SDL_SCANCODE_O: out = {false, 0x44u}; return true;
    case SDL_SCANCODE_P: out = {false, 0x4Du}; return true;
    case SDL_SCANCODE_Q: out = {false, 0x15u}; return true;
    case SDL_SCANCODE_R: out = {false, 0x2Du}; return true;
    case SDL_SCANCODE_S: out = {false, 0x1Bu}; return true;
    case SDL_SCANCODE_T: out = {false, 0x2Cu}; return true;
    case SDL_SCANCODE_U: out = {false, 0x3Cu}; return true;
    case SDL_SCANCODE_V: out = {false, 0x2Au}; return true;
    case SDL_SCANCODE_W: out = {false, 0x1Du}; return true;
    case SDL_SCANCODE_X: out = {false, 0x22u}; return true;
    case SDL_SCANCODE_Y: out = {false, 0x35u}; return true;
    case SDL_SCANCODE_Z: out = {false, 0x1Au}; return true;
    case SDL_SCANCODE_1: out = {false, 0x16u}; return true;
    case SDL_SCANCODE_2: out = {false, 0x1Eu}; return true;
    case SDL_SCANCODE_3: out = {false, 0x26u}; return true;
    case SDL_SCANCODE_4: out = {false, 0x25u}; return true;
    case SDL_SCANCODE_5: out = {false, 0x2Eu}; return true;
    case SDL_SCANCODE_6: out = {false, 0x36u}; return true;
    case SDL_SCANCODE_7: out = {false, 0x3Du}; return true;
    case SDL_SCANCODE_8: out = {false, 0x3Eu}; return true;
    case SDL_SCANCODE_9: out = {false, 0x46u}; return true;
    case SDL_SCANCODE_0: out = {false, 0x45u}; return true;
    case SDL_SCANCODE_RETURN: out = {false, 0x5Au}; return true;
    case SDL_SCANCODE_ESCAPE: out = {false, 0x76u}; return true;
    case SDL_SCANCODE_BACKSPACE: out = {false, 0x66u}; return true;
    case SDL_SCANCODE_TAB: out = {false, 0x0Du}; return true;
    case SDL_SCANCODE_SPACE: out = {false, 0x29u}; return true;
    case SDL_SCANCODE_MINUS: out = {false, 0x4Eu}; return true;
    case SDL_SCANCODE_EQUALS: out = {false, 0x55u}; return true;
    case SDL_SCANCODE_LEFTBRACKET: out = {false, 0x54u}; return true;
    case SDL_SCANCODE_RIGHTBRACKET: out = {false, 0x5Bu}; return true;
    case SDL_SCANCODE_BACKSLASH: out = {false, 0x5Du}; return true;
    case SDL_SCANCODE_SEMICOLON: out = {false, 0x4Cu}; return true;
    case SDL_SCANCODE_APOSTROPHE: out = {false, 0x52u}; return true;
    case SDL_SCANCODE_GRAVE: out = {false, 0x0Eu}; return true;
    case SDL_SCANCODE_COMMA: out = {false, 0x41u}; return true;
    case SDL_SCANCODE_PERIOD: out = {false, 0x49u}; return true;
    case SDL_SCANCODE_SLASH: out = {false, 0x4Au}; return true;
    case SDL_SCANCODE_CAPSLOCK: out = {false, 0x58u}; return true;
    case SDL_SCANCODE_F1: out = {false, 0x05u}; return true;
    case SDL_SCANCODE_F2: out = {false, 0x06u}; return true;
    case SDL_SCANCODE_F3: out = {false, 0x04u}; return true;
    case SDL_SCANCODE_F4: out = {false, 0x0Cu}; return true;
    case SDL_SCANCODE_F5: out = {false, 0x03u}; return true;
    case SDL_SCANCODE_F6: out = {false, 0x0Bu}; return true;
    case SDL_SCANCODE_F7: out = {false, 0x83u}; return true;
    case SDL_SCANCODE_F8: out = {false, 0x0Au}; return true;
    case SDL_SCANCODE_F9: out = {false, 0x01u}; return true;
    case SDL_SCANCODE_F10: out = {false, 0x09u}; return true;
    case SDL_SCANCODE_F11: out = {false, 0x78u}; return true;
    case SDL_SCANCODE_F12: out = {false, 0x07u}; return true;
    case SDL_SCANCODE_LCTRL: out = {false, 0x14u}; return true;
    case SDL_SCANCODE_RCTRL: out = {true, 0x14u}; return true;
    case SDL_SCANCODE_LSHIFT: out = {false, 0x12u}; return true;
    case SDL_SCANCODE_RSHIFT: out = {false, 0x59u}; return true;
    case SDL_SCANCODE_LALT: out = {false, 0x11u}; return true;
    case SDL_SCANCODE_RALT: out = {true, 0x11u}; return true;
    case SDL_SCANCODE_INSERT: out = {true, 0x70u}; return true;
    case SDL_SCANCODE_DELETE: out = {true, 0x71u}; return true;
    case SDL_SCANCODE_HOME: out = {true, 0x6Cu}; return true;
    case SDL_SCANCODE_END: out = {true, 0x69u}; return true;
    case SDL_SCANCODE_PAGEUP: out = {true, 0x7Du}; return true;
    case SDL_SCANCODE_PAGEDOWN: out = {true, 0x7Au}; return true;
    case SDL_SCANCODE_UP: out = {true, 0x75u}; return true;
    case SDL_SCANCODE_DOWN: out = {true, 0x72u}; return true;
    case SDL_SCANCODE_LEFT: out = {true, 0x6Bu}; return true;
    case SDL_SCANCODE_RIGHT: out = {true, 0x74u}; return true;
    default:
      return false;
  }
}

void emit_ps2_key(const FramebufferSdl::KeyboardSink& sink, const SDL_KeyboardEvent& key) {
  if (!sink || key.repeat != 0) {
    return;
  }

  Ps2Key ps2{};
  if (!lookup_ps2_set2(key.keysym.scancode, ps2)) {
    return;
  }

  if (ps2.extended) {
    sink(0xE0u);
  }
  if (key.type == SDL_KEYUP) {
    sink(0xF0u);
  }
  sink(ps2.code);
}

} // namespace

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
  KeyboardSink keyboard_sink;
};

FramebufferSdl::FramebufferSdl(std::span<const std::uint8_t> pixels,
                               std::uint32_t width,
                               std::uint32_t height,
                               std::uint32_t stride,
                               std::shared_ptr<FramebufferDirtyTracker> dirty_tracker,
                               KeyboardSink keyboard_sink)
    : impl_(std::make_unique<Impl>()) {
  impl_->pixels = pixels.data();
  impl_->width = width;
  impl_->height = height;
  impl_->stride = stride;
  impl_->frame_bytes = static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
  impl_->dirty_tracker = std::move(dirty_tracker);
  impl_->keyboard_sink = std::move(keyboard_sink);

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
      if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        emit_ps2_key(impl_->keyboard_sink, event.key);
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
