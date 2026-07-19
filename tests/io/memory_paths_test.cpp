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
  CHECK(read->get_allocator().resource() == &tracking);
  CHECK(std::string_view{reinterpret_cast<const char*>(read->data()), read->size()} == "memory");

  REQUIRE(fs.write_all("assets/data.txt", std::string_view{"updated"}).succeeded());
  CHECK(fs.file_count() == 1);

  caors::io::byte_buffer_result updated = fs.read_all("assets/data.txt");
  REQUIRE(updated.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(updated->data()), updated->size()} ==
        "updated");
}

TEST_CASE("io - memory file system lists virtual directories", "[io]") {
  caors::io::memory_file_system fs;

  REQUIRE(fs.write_all("assets/a.txt", std::string_view{"a"}).succeeded());
  REQUIRE(fs.write_all("assets/nested/b.txt", std::string_view{"b"}).succeeded());
  REQUIRE(fs.write_all("readme.txt", std::string_view{"readme"}).succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::io::directory_entry_list_result root = fs.list("", &tracking);

  REQUIRE(root.succeeded());
  CHECK(root->get_allocator().resource() == &tracking);
  REQUIRE(root->size() == 2);
  CHECK((*root)[0].name == "assets");
  CHECK((*root)[0].path == "assets");
  CHECK((*root)[0].kind == caors::io::file_kind::directory);
  CHECK((*root)[1].name == "readme.txt");
  CHECK((*root)[1].path == "readme.txt");
  CHECK((*root)[1].kind == caors::io::file_kind::file);

  caors::io::directory_entry_list_result assets = fs.list("assets", &tracking);

  REQUIRE(assets.succeeded());
  REQUIRE(assets->size() == 2);
  CHECK((*assets)[0].name == "a.txt");
  CHECK((*assets)[0].path == "assets/a.txt");
  CHECK((*assets)[0].kind == caors::io::file_kind::file);
  CHECK((*assets)[1].name == "nested");
  CHECK((*assets)[1].path == "assets/nested");
  CHECK((*assets)[1].kind == caors::io::file_kind::directory);

  caors::io::directory_entry_list_result missing = fs.list("missing");
  REQUIRE(missing.failed());
  CHECK(missing.error().domain() == caors::error_domain::io);
  CHECK(missing.error().value() == caors::errc::not_found);
}

TEST_CASE("io - memory file system reports virtual file status", "[io]") {
  caors::io::memory_file_system fs;

  REQUIRE(fs.write_all("assets/a.txt", std::string_view{"abc"}).succeeded());
  REQUIRE(fs.write_all("assets/nested/b.txt", std::string_view{"b"}).succeeded());

  caors::io::file_status_result root = fs.status("");
  REQUIRE(root.succeeded());
  CHECK(root->kind == caors::io::file_kind::directory);
  CHECK_FALSE(root->has_size);

  caors::io::file_status_result file = fs.status("assets/a.txt");
  REQUIRE(file.succeeded());
  CHECK(file->kind == caors::io::file_kind::file);
  CHECK(file->has_size);
  CHECK(file->size == 3);
  CHECK_FALSE(file->has_modified_time);
  CHECK_FALSE(file->has_read_only);

  caors::io::file_status_result directory = fs.status("assets/nested");
  REQUIRE(directory.succeeded());
  CHECK(directory->kind == caors::io::file_kind::directory);
  CHECK_FALSE(directory->has_size);

  caors::io::file_status_result missing = fs.status("missing");
  REQUIRE(missing.failed());
  CHECK(missing.error().domain() == caors::error_domain::io);
  CHECK(missing.error().value() == caors::errc::not_found);
}
