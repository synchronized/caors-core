// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources.h"

#include "caors/core/containers.h"
#include "caors/io.h"
#include "caors/memory/tracking.h"
#include "caors/reflection.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr caors::resource_guid test_guid_a{caors::guid::from_bytes(
    caors::guid::byte_array{0x6c, 0xd2, 0xda, 0x3d, 0xb5, 0x49, 0x47, 0x56, 0x8b, 0x8b, 0x0c, 0x84,
                            0x29, 0x3a, 0xf4, 0x10})};

constexpr caors::resource_guid test_guid_b{caors::guid::from_bytes(
    caors::guid::byte_array{0x0c, 0x2f, 0x82, 0xf8, 0x2b, 0x3e, 0x4f, 0x77, 0x96, 0x5c, 0x4f, 0x2d,
                            0x1c, 0x4a, 0x90, 0x7a})};

[[nodiscard]] std::filesystem::path make_temp_dir() {
  static std::atomic_uint64_t counter{0};
  const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                      "_" + std::to_string(counter.fetch_add(1));
  return std::filesystem::temp_directory_path() / ("caors_resources_test_" + suffix);
}

[[nodiscard]] std::string path_string(const std::filesystem::path& path) { return path.string(); }

class texture_resource : public caors::resource {
public:
  using caors::resource::resource;
};

class image_texture_resource : public texture_resource {
public:
  using texture_resource::texture_resource;
};

class observed_resource : public caors::resource {
public:
  using caors::resource::resource;

  ~observed_resource() override { ++destroyed; }

  inline static int destroyed = 0;
};

class serialized_resource : public caors::resource {
public:
  using caors::resource::resource;

  int width = 0;
};

class counting_texture_loader final : public caors::resource_loader<texture_resource> {
public:
  [[nodiscard]] caors::resource_load_result<texture_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    ++load_count;
    auto loaded = caors::memory::make_unique<texture_resource>(resource, guid);
    return {{}, std::move(loaded)};
  }

  int load_count = 0;
};

class counting_texture_saver final : public caors::resource_saver<texture_resource> {
public:
  [[nodiscard]] caors::error save(const texture_resource& value) override {
    ++save_count;
    saved_guid = value.guid();
    return {};
  }

  int save_count = 0;
  caors::resource_guid saved_guid{};
};

[[nodiscard]] bool reflection_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_reflection;
}

[[nodiscard]] bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

void require_serialized_resource_metadata() {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serialized_resource>("caors.test.resources.serialized_resource")
          .field("width", &serialized_resource::width)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<serialized_resource>()));
}

} // namespace

TEST_CASE("resources - resource guid wraps stable guid", "[resources]") {
  CHECK(caors::resource_guid::nil().is_nil());
  CHECK_FALSE(static_cast<bool>(caors::resource_guid::nil()));
  CHECK(test_guid_a);
  CHECK(test_guid_a != test_guid_b);

  const auto parsed = caors::resource_guid::parse(test_guid_a.string());
  REQUIRE(parsed.has_value());
  CHECK(*parsed == test_guid_a);
  CHECK_FALSE(caors::resource_guid::parse("not-a-guid").has_value());

  caors::hash_map<caors::resource_guid, int, caors::resource_guid_hash> values;
  values.emplace(test_guid_a, 7);
  REQUIRE(values.contains(test_guid_a));
  CHECK(values[test_guid_a] == 7);
}

TEST_CASE("resources - resource stores stable identity", "[resources]") {
  texture_resource texture;

  CHECK_FALSE(texture.has_guid());
  CHECK(texture.guid() == caors::resource_guid::nil());

  texture.set_guid(test_guid_a);

  CHECK(texture.has_guid());
  CHECK(texture.guid() == test_guid_a);
}

TEST_CASE("resources - ref can hold unresolved guid", "[resources]") {
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK(reference);
  CHECK(reference.has_guid());
  CHECK_FALSE(reference.resolved());
  CHECK(reference.get() == nullptr);
  CHECK(reference.guid() == test_guid_a);

  reference.unresolve();
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref resolves compatible resource", "[resources]") {
  texture_resource texture{test_guid_a};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  REQUIRE(reference.resolve(&texture));

  CHECK(reference.resolved());
  CHECK(reference.get() == &texture);
  CHECK(reference->guid() == test_guid_a);
  CHECK((*reference).guid() == test_guid_a);

  reference.unresolve();

  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref rejects mismatched guid", "[resources]") {
  texture_resource texture{test_guid_b};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK_FALSE(reference.resolve(&texture));
  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref supports derived to base conversion", "[resources]") {
  image_texture_resource image{test_guid_a};
  caors::resources::ref<image_texture_resource> derived{&image};
  caors::resource_ref<texture_resource> base{derived};

  CHECK(base.resolved());
  CHECK(base.get() == &image);
  CHECK(base.guid() == test_guid_a);

  base.reset();
  CHECK_FALSE(base);
  CHECK_FALSE(base.resolved());
  CHECK(base.guid() == caors::resource_guid::nil());
}

TEST_CASE("resources - manager binds and finds resources", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture{test_guid_a};

  CHECK(manager.empty());
  CHECK(manager.resource() != nullptr);

  REQUIRE(manager.bind(&texture));

  CHECK(manager.size() == 1);
  CHECK(manager.contains(test_guid_a));
  CHECK(manager.contains(&texture));
  CHECK(manager.find(test_guid_a) == &texture);
  CHECK(manager.find<texture_resource>(test_guid_a) == &texture);
  CHECK(manager.find<image_texture_resource>(test_guid_a) == nullptr);
  CHECK(manager.guid_of(&texture) == test_guid_a);
}

TEST_CASE("resources - manager can assign guid while binding", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture;

  REQUIRE(manager.bind(test_guid_a, &texture));

  CHECK(texture.guid() == test_guid_a);
  CHECK(manager.find(test_guid_a) == &texture);
}

TEST_CASE("resources - manager rejects conflicting bindings", "[resources]") {
  caors::resource_manager manager;
  texture_resource first{test_guid_a};
  texture_resource second{test_guid_b};

  REQUIRE(manager.bind(&first));

  CHECK(manager.bind(&first));
  CHECK_FALSE(manager.bind(test_guid_b, &first));
  CHECK_FALSE(manager.bind(test_guid_a, &second));
  CHECK_FALSE(manager.bind(caors::resource_guid::nil(), &second));
  CHECK_FALSE(manager.bind(test_guid_b, nullptr));

  CHECK(manager.size() == 1);
  CHECK(manager.find(test_guid_a) == &first);
}

TEST_CASE("resources - manager unbinds by guid and resource pointer", "[resources]") {
  caors::resource_manager manager;
  texture_resource first{test_guid_a};
  texture_resource second{test_guid_b};

  REQUIRE(manager.bind(&first));
  REQUIRE(manager.bind(&second));

  CHECK(manager.unbind(test_guid_a));
  CHECK_FALSE(manager.unbind(test_guid_a));
  CHECK_FALSE(manager.contains(test_guid_a));
  CHECK_FALSE(manager.contains(&first));
  CHECK(manager.find(test_guid_a) == nullptr);
  CHECK(manager.guid_of(&first) == caors::resource_guid::nil());

  CHECK(manager.unbind(&second));
  CHECK(manager.empty());
  CHECK(manager.find(test_guid_b) == nullptr);
}

TEST_CASE("resources - manager clears without destroying resources", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture{test_guid_a};

  REQUIRE(manager.bind(&texture));

  manager.clear();

  CHECK(manager.empty());
  CHECK(texture.guid() == test_guid_a);
  CHECK(manager.find(test_guid_a) == nullptr);
}

TEST_CASE("resources - manager resolves resource refs", "[resources]") {
  caors::resource_manager manager;
  image_texture_resource image{test_guid_a};
  texture_resource other{test_guid_b};

  REQUIRE(manager.bind(&image));
  REQUIRE(manager.bind(&other));

  caors::resource_ref<texture_resource> base_ref{test_guid_a};
  REQUIRE(manager.resolve(base_ref));

  CHECK(base_ref.resolved());
  CHECK(base_ref.get() == &image);
  CHECK(base_ref.guid() == test_guid_a);

  caors::resources::ref<image_texture_resource> derived_ref{test_guid_a};
  REQUIRE(manager.resolve(derived_ref));
  CHECK(derived_ref.get() == &image);

  caors::resources::ref<image_texture_resource> mismatched_type{test_guid_b};
  CHECK_FALSE(manager.resolve(mismatched_type));
  CHECK_FALSE(mismatched_type.resolved());
  CHECK(mismatched_type.guid() == test_guid_b);

  caors::resources::ref<texture_resource> missing{caors::resource_guid::nil()};
  CHECK_FALSE(manager.resolve(missing));
}

TEST_CASE("resources - cache creates and indexes resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  texture_resource* texture = cache.create(test_guid_a);
  REQUIRE(texture != nullptr);

  CHECK(cache.size() == 1);
  CHECK(cache.contains(test_guid_a));
  CHECK(texture->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == texture);
  CHECK(cache.find_as<texture_resource>(test_guid_a) == texture);
  CHECK(cache.index().find(test_guid_a) == texture);
}

TEST_CASE("resources - cache rejects invalid duplicate and mismatched resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  CHECK(cache.create(caors::resource_guid::nil()) == nullptr);
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.create(test_guid_a) == nullptr);
  CHECK(cache.create(test_guid_b, test_guid_a) == nullptr);
  CHECK_FALSE(cache.contains(test_guid_b));
}

TEST_CASE("resources - cache resolves refs through internal manager", "[resources]") {
  caors::resource_cache<image_texture_resource> cache;
  image_texture_resource* image = cache.create(test_guid_a);
  REQUIRE(image != nullptr);

  caors::resource_ref<texture_resource> base_ref{test_guid_a};
  REQUIRE(cache.resolve(base_ref));

  CHECK(base_ref.resolved());
  CHECK(base_ref.get() == image);

  caors::resources::ref<image_texture_resource> derived_ref{test_guid_a};
  REQUIRE(cache.resolve(derived_ref));
  CHECK(derived_ref.get() == image);
}

TEST_CASE("resources - cache inserts owned resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  auto texture = caors::memory::make_unique<texture_resource>(nullptr, test_guid_a);
  texture_resource* stored = texture.get();

  REQUIRE(cache.insert(std::move(texture)));

  CHECK(cache.size() == 1);
  CHECK(cache.find(test_guid_a) == stored);
  CHECK(cache.index().find(test_guid_a) == stored);
  CHECK_FALSE(cache.insert({}));
}

TEST_CASE("resources - cache erase destroys owned resource", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.erase(test_guid_a));

  CHECK(observed_resource::destroyed == 1);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK_FALSE(cache.erase(test_guid_a));
}

TEST_CASE("resources - cache release transfers ownership", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  auto released = cache.release(test_guid_a);

  REQUIRE(released != nullptr);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK(released->guid() == test_guid_a);
  CHECK(observed_resource::destroyed == 0);

  released.reset();
  CHECK(observed_resource::destroyed == 1);
}

TEST_CASE("resources - loader can populate cache", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::result<texture_resource*> loaded = caors::load_into(cache, loader, test_guid_a);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);

  CHECK(loader.load_count == 1);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == loaded.value);

  caors::result<texture_resource*> cached = caors::load_into(cache, loader, test_guid_a);
  REQUIRE(cached.succeeded());
  CHECK(cached.value == loaded.value);
  CHECK(loader.load_count == 1);
}

TEST_CASE("resources - saver interface receives resources", "[resources]") {
  texture_resource texture{test_guid_a};
  counting_texture_saver saver;

  const caors::error status = saver.save(texture);

  REQUIRE(status.succeeded());
  CHECK(saver.save_count == 1);
  CHECK(saver.saved_guid == test_guid_a);
}

TEST_CASE("resources - serialized helpers load and save resources", "[resources]") {
  require_serialized_resource_metadata();

  const caors::serialization::variant_result parsed = caors::parse_json(R"({"width":256})");
  REQUIRE(parsed.succeeded());

  caors::serialization::variant_reader reader{parsed.value};
  caors::resource_load_result<serialized_resource> loaded =
      caors::load_serialized<serialized_resource>(test_guid_a, reader);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 256);

  caors::string output;
  caors::serialization::json_writer writer{output};
  const caors::error save_status = caors::save_serialized(*loaded.value, writer);

  REQUIRE(save_status.succeeded());
  CHECK(output == R"({"width":256})");
}

TEST_CASE("resources - serialized helpers reject invalid resource identity", "[resources]") {
  require_serialized_resource_metadata();

  const caors::serialization::variant_result parsed = caors::parse_json(R"({"width":256})");
  REQUIRE(parsed.succeeded());

  caors::serialization::variant_reader reader{parsed.value};
  caors::resource_load_result<serialized_resource> loaded =
      caors::load_serialized<serialized_resource>(caors::resource_guid::nil(), reader);

  REQUIRE(loaded.failed());
  CHECK(loaded.status.domain() == caors::error_domain::resources);
  CHECK(loaded.status.value() == caors::errc::invalid_argument);

  serialized_resource value{};
  caors::serialization::json_writer writer;
  const caors::error save_status = caors::save_serialized(value, writer);

  REQUIRE(save_status.failed());
  CHECK(save_status.domain() == caors::error_domain::resources);
  CHECK(save_status.value() == caors::errc::invalid_argument);
}

TEST_CASE("resources - cache uses requested memory resource", "[resources]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::resource_cache<texture_resource> cache{&tracking};

  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.resource() == &tracking);
  CHECK(cache.index().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count > 0);
}

TEST_CASE("resources - manifest stores normalized locations and metadata", "[resources]") {
  caors::resource_manifest manifest;
  const caors::string_name texture_type{"game.texture"};

  REQUIRE(
      manifest.bind(test_guid_a, "/assets//texture.json", texture_type, "etag-a", 7).succeeded());

  CHECK(manifest.size() == 1);
  CHECK(manifest.contains(test_guid_a));

  const caors::resource_manifest_entry* entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->guid == test_guid_a);
  CHECK(entry->location.string_view() == "assets/texture.json");
  CHECK(entry->type == std::string_view{"game.texture"});
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-a");
  CHECK(entry->version == 7);
  CHECK(entry->valid());

  const caors::error duplicate = manifest.bind(test_guid_a, "other.json");
  REQUIRE(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::resources);
  CHECK(duplicate.value() == caors::errc::already_exists);

  const caors::error invalid_guid = manifest.bind(caors::resource_guid::nil(), "asset.json");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  const caors::error empty_location = manifest.bind(test_guid_b, "///");
  REQUIRE(empty_location.failed());
  CHECK(empty_location.domain() == caors::error_domain::resources);
  CHECK(empty_location.value() == caors::errc::invalid_argument);
}

TEST_CASE("resources - manifest can populate provider bindings", "[resources]") {
  caors::memory_file_system files;
  REQUIRE(files.write_all("assets/texture.json", std::string_view{"texture-bytes"}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "/assets//texture.json").succeeded());

  caors::resource_provider provider;
  REQUIRE(caors::bind_manifest(provider, source, manifest).succeeded());

  CHECK(provider.size() == 1);
  CHECK(provider.exists(test_guid_a));

  const caors::resource_source_location location = provider.find(test_guid_a);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == "assets/texture.json");

  const caors::byte_buffer_result bytes = provider.read(test_guid_a);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "texture-bytes");
}

TEST_CASE("resources - manifest loads from json document", "[resources]") {
  caors::resource_manifest manifest;
  const std::string json =
      std::string{R"({"resources":[{"guid":")"} + test_guid_a.string() +
      R"(","location":"/assets//texture.json","type":"game.texture","hash":"etag-a","version":9}]})";

  REQUIRE(caors::load_manifest_json(manifest, json).succeeded());

  const caors::resource_manifest_entry* entry = manifest.find(test_guid_a);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/texture.json");
  CHECK(entry->type == std::string_view{"game.texture"});
  CHECK(std::string_view{entry->hash.data(), entry->hash.size()} == "etag-a");
  CHECK(entry->version == 9);
}

TEST_CASE("resources - manifest loads array json from source", "[resources]") {
  const std::string json =
      std::string{R"([{"guid":")"} + test_guid_b.string() + R"(","location":"assets/mesh.json"}])";

  caors::memory_file_system files;
  REQUIRE(files.write_all("manifest.json", std::string_view{json}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_manifest manifest;

  REQUIRE(caors::load_manifest(manifest, source, "manifest.json").succeeded());

  const caors::resource_manifest_entry* entry = manifest.find(test_guid_b);
  REQUIRE(entry != nullptr);
  CHECK(entry->location.string_view() == "assets/mesh.json");
  CHECK(entry->type.empty());
  CHECK(entry->hash.empty());
  CHECK(entry->version == 0);
}

TEST_CASE("resources - manifest reports invalid json entries", "[resources]") {
  caors::resource_manifest manifest;

  caors::error invalid_guid = caors::load_manifest_json(
      manifest, R"({"resources":[{"guid":"not-a-guid","location":"asset.json"}]})");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  caors::error missing_entries = caors::load_manifest_json(manifest, R"({"items":[]})");
  REQUIRE(missing_entries.failed());
  CHECK(missing_entries.domain() == caors::error_domain::resources);
  CHECK(missing_entries.value() == caors::errc::invalid_argument);

  caors::error syntax_error = caors::load_manifest_json(manifest, R"({"resources":)");
  REQUIRE(syntax_error.failed());
  CHECK(syntax_error.domain() == caors::error_domain::serialization);
  CHECK(syntax_error.value() == caors::errc::invalid_argument);
}

TEST_CASE("resources - provider reads bytes from file source", "[resources]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "texture.txt";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{"texture-bytes"}).succeeded());

  caors::resource_file_source source{native};
  caors::resource_provider provider;

  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());
  CHECK(provider.size() == 1);
  CHECK(provider.contains(test_guid_a));
  CHECK(provider.exists(test_guid_a));

  const caors::resource_source_location location = provider.find(test_guid_a);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == file_path);

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::byte_buffer_result bytes = provider.read(test_guid_a, &tracking);

  REQUIRE(bytes.succeeded());
  CHECK(bytes.value.get_allocator().resource() == &tracking);
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "texture-bytes");

  std::filesystem::remove_all(directory, ec);
}

TEST_CASE("resources - provider reports invalid and missing resources", "[resources]") {
  caors::native_file_system native;
  caors::resource_file_source source{native};
  caors::resource_provider provider;

  caors::error invalid_guid = provider.bind(caors::resource_guid::nil(), source, "asset.txt");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  caors::error invalid_location = provider.bind(test_guid_a, source, "");
  REQUIRE(invalid_location.failed());
  CHECK(invalid_location.domain() == caors::error_domain::resources);
  CHECK(invalid_location.value() == caors::errc::invalid_argument);

  REQUIRE(provider.bind(test_guid_a, source, "missing.txt").succeeded());
  caors::error duplicate = provider.bind(test_guid_a, source, "other.txt");
  REQUIRE(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::resources);
  CHECK(duplicate.value() == caors::errc::already_exists);

  caors::byte_buffer_result missing_guid = provider.read(test_guid_b);
  REQUIRE(missing_guid.failed());
  CHECK(missing_guid.status.domain() == caors::error_domain::resources);
  CHECK(missing_guid.status.value() == caors::errc::not_found);

  caors::byte_buffer_result missing_file = provider.read(test_guid_a);
  REQUIRE(missing_file.failed());
  CHECK(missing_file.status.domain() == caors::error_domain::io);
  CHECK(missing_file.status.value() == caors::errc::not_found);
}

TEST_CASE("resources - json loader loads resource through provider", "[resources]") {
  require_serialized_resource_metadata();

  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "resource.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":512})"}).succeeded());

  caors::resource_file_source source{native};
  caors::resource_provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::resource_json_loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;

  caors::result<serialized_resource*> loaded = caors::load_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 512);
  CHECK(cache.find(test_guid_a) == loaded.value);

  std::filesystem::remove_all(directory, ec);
}

TEST_CASE("resources - json loader reports parse failures", "[resources]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "invalid.json";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{R"({"width":)"}).succeeded());

  caors::resource_file_source source{native};
  caors::resource_provider provider;
  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());

  caors::resource_json_loader<serialized_resource> loader{provider};
  caors::resource_load_result<serialized_resource> loaded = loader.load(test_guid_a);

  REQUIRE(loaded.failed());
  CHECK(loaded.status.domain() == caors::error_domain::serialization);
  CHECK(loaded.status.value() == caors::errc::invalid_argument);

  std::filesystem::remove_all(directory, ec);
}
