// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - memory file system reports missing and supports erase", "[io]") {
  caors::io::memory_file_system fs;

  caors::io::byte_buffer_result empty = fs.read_all("");
  REQUIRE(empty.failed());
  CHECK(empty.error().domain() == caors::error_domain::io);
  CHECK(empty.error().value() == caors::errc::invalid_argument);

  caors::io::byte_buffer_result missing = fs.read_all("missing.txt");
  REQUIRE(missing.failed());
  CHECK(missing.error().domain() == caors::error_domain::io);
  CHECK(missing.error().value() == caors::errc::not_found);

  REQUIRE(fs.write_all("assets/data.txt", std::string_view{"data"}).succeeded());
  CHECK(fs.erase("/assets/data.txt"));
  CHECK_FALSE(fs.exists("assets/data.txt"));
  CHECK(fs.empty());

  REQUIRE(fs.write_all("assets/a.txt", std::string_view{"a"}).succeeded());
  REQUIRE(fs.write_all("assets/b.txt", std::string_view{"b"}).succeeded());
  CHECK(fs.file_count() == 2);
  fs.clear();
  CHECK(fs.empty());
}
