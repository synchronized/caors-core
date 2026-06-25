// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/version.h"

namespace caors {

version get_version() noexcept { return k_version; }

std::string_view get_version_string() noexcept { return k_version_string; }

}
