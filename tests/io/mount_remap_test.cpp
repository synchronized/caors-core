// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - mount file system can remap target prefixes", "[io]") {
  caors::io::memory_file_system storage;
  caors::io::mount_file_system mounted;

  REQUIRE(mounted.mount("textures", storage, "assets/textures").succeeded());
  REQUIRE(mounted.write_all("textures/hero.txt", std::string_view{"hero"}).succeeded());

  CHECK(storage.exists("assets/textures/hero.txt"));

  caors::io::byte_buffer_result read = mounted.read_all("textures/hero.txt");
  REQUIRE(read.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(read->data()), read->size()} == "hero");

  caors::io::directory_entry_list_result root = mounted.list("");
  REQUIRE(root.succeeded());
  REQUIRE(root->size() == 1);
  CHECK((*root)[0].name == "textures");
  CHECK((*root)[0].path == "textures");
  CHECK((*root)[0].kind == caors::io::file_kind::directory);

  caors::io::directory_entry_list_result textures = mounted.list("textures");
  REQUIRE(textures.succeeded());
  REQUIRE(textures->size() == 1);
  CHECK((*textures)[0].name == "hero.txt");
  CHECK((*textures)[0].path == "textures/hero.txt");
  CHECK((*textures)[0].kind == caors::io::file_kind::file);
}
