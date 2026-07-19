// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "io_test_support.h"

using namespace caors::tests::io;

TEST_CASE("io - walk recursively visits entries", "[io]") {
  caors::io::memory_file_system fs;

  REQUIRE(fs.write_all("assets/a.txt", std::string_view{"a"}).succeeded());
  REQUIRE(fs.write_all("assets/nested/b.txt", std::string_view{"b"}).succeeded());
  REQUIRE(fs.write_all("assets/nested/deeper/c.txt", std::string_view{"c"}).succeeded());

  caors::vector<caors::string> visited;
  caors::error status = caors::io::walk(
      fs, "assets", [&](const caors::io::directory_entry& entry, std::size_t depth) {
        caors::string text;
        text.assign(entry.path.string_view().begin(), entry.path.string_view().end());
        text.push_back(':');
        text.append(std::to_string(depth));
        visited.push_back(std::move(text));
      });

  REQUIRE(status.succeeded());
  REQUIRE(visited.size() == 5);
  CHECK(visited[0] == "assets/a.txt:0");
  CHECK(visited[1] == "assets/nested:0");
  CHECK(visited[2] == "assets/nested/b.txt:1");
  CHECK(visited[3] == "assets/nested/deeper:1");
  CHECK(visited[4] == "assets/nested/deeper/c.txt:2");
}

TEST_CASE("io - walk can include root and skip children", "[io]") {
  caors::io::memory_file_system fs;

  REQUIRE(fs.write_all("assets/a.txt", std::string_view{"a"}).succeeded());
  REQUIRE(fs.write_all("assets/nested/b.txt", std::string_view{"b"}).succeeded());

  caors::vector<caors::string> visited;
  caors::io::walk_options options{};
  options.include_root = true;

  caors::error status = caors::io::walk(
      fs, "assets",
      [&](const caors::io::directory_entry& entry, std::size_t depth) {
        caors::string text;
        text.assign(entry.path.string_view().begin(), entry.path.string_view().end());
        text.push_back(':');
        text.append(std::to_string(depth));
        visited.push_back(std::move(text));
        return entry.name == "nested" ? caors::io::walk_action::skip_children
                                      : caors::io::walk_action::next;
      },
      options);

  REQUIRE(status.succeeded());
  REQUIRE(visited.size() == 3);
  CHECK(visited[0] == "assets:0");
  CHECK(visited[1] == "assets/a.txt:1");
  CHECK(visited[2] == "assets/nested:1");
}

TEST_CASE("io - walk reports listing errors", "[io]") {
  caors::io::memory_file_system fs;

  caors::error status = caors::io::walk(fs, "missing", [](const caors::io::directory_entry&) {});

  REQUIRE(status.failed());
  CHECK(status.domain() == caors::error_domain::io);
  CHECK(status.value() == caors::errc::not_found);
}
