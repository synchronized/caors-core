// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/native_file_system.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>

namespace caors::io {
namespace {

[[nodiscard]] std::filesystem::path native_path(path_view path) {
  return std::filesystem::path{std::string{path.string_view()}};
}

[[nodiscard]] error validate_path(path_view path) noexcept {
  return path.empty() ? error{error_domain::io, errc::invalid_argument} : error{};
}

} // namespace

bool native_file_system::exists(path_view path) const {
  if (path.empty()) {
    return false;
  }

  std::error_code ec;
  return std::filesystem::exists(native_path(path), ec) && !ec;
}

byte_buffer_result native_file_system::read_all(path_view path, memory_resource* resource) {
  error status = validate_path(path);
  if (status) {
    return {status, {}};
  }

  if (!exists(path)) {
    return {{error_domain::io, errc::not_found}, {}};
  }

  std::ifstream input{native_path(path), std::ios::binary | std::ios::ate};
  if (!input) {
    return {{error_domain::io, errc::backend_failure}, {}};
  }

  const std::streampos end = input.tellg();
  if (end < std::streampos{0}) {
    return {{error_domain::io, errc::backend_failure}, {}};
  }

  byte_buffer bytes = make_byte_buffer(resource);
  bytes.resize(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
      return {{error_domain::io, errc::backend_failure}, {}};
    }
  }

  return {{}, std::move(bytes)};
}

error native_file_system::write_all(path_view path, std::span<const std::byte> bytes) {
  error status = validate_path(path);
  if (status) {
    return status;
  }

  std::ofstream output{native_path(path), std::ios::binary | std::ios::trunc};
  if (!output) {
    return {error_domain::io, errc::backend_failure};
  }

  if (!bytes.empty()) {
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
      return {error_domain::io, errc::backend_failure};
    }
  }

  return {};
}

native_file_system& get_native_file_system() noexcept {
  static native_file_system instance;
  return instance;
}

} // namespace caors::io
