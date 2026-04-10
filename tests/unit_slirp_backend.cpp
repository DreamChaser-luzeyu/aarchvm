#include "aarchvm/slirp_net_backend.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace {

using aarchvm::SlirpNetBackend;

constexpr std::array<std::uint8_t, 6> kGuestMac = {0x02u, 0x12u, 0x34u, 0x56u, 0x78u, 0x9au};
constexpr std::array<std::uint8_t, 6> kHostMac = {0x52u, 0x55u, 0x0au, 0x00u, 0x02u, 0x02u};
constexpr std::array<std::uint8_t, 4> kGuestIp = {10u, 0u, 2u, 15u};
constexpr std::array<std::uint8_t, 4> kHostIp = {10u, 0u, 2u, 2u};

std::vector<std::uint8_t> make_arp_request() {
  std::vector<std::uint8_t> frame(60u, 0u);

  for (std::size_t i = 0; i < 6; ++i) {
    frame[i] = 0xffu;
    frame[6u + i] = kGuestMac[i];
  }
  frame[12] = 0x08u;
  frame[13] = 0x06u;
  frame[14] = 0x00u;
  frame[15] = 0x01u;
  frame[16] = 0x08u;
  frame[17] = 0x00u;
  frame[18] = 0x06u;
  frame[19] = 0x04u;
  frame[20] = 0x00u;
  frame[21] = 0x01u;
  for (std::size_t i = 0; i < 6; ++i) {
    frame[22u + i] = kGuestMac[i];
  }
  for (std::size_t i = 0; i < 4; ++i) {
    frame[28u + i] = kGuestIp[i];
    frame[38u + i] = kHostIp[i];
  }
  return frame;
}

bool validate_arp_reply(const std::vector<std::uint8_t>& frame) {
  if (frame.size() < 42u) {
    return false;
  }
  if (frame[12] != 0x08u || frame[13] != 0x06u) {
    return false;
  }
  if (frame[20] != 0x00u || frame[21] != 0x02u) {
    return false;
  }

  for (std::size_t i = 0; i < 6; ++i) {
    if (frame[i] != kGuestMac[i] || frame[6u + i] != kHostMac[i] || frame[22u + i] != kHostMac[i] ||
        frame[32u + i] != kGuestMac[i]) {
      return false;
    }
  }
  for (std::size_t i = 0; i < 4; ++i) {
    if (frame[28u + i] != kHostIp[i] || frame[38u + i] != kGuestIp[i]) {
      return false;
    }
  }
  return true;
}

bool probe_arp(SlirpNetBackend& backend, std::vector<std::uint8_t>& reply) {
  reply.clear();
  backend.send_guest_frame(make_arp_request());

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    (void)backend.poll();
    if (!reply.empty()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return !reply.empty();
}

} // namespace

int main() {
  std::vector<std::uint8_t> reply;
  SlirpNetBackend backend([&reply](std::vector<std::uint8_t> frame) { reply = std::move(frame); });
  if (!backend.ready()) {
    std::cerr << "slirp backend failed to initialize\n";
    return 1;
  }
  if (!probe_arp(backend, reply) || !validate_arp_reply(reply)) {
    std::cerr << "failed to observe a valid ARP reply from libslirp\n";
    return 1;
  }

  std::stringstream snapshot(std::ios::in | std::ios::out | std::ios::binary);
  if (!backend.save_state(snapshot)) {
    std::cerr << "failed to save slirp state\n";
    return 1;
  }

  std::vector<std::uint8_t> restored_reply;
  SlirpNetBackend restored([&restored_reply](std::vector<std::uint8_t> frame) { restored_reply = std::move(frame); });
  if (!restored.ready()) {
    std::cerr << "restored slirp backend failed to initialize\n";
    return 1;
  }

  snapshot.seekg(0);
  if (!restored.load_state(snapshot)) {
    std::cerr << "failed to load slirp state\n";
    return 1;
  }
  if (!probe_arp(restored, restored_reply) || !validate_arp_reply(restored_reply)) {
    std::cerr << "restored slirp backend did not respond to ARP\n";
    return 1;
  }

  return 0;
}
