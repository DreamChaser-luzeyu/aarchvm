#include "aarchvm/plugin_config.hpp"

#include <cctype>
#include <stdexcept>

namespace aarchvm {

namespace {

std::string trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return std::string(text.substr(start, end - start));
}

bool parse_u64(std::string_view text, std::uint64_t& value) {
  try {
    std::size_t consumed = 0;
    const std::string owned(text);
    const unsigned long long parsed = std::stoull(owned, &consumed, 0);
    if (consumed != owned.size()) {
      return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

std::string default_instance_name(std::string_view path) {
  const std::size_t slash = path.find_last_of("/\\");
  std::string name(path.substr((slash == std::string_view::npos) ? 0u : (slash + 1u)));
  const std::size_t dot = name.find_last_of('.');
  if (dot != std::string::npos) {
    name.erase(dot);
  }
  if (name.empty()) {
    name = "plugin";
  }
  return name;
}

} // namespace

bool parse_external_plugin_spec(std::string_view spec,
                                ExternalPluginConfig& out,
                                std::string& error) {
  out = {};
  const std::size_t first_comma = spec.find(',');
  const std::string path = trim(spec.substr(0, first_comma));
  if (path.empty()) {
    error = "missing shared-object path before plugin options";
    return false;
  }
  out.shared_object_path = path;

  bool saw_mmio = false;
  bool saw_size = false;
  std::size_t cursor = (first_comma == std::string_view::npos) ? spec.size() : (first_comma + 1u);
  while (cursor < spec.size()) {
    const std::size_t next_comma = spec.find(',', cursor);
    const std::string_view raw_token =
        (next_comma == std::string_view::npos) ? spec.substr(cursor) : spec.substr(cursor, next_comma - cursor);
    const std::string token = trim(raw_token);
    if (token.empty()) {
      error = "empty plugin option in -plugin spec";
      return false;
    }
    const std::size_t eq = token.find('=');
    if (eq == std::string::npos || eq == 0u || eq + 1u >= token.size()) {
      error = "plugin option must be key=value: " + token;
      return false;
    }

    const std::string key = token.substr(0, eq);
    if (key == "arg") {
      out.opaque_arg = trim(spec.substr(cursor + eq + 1u));
      cursor = spec.size();
      break;
    }

    const std::string value = token.substr(eq + 1u);
    if (key == "mmio") {
      if (saw_mmio) {
        error = "duplicate mmio= option";
        return false;
      }
      if (!parse_u64(value, out.mmio_base)) {
        error = "invalid mmio base: " + value;
        return false;
      }
      saw_mmio = true;
    } else if (key == "size") {
      if (saw_size) {
        error = "duplicate size= option";
        return false;
      }
      if (!parse_u64(value, out.mmio_size) || out.mmio_size == 0u) {
        error = "invalid non-zero MMIO size: " + value;
        return false;
      }
      saw_size = true;
    } else if (key == "name") {
      out.instance_name = value;
    } else if (key == "irq" || key == "irqs") {
      error = "plugin IRQ routing is not implemented yet in this stage";
      return false;
    } else {
      error = "unknown plugin option: " + key;
      return false;
    }

    if (next_comma == std::string_view::npos) {
      break;
    }
    cursor = next_comma + 1u;
  }

  if (!saw_mmio) {
    error = "missing required mmio= option";
    return false;
  }
  if (!saw_size) {
    error = "missing required size= option";
    return false;
  }
  if (out.instance_name.empty()) {
    out.instance_name = default_instance_name(out.shared_object_path);
  }
  return true;
}

} // namespace aarchvm
