// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - native file system can use a root directory", "[io]") {
  const std::filesystem::path directory = make_temp_dir();
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  const std::string root = path_string(directory);
  caors::native_file_system fs{root};

  CHECK(fs.root().string_view() == root);

  REQUIRE(fs.write_all("rooted.txt", std::string_view{"rooted"}).succeeded());
  CHECK(std::filesystem::exists(directory / "rooted.txt", ec));

  caors::byte_buffer_result read = fs.read_all("rooted.txt");
  REQUIRE(read.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(read.value.data()), read.value.size()} ==
        "rooted");

  const std::filesystem::path absolute_file = directory / "absolute.txt";
  REQUIRE(fs.write_all(path_string(absolute_file), std::string_view{"absolute"}).succeeded());
  caors::byte_buffer_result absolute_read = fs.read_all(path_string(absolute_file));
  REQUIRE(absolute_read.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(absolute_read.value.data()),
                         absolute_read.value.size()} == "absolute");

  std::filesystem::remove_all(directory, ec);
}
