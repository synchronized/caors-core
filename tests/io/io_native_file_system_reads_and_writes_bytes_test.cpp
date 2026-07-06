// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - native file system reads and writes bytes", "[io]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "bytes.bin";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system fs;
  const std::string file_path = path_string(file);
  const std::array<std::byte, 4> expected{
      std::byte{0x01},
      std::byte{0x7f},
      std::byte{0x80},
      std::byte{0xff},
  };

  CHECK_FALSE(fs.exists(file_path));
  REQUIRE(fs.write_all(file_path, std::span<const std::byte>{expected}).succeeded());
  CHECK(fs.exists(file_path));

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::byte_buffer_result read = fs.read_all(file_path, &tracking);

  REQUIRE(read.succeeded());
  CHECK(read.value.get_allocator().resource() == &tracking);
  REQUIRE(read.value.size() == expected.size());
  CHECK(std::equal(read.value.begin(), read.value.end(), expected.begin(), expected.end()));

  std::filesystem::remove_all(directory, ec);
}
