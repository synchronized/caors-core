// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - memory file system stores normalized virtual paths", "[io]") {
  caors::io::memory_file_system fs;

  REQUIRE(fs.write_all("\\assets//data.txt", std::string_view{"memory"}).succeeded());

  CHECK(fs.file_count() == 1);
  CHECK(fs.exists("assets/data.txt"));
  CHECK(fs.exists("/assets\\data.txt"));

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::io::byte_buffer_result read = fs.read_all("assets/data.txt", &tracking);

  REQUIRE(read.succeeded());
  CHECK(read.value.get_allocator().resource() == &tracking);
  CHECK(std::string_view{reinterpret_cast<const char*>(read.value.data()), read.value.size()} ==
        "memory");

  REQUIRE(fs.write_all("assets/data.txt", std::string_view{"updated"}).succeeded());
  CHECK(fs.file_count() == 1);

  caors::io::byte_buffer_result updated = fs.read_all("assets/data.txt");
  REQUIRE(updated.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(updated.value.data()),
                         updated.value.size()} == "updated");
}
