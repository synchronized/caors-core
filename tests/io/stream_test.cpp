// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - file system opens read stream backed by read all", "[io]") {
  caors::io::memory_file_system fs;
  REQUIRE(fs.write_all("assets/config.json", std::string_view{"abcdef"}).succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::io::input_stream_result opened = fs.open_read("assets/config.json", &tracking);

  REQUIRE(opened.succeeded());
  REQUIRE(opened.value != nullptr);
  caors::io::input_stream& stream = *opened.value;
  CHECK_FALSE(stream.eof());

  std::array<std::byte, 3> chunk{};
  caors::io::size_result first = stream.read(chunk);
  REQUIRE(first.succeeded());
  CHECK(*first == 3U);
  CHECK(std::string_view{reinterpret_cast<const char*>(chunk.data()), chunk.size()} == "abc");

  caors::io::size_result second = stream.read(chunk);
  REQUIRE(second.succeeded());
  CHECK(*second == 3U);
  CHECK(std::string_view{reinterpret_cast<const char*>(chunk.data()), chunk.size()} == "def");
  CHECK(stream.eof());

  caors::io::size_result third = stream.read(chunk);
  REQUIRE(third.succeeded());
  CHECK(*third == 0U);
}

TEST_CASE("io - file system opens buffered write stream", "[io]") {
  caors::io::memory_file_system fs;

  caors::io::output_stream_result opened = fs.open_write("assets/config.json");

  REQUIRE(opened.succeeded());
  REQUIRE(opened.value != nullptr);

  caors::io::output_stream& stream = *opened.value;
  CHECK(stream.write_all(std::string_view{"abc"}).succeeded());
  CHECK(stream.write_all(std::string_view{"def"}).succeeded());
  CHECK_FALSE(fs.exists("assets/config.json"));
  CHECK(stream.close().succeeded());

  caors::io::byte_buffer_result bytes = fs.read_all("assets/config.json");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "abcdef");

  caors::io::size_result closed_write =
      stream.write(std::span<const std::byte>{reinterpret_cast<const std::byte*>("x"), 1});
  CHECK(closed_write.failed());
  CHECK(closed_write.error().domain() == caors::error_domain::io);
  CHECK(closed_write.error().value() == caors::errc::invalid_argument);
}
