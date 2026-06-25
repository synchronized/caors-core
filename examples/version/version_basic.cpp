// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/version.h"

#include <iostream>

int main() {
  const caors::version version = caors::get_version();

  std::cout << "caors version: " << version.major << '.' << version.minor << '.' << version.patch
            << '\n';
  std::cout << "caors version string: " << caors::get_version_string() << '\n';

  return 0;
}
