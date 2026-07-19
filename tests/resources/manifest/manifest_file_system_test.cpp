// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

namespace {

class read_write_file_system_without_listing final : public caors::io::file_system {
public:
  using caors::io::file_system::write_all;

  [[nodiscard]] bool exists(caors::io::path_view path) const override {
    return storage_.exists(path);
  }

  [[nodiscard]] caors::io::file_system_capabilities capabilities() const noexcept override {
    caors::io::file_system_capabilities result{};
    result.read = true;
    result.write = true;
    return result;
  }

  [[nodiscard]] caors::io::byte_buffer_result
  read_all(caors::io::path_view path, caors::memory_resource* resource = nullptr) override {
    return storage_.read_all(path, resource);
  }

  [[nodiscard]] caors::error write_all(caors::io::path_view path,
                                       std::span<const std::byte> bytes) override {
    return storage_.write_all(path, bytes);
  }

private:
  caors::io::memory_file_system storage_;
};

} // namespace

TEST_CASE("resources - manifest file system lists directories from manifest", "[resources]") {
  read_write_file_system_without_listing storage;
  REQUIRE(storage.write_all("assets/textures/albedo.txt", std::string_view{"albedo"}).succeeded());
  REQUIRE(
      storage.write_all("assets/materials/basic.txt", std::string_view{"material"}).succeeded());

  caors::resources::manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/textures/albedo.txt").succeeded());
  REQUIRE(manifest.bind(test_guid_b, "assets/materials/basic.txt").succeeded());

  caors::resources::manifest_file_system files{manifest, storage};

  const caors::io::file_system_capabilities capabilities = files.capabilities();
  CHECK(capabilities.read);
  CHECK(capabilities.write);
  CHECK(capabilities.status);
  CHECK(capabilities.list);

  caors::io::directory_entry_list_result root_entries = files.list("assets");
  REQUIRE(root_entries.succeeded());
  REQUIRE(root_entries->size() == 2);
  CHECK((*root_entries)[0].name == "materials");
  CHECK((*root_entries)[0].kind == caors::io::file_kind::directory);
  CHECK((*root_entries)[1].name == "textures");
  CHECK((*root_entries)[1].kind == caors::io::file_kind::directory);

  caors::io::directory_entry_list_result texture_entries = files.list("assets/textures");
  REQUIRE(texture_entries.succeeded());
  REQUIRE(texture_entries->size() == 1);
  CHECK((*texture_entries)[0].name == "albedo.txt");
  CHECK((*texture_entries)[0].kind == caors::io::file_kind::file);

  caors::io::file_status_result texture_status = files.status("assets/textures");
  REQUIRE(texture_status.succeeded());
  CHECK(texture_status->kind == caors::io::file_kind::directory);

  caors::io::byte_buffer_result bytes = files.read_all("assets/textures/albedo.txt");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "albedo");
}
