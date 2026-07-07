// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include <chrono>
#include <filesystem>
#include <optional>

namespace caors::tests::serialization::builtin_path_chrono {

struct late_builtin_codec_actor {
  std::chrono::seconds timeout{};
};

} // namespace caors::tests::serialization::builtin_path_chrono
