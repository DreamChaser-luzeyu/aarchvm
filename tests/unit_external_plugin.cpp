#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "aarchvm/external_device_proxy.hpp"
#include "aarchvm/plugin_config.hpp"

namespace {

using aarchvm::ExternalDeviceProxy;
using aarchvm::ExternalPluginConfig;

constexpr std::uint64_t kPluginBase = 0x09050000ull;
constexpr std::uint64_t kPluginSize = 0x1000ull;
constexpr std::uint32_t kExpectedId = 0x504C5547u;       // "PLUG"
constexpr std::uint32_t kExpectedVersion = 0x00010000u;  // 1.0
constexpr std::uint64_t kScratchValue = 0x1122334455667788ull;

std::filesystem::path sdk_build_dir() {
  return std::filesystem::path(AARCHVM_SDK_BUILD_DIR);
}

std::filesystem::path sdk_artifact(const char* relative_path) {
  return sdk_build_dir() / relative_path;
}

bool ensure_artifact_exists(const std::filesystem::path& path) {
  if (std::filesystem::exists(path)) {
    return true;
  }
  std::cerr << "Missing SDK build artifact: " << path << '\n';
  return false;
}

ExternalPluginConfig make_config(const std::filesystem::path& path) {
  ExternalPluginConfig config{};
  config.shared_object_path = path.string();
  config.instance_name = "unit_plugin";
  config.mmio_base = kPluginBase;
  config.mmio_size = kPluginSize;
  return config;
}

bool test_parse_external_plugin_spec() {
  ExternalPluginConfig config{};
  std::string error;
  if (!aarchvm::parse_external_plugin_spec(
          "/tmp/device.so,mmio=0x9050000,size=0x1000,name=demo,arg=foo=bar,baz=qux",
          config,
          error)) {
    std::cerr << error << '\n';
    return false;
  }
  return config.shared_object_path == "/tmp/device.so" &&
         config.instance_name == "demo" &&
         config.mmio_base == kPluginBase &&
         config.mmio_size == kPluginSize &&
         config.opaque_arg == "foo=bar,baz=qux";
}

bool test_parse_external_plugin_irq_rejected() {
  ExternalPluginConfig config{};
  std::string error;
  return !aarchvm::parse_external_plugin_spec("/tmp/device.so,mmio=0x1,size=0x1000,irq=35",
                                              config,
                                              error) &&
         error.find("IRQ routing") != std::string::npos;
}

bool test_register_bank_proxy_mmio_roundtrip() {
  const std::filesystem::path plugin_path =
      sdk_artifact("examples/register_bank/aarchvm_register_bank.so");
  if (!ensure_artifact_exists(plugin_path)) {
    return false;
  }

  std::vector<std::string> faults;
  std::string error;
  auto proxy = ExternalDeviceProxy::spawn(
      make_config(plugin_path),
      [&faults](const std::string& message) { faults.push_back(message); },
      error);
  if (!proxy) {
    std::cerr << error << '\n';
    return false;
  }

  std::uint64_t value = proxy->read(0x0, 4u);
  if (value != kExpectedId) {
    return false;
  }
  value = proxy->read(0x4, 4u);
  if (value != kExpectedVersion) {
    return false;
  }

  proxy->write(0x8, kScratchValue, 8u);
  value = proxy->read(0x8, 8u);
  if (value != kScratchValue) {
    return false;
  }

  return faults.empty();
}

bool test_missing_entry_rejected() {
  const std::filesystem::path plugin_path = sdk_artifact("tests/abi_smoke/aarchvm_no_entry.so");
  if (!ensure_artifact_exists(plugin_path)) {
    return false;
  }

  std::string error;
  auto proxy = ExternalDeviceProxy::spawn(make_config(plugin_path), nullptr, error);
  return proxy == nullptr && error.find("dlsym(aarchvm_plugin_get_api_v1)") != std::string::npos;
}

bool test_bad_abi_rejected() {
  const std::filesystem::path plugin_path = sdk_artifact("tests/abi_smoke/aarchvm_bad_abi.so");
  if (!ensure_artifact_exists(plugin_path)) {
    return false;
  }

  std::string error;
  auto proxy = ExternalDeviceProxy::spawn(make_config(plugin_path), nullptr, error);
  return proxy == nullptr && error.find("ABI major mismatch") != std::string::npos;
}

bool test_bad_manifest_rejected() {
  const std::filesystem::path plugin_path = sdk_artifact("tests/abi_smoke/aarchvm_bad_manifest.so");
  if (!ensure_artifact_exists(plugin_path)) {
    return false;
  }

  std::string error;
  auto proxy = ExternalDeviceProxy::spawn(make_config(plugin_path), nullptr, error);
  return proxy == nullptr && error.find("exactly one declared MMIO region") != std::string::npos;
}

bool test_mmio_size_mismatch_rejected() {
  const std::filesystem::path plugin_path =
      sdk_artifact("examples/register_bank/aarchvm_register_bank.so");
  if (!ensure_artifact_exists(plugin_path)) {
    return false;
  }

  ExternalPluginConfig config = make_config(plugin_path);
  config.mmio_size = 0x2000u;
  std::string error;
  auto proxy = ExternalDeviceProxy::spawn(config, nullptr, error);
  return proxy == nullptr && error.find("configured MMIO size") != std::string::npos;
}

} // namespace

int main() {
  struct TestCase {
    const char* name;
    bool (*fn)();
  };

  const TestCase tests[] = {
      {"parse_external_plugin_spec", test_parse_external_plugin_spec},
      {"parse_external_plugin_irq_rejected", test_parse_external_plugin_irq_rejected},
      {"register_bank_proxy_mmio_roundtrip", test_register_bank_proxy_mmio_roundtrip},
      {"missing_entry_rejected", test_missing_entry_rejected},
      {"bad_abi_rejected", test_bad_abi_rejected},
      {"bad_manifest_rejected", test_bad_manifest_rejected},
      {"mmio_size_mismatch_rejected", test_mmio_size_mismatch_rejected},
  };

  for (const auto& test : tests) {
    if (!test.fn()) {
      std::cerr << "FAIL: " << test.name << '\n';
      return 1;
    }
  }

  std::cout << "PASS\n";
  return 0;
}
