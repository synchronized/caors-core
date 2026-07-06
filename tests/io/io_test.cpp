// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

namespace {

[[nodiscard]] std::filesystem::path make_temp_dir() {
  static std::atomic_uint64_t counter{0};
  const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                      "_" + std::to_string(counter.fetch_add(1));
  return std::filesystem::temp_directory_path() / ("caors_io_test_" + suffix);
}

[[nodiscard]] std::string path_string(const std::filesystem::path& path) { return path.string(); }

} // namespace

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

TEST_CASE("io - default native file system is stable", "[io]") {
  caors::native_file_system& first = caors::get_native_file_system();
  caors::native_file_system& second = caors::get_native_file_system();

  CHECK(&first == &second);
}

TEST_CASE("io - mount file system routes virtual prefixes", "[io]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path base = directory / "base";
  const std::filesystem::path override = directory / "override";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(base, ec));
  REQUIRE_FALSE(ec);
  REQUIRE(std::filesystem::create_directories(override, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system native;
  caors::mount_file_system mounted;

  REQUIRE(mounted.mount("assets", native, path_string(base)).succeeded());
  REQUIRE(mounted.mount("assets/special", native, path_string(override)).succeeded());
  CHECK(mounted.mount_count() == 2);

  REQUIRE(mounted.write_all("assets/config.txt", std::string_view{"base"}).succeeded());
  REQUIRE(mounted.write_all("assets/special/config.txt", std::string_view{"override"}).succeeded());

  caors::byte_buffer_result base_read = native.read_all(path_string(base / "config.txt"));
  caors::byte_buffer_result override_read = native.read_all(path_string(override / "config.txt"));

  REQUIRE(base_read.succeeded());
  REQUIRE(override_read.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(base_read.value.data()),
                         base_read.value.size()} == "base");
  CHECK(std::string_view{reinterpret_cast<const char*>(override_read.value.data()),
                         override_read.value.size()} == "override");
  CHECK(mounted.exists("assets/config.txt"));
  CHECK(mounted.exists("assets/special/config.txt"));

  caors::byte_buffer_result missing = mounted.read_all("missing/config.txt");
  REQUIRE(missing.failed());
  CHECK(missing.status.domain() == caors::error_domain::io);
  CHECK(missing.status.value() == caors::errc::not_found);

  CHECK(mounted.unmount("assets/special"));
  CHECK(mounted.mount_count() == 1);

  std::filesystem::remove_all(directory, ec);
}
