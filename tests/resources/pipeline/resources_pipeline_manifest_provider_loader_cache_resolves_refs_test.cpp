// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

namespace {

struct serialized_resource_ref_holder {
  caors::resource_ref<serialized_resource> resource{};
};

[[nodiscard]] bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

void require_serialized_resource_ref_holder_metadata() {
  require_serialized_resource_metadata();

  REQUIRE(type_result_ok(caors::meta::type_<caors::resource_ref<serialized_resource>>(
                             "caors.resource_ref<caors.test.resources.serialized_resource>")
                             .result()));
  REQUIRE(serialization_result_ok(
      caors::serialization::register_codec<caors::resource_ref<serialized_resource>>()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<serialized_resource_ref_holder>(
                                   "caors.test.resources.serialized_resource_ref_holder")
                                   .field("resource", &serialized_resource_ref_holder::resource)
                                   .commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::register_type<serialized_resource_ref_holder>()));
}

} // namespace

TEST_CASE("resources - manifest provider loader cache can resolve serialized resource refs",
          "[resources]") {
  require_serialized_resource_ref_holder_metadata();

  caors::memory_file_system files;
  REQUIRE(
      files.write_all("assets/resource.json", std::string_view{R"({"width":512})"}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/resource.json").succeeded());

  caors::resource_provider provider;
  REQUIRE(caors::bind_manifest(provider, source, manifest).succeeded());

  caors::resource_json_loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;

  caors::serialization::resource_dependency_list dependencies;
  caors::serialization::resource_dependency_collector collector = dependencies.collector();
  caors::serialization::context dependency_context{};
  dependency_context.resources.dependencies = &collector;

  const serialized_resource_ref_holder holder{
      caors::resource_ref<serialized_resource>{test_guid_a},
  };
  const caors::serialization::serialization_text_result encoded =
      caors::serialize_json(holder, dependency_context);

  REQUIRE(encoded.succeeded());
  REQUIRE(dependencies.size() == 1);
  CHECK(dependencies.entries()[0].guid == test_guid_a);
  CHECK(dependencies.entries()[0].type == caors::meta::id_of<serialized_resource>());

  caors::result<serialized_resource*> loaded =
      caors::load_into(cache, loader, dependencies.entries()[0].guid);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->width == 512);

  const caors::serialization::resource_resolver resolver =
      caors::serialization::make_resource_resolver(cache);
  caors::serialization::context context{};
  context.resources.resolver = &resolver;
  context.resources.resolution = caors::serialization::resource_ref_resolution::require_resolved;

  serialized_resource_ref_holder decoded{};
  const caors::error status = caors::deserialize_json(
      decoded, R"({"resource":"6cd2da3d-b549-4756-8b8b-0c84293af410"})", context);

  REQUIRE(status.succeeded());
  CHECK(decoded.resource.guid() == test_guid_a);
  CHECK(decoded.resource.resolved());
  CHECK(decoded.resource.get() == loaded.value);

  REQUIRE(files.write_all("assets/resource-v2.json", std::string_view{R"({"width":1024})"})
              .succeeded());
  REQUIRE(manifest.set(test_guid_a, "assets/resource-v2.json").succeeded());
  REQUIRE(caors::sync_manifest(provider, source, manifest).succeeded());

  caors::result<serialized_resource*> reloaded = caors::reload_into(cache, loader, test_guid_a);

  REQUIRE(reloaded.succeeded());
  REQUIRE(reloaded.value != nullptr);
  CHECK(reloaded.value->width == 1024);
  CHECK(cache.find(test_guid_a) == reloaded.value);

  REQUIRE(cache.resolve(decoded.resource));
  CHECK(decoded.resource.get() == reloaded.value);

  REQUIRE(manifest.set(test_guid_a, "assets/missing.json").succeeded());
  REQUIRE(caors::sync_manifest(provider, source, manifest).succeeded());

  caors::result<serialized_resource*> failed_reload =
      caors::reload_into(cache, loader, test_guid_a);

  CHECK(failed_reload.failed());
  CHECK(cache.find(test_guid_a) == reloaded.value);
  CHECK(cache.find(test_guid_a)->width == 1024);
}
