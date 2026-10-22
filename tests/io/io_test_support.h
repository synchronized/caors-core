// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/io/legacy.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

namespace caors::tests::io {

[[nodiscard]] inline std::filesystem::path make_temp_dir() {
  static std::atomic_uint64_t counter{0};
  const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                      "_" + std::to_string(counter.fetch_add(1));
  return std::filesystem::temp_directory_path() / ("caors_io_test_" + suffix);
}

[[nodiscard]] inline std::string path_string(const std::filesystem::path& path) {
  return path.string();
}

} // namespace caors::tests::io
