// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

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

  caors::io::native_file_system base_fs{path_string(base)};
  caors::io::native_file_system override_fs{path_string(override)};
  caors::io::mount_file_system mounted;

  REQUIRE(mounted.mount("assets", base_fs).succeeded());
  REQUIRE(mounted.mount("assets/special", override_fs).succeeded());
  CHECK(mounted.mount_count() == 2);

  REQUIRE(mounted.write_all("assets/config.txt", std::string_view{"base"}).succeeded());
  REQUIRE(mounted.write_all("assets/special/config.txt", std::string_view{"override"}).succeeded());

  caors::io::byte_buffer_result base_read = base_fs.read_all("config.txt");
  caors::io::byte_buffer_result override_read = override_fs.read_all("config.txt");

  REQUIRE(base_read.succeeded());
  REQUIRE(override_read.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(base_read->data()), base_read->size()} ==
        "base");
  CHECK(std::string_view{reinterpret_cast<const char*>(override_read->data()),
                         override_read->size()} == "override");
  CHECK(mounted.exists("assets/config.txt"));
  CHECK(mounted.exists("assets/special/config.txt"));

  caors::io::directory_entry_list_result entries = mounted.list("assets");
  REQUIRE(entries.succeeded());
  REQUIRE(entries->size() == 2);
  CHECK((*entries)[0].name == "config.txt");
  CHECK((*entries)[0].path == "assets/config.txt");
  CHECK((*entries)[0].kind == caors::io::file_kind::file);
  CHECK((*entries)[1].name == "special");
  CHECK((*entries)[1].path == "assets/special");
  CHECK((*entries)[1].kind == caors::io::file_kind::directory);

  caors::io::byte_buffer_result missing = mounted.read_all("missing/config.txt");
  REQUIRE(missing.failed());
  CHECK(missing.error().domain() == caors::error_domain::io);
  CHECK(missing.error().value() == caors::errc::not_found);

  CHECK(mounted.unmount("assets/special"));
  CHECK(mounted.mount_count() == 1);

  std::filesystem::remove_all(directory, ec);
}
