// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - uri parses scheme authority and path", "[io]") {
  const caors::io::uri_view uri{"caors://assets/textures/albedo.png?debug=true"};

  CHECK(uri.scheme() == "caors");
  CHECK(uri.authority() == "assets");
  CHECK(uri.path() == "textures/albedo.png");
  CHECK(uri.has_scheme());
  CHECK(uri.has_authority());

  const caors::io::uri relative{"assets/config.json"};
  CHECK(relative.scheme().empty());
  CHECK(relative.authority().empty());
  CHECK(relative.path() == "assets/config.json");
}

TEST_CASE("io - file system capabilities describe implemented backend features", "[io]") {
  caors::io::memory_file_system files;

  const caors::io::file_system_capabilities capabilities = files.capabilities();

  CHECK(capabilities.read);
  CHECK(capabilities.write);
  CHECK(capabilities.status);
  CHECK(capabilities.list);
  CHECK_FALSE(capabilities.watch);
}

TEST_CASE("io - file system resolver routes uri locations", "[io]") {
  caors::io::memory_file_system files;
  REQUIRE(files.write_all("textures/albedo.txt", std::string_view{"albedo"}).succeeded());

  caors::io::file_system_resolver resolver;
  REQUIRE(resolver.mount("caors://assets", files).succeeded());

  CHECK(resolver.mount_count() == 1);
  CHECK(resolver.exists("caors://assets/textures/albedo.txt"));

  caors::io::byte_buffer_result bytes = resolver.read_all("caors://assets/textures/albedo.txt");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "albedo");

  caors::io::directory_entry_list_result entries = resolver.list("caors://assets/textures");
  REQUIRE(entries.succeeded());
  REQUIRE(entries->size() == 1);
  CHECK((*entries)[0].name == "albedo.txt");
  CHECK((*entries)[0].path == "caors://assets/textures/albedo.txt");

  REQUIRE(resolver.write_all("caors://assets/generated.txt", std::string_view{"generated"})
              .succeeded());
  CHECK(files.exists("generated.txt"));
}

TEST_CASE("io - file system resolver supports target path prefix", "[io]") {
  caors::io::memory_file_system files;
  REQUIRE(files.write_all("project/assets/config.json", std::string_view{"{}"}).succeeded());

  caors::io::file_system_resolver resolver;
  REQUIRE(resolver.mount("caors://assets", files, "project/assets").succeeded());

  CHECK(resolver.exists("caors://assets/config.json"));
  caors::io::byte_buffer_result bytes = resolver.read_all("caors://assets/config.json");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "{}");
}
