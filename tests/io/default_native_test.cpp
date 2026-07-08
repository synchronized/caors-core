// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - default native file system is stable", "[io]") {
  caors::native_file_system& first = caors::get_native_file_system();
  caors::native_file_system& second = caors::get_native_file_system();

  CHECK(&first == &second);
}
