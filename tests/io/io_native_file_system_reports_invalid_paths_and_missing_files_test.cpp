// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - native file system reports invalid paths and missing files", "[io]") {
  caors::native_file_system fs;

  caors::byte_buffer_result empty = fs.read_all("");
  REQUIRE(empty.failed());
  CHECK(empty.status.domain() == caors::error_domain::io);
  CHECK(empty.status.value() == caors::errc::invalid_argument);

  caors::byte_buffer_result missing = fs.read_all(path_string(make_temp_dir() / "missing.bin"));
  REQUIRE(missing.failed());
  CHECK(missing.status.domain() == caors::error_domain::io);
  CHECK(missing.status.value() == caors::errc::not_found);

  const caors::error write_empty = fs.write_all("", std::string_view{"data"});
  REQUIRE(write_empty.failed());
  CHECK(write_empty.domain() == caors::error_domain::io);
  CHECK(write_empty.value() == caors::errc::invalid_argument);
}
