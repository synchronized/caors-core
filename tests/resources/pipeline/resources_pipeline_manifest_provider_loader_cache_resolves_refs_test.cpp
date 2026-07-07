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
  caors::result<serialized_resource*> loaded = caors::load_into(cache, loader, test_guid_a);

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
}
