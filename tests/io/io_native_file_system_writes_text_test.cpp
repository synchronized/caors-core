// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - native file system writes text", "[io]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "text.txt";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system fs;
  const std::string file_path = path_string(file);

  REQUIRE(fs.write_all(file_path, std::string_view{"hello"}).succeeded());
  caors::byte_buffer_result read = fs.read_all(file_path);

  REQUIRE(read.succeeded());
  REQUIRE(read.value.size() == 5);
  CHECK(std::string_view{reinterpret_cast<const char*>(read.value.data()), read.value.size()} ==
        "hello");

  std::filesystem::remove_all(directory, ec);
}
