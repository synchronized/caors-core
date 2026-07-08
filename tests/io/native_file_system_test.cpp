// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：io_native_file_system_can_use_a_root_directory_test.cpp

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

// 原测试文件：io_native_file_system_reads_and_writes_bytes_test.cpp

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

// 原测试文件：io_native_file_system_reports_invalid_paths_and_missing_files_test.cpp

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

// 原测试文件：io_native_file_system_writes_text_test.cpp

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
