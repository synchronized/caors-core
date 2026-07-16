// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - default native file system is stable", "[io]") {
  caors::io::native_file_system& first = caors::io::get_native_file_system();
  caors::io::native_file_system& second = caors::io::get_native_file_system();

  CHECK(&first == &second);
}
