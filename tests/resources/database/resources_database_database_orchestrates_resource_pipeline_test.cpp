// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

#include <algorithm>

using namespace caors::tests::resources;

namespace {

constexpr caors::resource_guid test_guid_c{caors::guid::from_bytes(
    caors::guid::byte_array{0x9d, 0xe1, 0x08, 0x75, 0xa1, 0x63, 0x4b, 0x8d, 0x9c, 0x28, 0x5b, 0x28,
                            0x57, 0x49, 0x86, 0x92})};

struct database_resource_ref_holder {
  caors::resource_ref<serialized_resource> resource{};
};

[[nodiscard]] bool contains_guid(const caors::vector<caors::resource_guid>& values,
                                 caors::resource_guid guid) {
  return std::find(values.begin(), values.end(), guid) != values.end();
}

[[nodiscard]] bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

void require_texture_resource_type() {
  REQUIRE(type_result_ok(
      caors::meta::type_<texture_resource>("caors.test.resources.texture_resource").result()));
}

void require_database_resource_ref_metadata() {
  require_serialized_resource_metadata();

  REQUIRE(type_result_ok(caors::meta::type_<caors::resource_ref<serialized_resource>>(
                             "caors.resource_ref<caors.test.resources.serialized_resource>")
                             .result()));
  REQUIRE(serialization_result_ok(
      caors::serialization::register_codec<caors::resource_ref<serialized_resource>>()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<database_resource_ref_holder>(
                                   "caors.test.resources.database_resource_ref_holder")
                                   .field("resource", &database_resource_ref_holder::resource)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<database_resource_ref_holder>()));
}

} // namespace

TEST_CASE("resources - database orchestrates manifest provider loader cache and resolver",
          "[resources]") {
  require_database_resource_ref_metadata();

  caors::memory_file_system files;
  REQUIRE(
      files.write_all("assets/resource.json", std::string_view{R"({"width":512})"}).succeeded());

  caors::resource_file_source source{files};
  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/resource.json").succeeded());

  caors::resource_provider provider;
  caors::resource_dependency_graph graph;
  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_provider(provider);
  database.set_dependency_graph(graph);

  REQUIRE(database.sync_manifest(source).succeeded());
  CHECK(provider.contains(test_guid_a));

  caors::resource_json_loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;
  REQUIRE(database.register_type(cache, loader).succeeded());

  caors::result<serialized_resource*> loaded = database.load<serialized_resource>(test_guid_a);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 512);
  CHECK(database.find<serialized_resource>(test_guid_a) == loaded.value);

  caors::resource_handle_result<serialized_resource> loaded_again =
      database.load_handle<serialized_resource>(test_guid_a);
  REQUIRE(loaded_again.succeeded());
  CHECK(loaded_again.value.get() == loaded.value);

  caors::serialization::resource_dependency_list dependencies;
  caors::serialization::resource_dependency_collector collector = dependencies.collector();
  caors::serialization::context dependency_context{};
  dependency_context.resources.dependencies = &collector;

  const database_resource_ref_holder holder{
      caors::resource_ref<serialized_resource>{test_guid_a},
  };
  const caors::serialization::serialization_text_result encoded =
      caors::serialize_json(dependency_context, holder);

  REQUIRE(encoded.succeeded());
  REQUIRE(dependencies.size() == 1);
  CHECK(dependencies.entries()[0].guid == test_guid_a);
  CHECK(database.replace_dependencies(test_guid_b, dependencies) == 1);
  CHECK(graph.contains(test_guid_b, test_guid_a));

  const caors::serialization::resource_resolver resolver = database.resolver();
  caors::serialization::context context{};
  context.resources.resolver = &resolver;
  context.resources.resolution = caors::serialization::resource_ref_resolution::require_resolved;

  database_resource_ref_holder decoded{};
  const caors::error status = caors::deserialize_json(
      context, decoded, R"({"resource":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(status.succeeded());
  CHECK(decoded.resource.guid() == test_guid_a);
  CHECK(decoded.resource.resolved());
  CHECK(decoded.resource.get() == loaded.value);

  REQUIRE(database.resolve(decoded.resource));
  CHECK(database.is_current(decoded.resource));

  REQUIRE(files.write_all("assets/resource-v2.json", std::string_view{R"({"width":1024})"})
              .succeeded());
  REQUIRE(manifest.set(test_guid_a, "assets/resource-v2.json").succeeded());
  REQUIRE(database.sync_manifest(source).succeeded());

  caors::result<serialized_resource*> reloaded = database.reload<serialized_resource>(test_guid_a);

  REQUIRE(reloaded.succeeded());
  REQUIRE(reloaded.value != nullptr);
  CHECK(reloaded.value->width == 1024);
  CHECK(database.find<serialized_resource>(test_guid_a) == reloaded.value);

  caors::vector<caors::resource_guid> affected = database.affected_by(test_guid_a);
  REQUIRE(affected.size() == 1);
  CHECK(affected[0] == test_guid_b);

  CHECK_FALSE(database.is_current(decoded.resource));
  REQUIRE(database.resolve(decoded.resource));
  CHECK(decoded.resource.get() == reloaded.value);
  CHECK(database.is_current(decoded.resource));

  REQUIRE(manifest.set(test_guid_a, "assets/missing.json").succeeded());
  REQUIRE(database.sync_manifest(source).succeeded());

  caors::result<serialized_resource*> failed_reload =
      database.reload<serialized_resource>(test_guid_a);

  CHECK(failed_reload.failed());
  CHECK(database.find<serialized_resource>(test_guid_a) == reloaded.value);
  CHECK(database.find<serialized_resource>(test_guid_a)->width == 1024);
}

TEST_CASE("resources - database plans and preloads dependency closure", "[resources]") {
  require_texture_resource_type();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/root.json").succeeded());
  REQUIRE(manifest.bind(test_guid_b, "assets/dependency.json").succeeded());
  REQUIRE(manifest.bind(test_guid_c, "assets/leaf.json").succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));
  REQUIRE(graph.add(test_guid_b, test_guid_c));

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;
  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(database.register_type(cache, loader).succeeded());

  caors::resource_preload_plan plan = database.plan_preload<texture_resource>(test_guid_a);
  REQUIRE(plan.succeeded());
  CHECK(plan.cycle.empty());
  CHECK(plan.missing_manifest.empty());
  REQUIRE(plan.load_order.size() == 3);
  CHECK(plan.load_order[0] == test_guid_c);
  CHECK(plan.load_order[1] == test_guid_b);
  CHECK(plan.load_order[2] == test_guid_a);

  caors::resource_preload_result loaded = database.preload<texture_resource>(test_guid_a);
  REQUIRE(loaded.succeeded());
  CHECK(loaded.loaded.size() == 3);
  CHECK(loader.load_count == 3);
  CHECK(cache.contains(test_guid_a));
  CHECK(cache.contains(test_guid_b));
  CHECK(cache.contains(test_guid_c));

  caors::resource_preload_result loaded_again = database.preload<texture_resource>(test_guid_a);
  REQUIRE(loaded_again.succeeded());
  CHECK(loaded_again.loaded.empty());
  CHECK(loaded_again.plan.already_loaded.size() == 3);
  CHECK(loader.load_count == 3);
}

TEST_CASE("resources - database preload reports missing manifest entries", "[resources]") {
  require_texture_resource_type();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/root.json").succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;
  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(database.register_type(cache, loader).succeeded());

  caors::resource_preload_result loaded = database.preload<texture_resource>(test_guid_a);
  CHECK(loaded.status == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(contains_guid(loaded.plan.missing_manifest, test_guid_b));
  CHECK(loader.load_count == 0);
  CHECK(cache.empty());
}

TEST_CASE("resources - database preload rejects dependency cycles", "[resources]") {
  require_texture_resource_type();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/root.json").succeeded());
  REQUIRE(manifest.bind(test_guid_b, "assets/dependency.json").succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));
  REQUIRE(graph.add(test_guid_b, test_guid_a));

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;
  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(database.register_type(cache, loader).succeeded());

  caors::resource_preload_result loaded = database.preload<texture_resource>(test_guid_a);
  CHECK(loaded.status == caors::error{caors::error_domain::resources, caors::errc::cycle_detected});
  CHECK(loaded.plan.cycle.size() == 3);
  CHECK(loaded.plan.cycle.front() == loaded.plan.cycle.back());
  CHECK(contains_guid(loaded.plan.cycle, test_guid_a));
  CHECK(contains_guid(loaded.plan.cycle, test_guid_b));
  CHECK(loader.load_count == 0);
  CHECK(cache.empty());
}
