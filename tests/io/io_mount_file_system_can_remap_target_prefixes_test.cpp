// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - mount file system can remap target prefixes", "[io]") {
  caors::memory_file_system storage;
  caors::mount_file_system mounted;

  REQUIRE(mounted.mount("textures", storage, "assets/textures").succeeded());
  REQUIRE(mounted.write_all("textures/hero.txt", std::string_view{"hero"}).succeeded());

  CHECK(storage.exists("assets/textures/hero.txt"));

  caors::byte_buffer_result read = mounted.read_all("textures/hero.txt");
  REQUIRE(read.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(read.value.data()), read.value.size()} ==
        "hero");
}
