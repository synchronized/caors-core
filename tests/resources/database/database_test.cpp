// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

#include <algorithm>

using namespace caors::tests::resources;

namespace {

constexpr caors::resource_guid test_guid_c{caors::guid::from_bytes(
    caors::guid::byte_array{0x9d, 0xe1, 0x08, 0x75, 0xa1, 0x63, 0x4b, 0x8d, 0x9c, 0x28, 0x5b, 0x28,
                            0x57, 0x49, 0x86, 0x92})};

constexpr caors::type_guid texture_stable_type_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0xc5, 0x19, 0xf3, 0xa7, 0x2d, 0x9a, 0x45, 0x24, 0x84, 0x7e, 0x92, 0x2e,
                            0x98, 0x53, 0x8c, 0x11})};

[[nodiscard]] caors::string_name texture_manifest_type() {
  return caors::string_name{"caors.test.resources.texture"};
}

[[nodiscard]] caors::string_name material_manifest_type() {
  return caors::string_name{"caors.test.resources.material"};
}

[[nodiscard]] caors::string_name missing_manifest_type() {
  return caors::string_name{"caors.test.resources.missing"};
}

class material_resource final : public caors::resource {
public:
  using caors::resource::resource;
};

class counting_material_loader final
    : public caors::resources::sync_loader_adapter<material_resource> {
public:
  [[nodiscard]] caors::resources::load_result<material_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    ++load_count;
    auto loaded = caors::memory::make_unique<material_resource>(resource, guid);
    return {{}, std::move(loaded)};
  }

  int load_count = 0;
};

struct database_resource_ref_holder {
  caors::resource_ref<serialized_resource> resource{};
};

[[nodiscard]] bool contains_guid(const caors::vector<caors::resource_guid>& values,
                                 caors::resource_guid guid) {
  return std::find(values.begin(), values.end(), guid) != values.end();
}

[[nodiscard]] bool contains_item(const caors::resources::preload_plan::item_list& values,
                                 caors::resource_guid guid, caors::string_name type) {
  return std::find(values.begin(), values.end(), caors::resources::preload_item{guid, type}) !=
         values.end();
}

[[nodiscard]] bool contains_failure(const caors::resources::preload_result::failure_list& values,
                                    caors::resource_guid guid, caors::string_name type,
                                    caors::error status) {
  return std::find(values.begin(), values.end(),
                   caors::resources::preload_failure{
                       caors::resources::preload_item{guid, type},
                       status,
                   }) != values.end();
}

[[nodiscard]] bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

void require_texture_resource_type() {
  REQUIRE(type_result_ok(
      caors::registration::type_<texture_resource>("caors.test.resources.texture_resource")
          .commit()));
}

void require_material_resource_type() {
  REQUIRE(type_result_ok(
      caors::registration::type_<material_resource>("caors.test.resources.material_resource")
          .commit()));
}

void require_database_resource_ref_metadata() {
  require_serialized_resource_metadata();

  REQUIRE(type_result_ok(caors::registration::type_<caors::resource_ref<serialized_resource>>(
                             "caors.resource_ref<caors.test.resources.serialized_resource>")
                             .commit()));
  REQUIRE(serialization_result_ok(caors::registration::type_<database_resource_ref_holder>(
                                      "caors.test.resources.database_resource_ref_holder")
                                      .meta()
                                      .property("resource", &database_resource_ref_holder::resource)
                                      .serialization()
                                      .commit()));
}

} // namespace

TEST_CASE("resources - database orchestrates manifest provider loader cache and resolver",
          "[resources]") {
  require_database_resource_ref_metadata();

  caors::io::memory_file_system files;
  REQUIRE(
      files.write_all("assets/resource.json", std::string_view{R"({"width":512})"}).succeeded());

  caors::resources::file_source source{files};
  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/resource.json").succeeded());

  caors::resources::provider provider;
  caors::resource_dependency_graph graph;
  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);

  REQUIRE(caors::resources::sync_manifest(provider, source, manifest).succeeded());
  CHECK(provider.contains(test_guid_a));

  caors::assets::json::loader<serialized_resource> loader{provider};
  caors::resource_cache<serialized_resource> cache;
  REQUIRE(database.register_type(cache, loader).succeeded());

  caors::result<serialized_resource*> loaded = database.load<serialized_resource>(test_guid_a);
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->guid() == test_guid_a);
  CHECK(loaded.value->width == 512);
  CHECK(database.find<serialized_resource>(test_guid_a) == loaded.value);

  caors::resources::handle_result<serialized_resource> loaded_again =
      database.load_handle<serialized_resource>(test_guid_a);
  REQUIRE(loaded_again.succeeded());
  CHECK(loaded_again.value.get() == loaded.value);

  caors::resources::resource_dependency_list dependencies;
  caors::resources::resource_dependency_collector_ops collector =
      caors::resources::make_resource_dependency_collector(dependencies);
  caors::serialization::context dependency_context{};
  dependency_context.resolvers.external_refs.collector = &collector;

  const database_resource_ref_holder holder{
      caors::resource_ref<serialized_resource>{test_guid_a},
  };
  const caors::serialization::serialization_text_result encoded =
      caors::to_json(dependency_context, holder);

  REQUIRE(encoded.succeeded());
  REQUIRE(dependencies.size() == 1);
  CHECK(dependencies.entries()[0].guid == test_guid_a);
  CHECK(database.replace_dependencies(test_guid_b, dependencies) == 1);
  CHECK(graph.contains(test_guid_b, test_guid_a));

  const caors::resources::resource_ref_resolver_ops resolver =
      caors::resources::make_resource_resolver(database);
  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;
  context.resolvers.external_refs.resolution =
      caors::resources::resource_ref_resolution::require_resolved;

  database_resource_ref_holder decoded{};
  const caors::error status = caors::read_json_from(
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
  REQUIRE(caors::resources::sync_manifest(provider, source, manifest).succeeded());

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
  REQUIRE(caors::resources::sync_manifest(provider, source, manifest).succeeded());

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

  caors::resources::preload_plan plan = database.plan_preload<texture_resource>(test_guid_a);
  REQUIRE(plan.succeeded());
  CHECK(plan.cycle.empty());
  CHECK(plan.missing_manifest.empty());
  REQUIRE(plan.load_order.size() == 3);
  CHECK(plan.load_order[0].guid == test_guid_c);
  CHECK(plan.load_order[1].guid == test_guid_b);
  CHECK(plan.load_order[2].guid == test_guid_a);

  caors::resources::preload_result loaded = database.preload<texture_resource>(test_guid_a);
  REQUIRE(loaded.succeeded());
  CHECK(loaded.loaded.size() == 3);
  CHECK(loader.load_count == 3);
  CHECK(cache.contains(test_guid_a));
  CHECK(cache.contains(test_guid_b));
  CHECK(cache.contains(test_guid_c));

  caors::resources::preload_result loaded_again = database.preload<texture_resource>(test_guid_a);
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

  caors::resources::preload_result loaded = database.preload<texture_resource>(test_guid_a);
  CHECK(loaded.status == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(contains_guid(loaded.plan.missing_manifest, test_guid_b));
  CHECK(contains_failure(loaded.failed_items, test_guid_b, {},
                         {caors::error_domain::resources, caors::errc::not_found}));
  CHECK(loader.load_count == 0);
  CHECK(cache.empty());
}

TEST_CASE("resources - database preload dispatches by manifest type", "[resources]") {
  require_texture_resource_type();
  require_material_resource_type();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/material.json", material_manifest_type()).succeeded());
  REQUIRE(manifest.bind(test_guid_b, "assets/texture.json", texture_manifest_type()).succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));

  caors::resource_cache<texture_resource> texture_cache;
  counting_texture_loader texture_loader;
  caors::resource_cache<material_resource> material_cache;
  counting_material_loader material_loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(
      database.register_type(texture_cache, texture_loader, texture_manifest_type()).succeeded());
  REQUIRE(database.register_type(material_cache, material_loader, material_manifest_type())
              .succeeded());

  caors::resources::preload_plan plan = database.plan_preload(test_guid_a);
  REQUIRE(plan.succeeded());
  REQUIRE(plan.load_order.size() == 2);
  CHECK(plan.load_order[0] == caors::resources::preload_item{test_guid_b, texture_manifest_type()});
  CHECK(plan.load_order[1] ==
        caors::resources::preload_item{test_guid_a, material_manifest_type()});

  caors::resources::preload_result loaded = database.preload(test_guid_a);
  REQUIRE(loaded.succeeded());
  CHECK(loaded.loaded.size() == 2);
  CHECK(texture_loader.load_count == 1);
  CHECK(material_loader.load_count == 1);
  CHECK(texture_cache.contains(test_guid_b));
  CHECK(material_cache.contains(test_guid_a));

  caors::resources::preload_result loaded_again = database.preload(test_guid_a);
  REQUIRE(loaded_again.succeeded());
  CHECK(loaded_again.loaded.empty());
  CHECK(contains_item(loaded_again.plan.already_loaded, test_guid_b, texture_manifest_type()));
  CHECK(contains_item(loaded_again.plan.already_loaded, test_guid_a, material_manifest_type()));
  CHECK(texture_loader.load_count == 1);
  CHECK(material_loader.load_count == 1);
}

TEST_CASE("resources - database preload dispatches by external manifest type alias",
          "[resources]") {
  require_texture_resource_type();

  const caors::string_name external_texture_type{"external.texture"};

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/texture.json", external_texture_type).succeeded());

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  REQUIRE(database.register_type(cache, loader, texture_manifest_type()).succeeded());
  REQUIRE(database.bind_resource_type_alias(external_texture_type, texture_manifest_type())
              .succeeded());

  caors::resources::preload_plan plan = database.plan_preload(test_guid_a);
  REQUIRE(plan.succeeded());
  REQUIRE(plan.load_order.size() == 1);
  CHECK(plan.load_order[0] == caors::resources::preload_item{test_guid_a, external_texture_type});

  caors::resources::preload_result loaded = database.preload(test_guid_a);
  REQUIRE(loaded.succeeded());
  CHECK(loader.load_count == 1);
  CHECK(cache.contains(test_guid_a));
}

TEST_CASE("resources - database preload dispatches by manifest type guid token", "[resources]") {
  require_texture_resource_type();

  const caors::string_name type_guid_token{texture_stable_type_guid.value.string()};

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/texture.json", type_guid_token).succeeded());

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  REQUIRE(database.register_type(cache, loader, texture_stable_type_guid).succeeded());

  caors::resources::preload_plan plan = database.plan_preload(test_guid_a);
  REQUIRE(plan.succeeded());
  REQUIRE(plan.load_order.size() == 1);
  CHECK(plan.load_order[0] == caors::resources::preload_item{test_guid_a, type_guid_token});

  caors::resources::preload_result loaded = database.preload(test_guid_a);
  REQUIRE(loaded.succeeded());
  CHECK(loader.load_count == 1);
  CHECK(cache.contains(test_guid_a));
}

TEST_CASE("resources - database dispatched preload reports missing manifest type", "[resources]") {
  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/root.json").succeeded());

  caors::resource_database database;
  database.set_manifest(manifest);

  caors::resources::preload_result loaded = database.preload(test_guid_a);
  CHECK(loaded.status == caors::error{caors::error_domain::resources, caors::errc::missing_type});
  CHECK(contains_guid(loaded.plan.missing_type, test_guid_a));
  CHECK(loaded.loaded.empty());
}

TEST_CASE("resources - database dispatched preload reports missing cache and loader",
          "[resources]") {
  require_texture_resource_type();
  require_material_resource_type();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "assets/material.json", material_manifest_type()).succeeded());
  REQUIRE(manifest.bind(test_guid_b, "assets/texture.json", texture_manifest_type()).succeeded());
  REQUIRE(manifest.bind(test_guid_c, "assets/missing.json", missing_manifest_type()).succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));
  REQUIRE(graph.add(test_guid_a, test_guid_c));

  caors::resource_cache<texture_resource> texture_cache;
  counting_material_loader material_loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(database.register_cache(texture_cache, texture_manifest_type()).succeeded());
  REQUIRE(database.register_loader(material_loader, material_manifest_type()).succeeded());

  caors::resources::preload_result loaded = database.preload(test_guid_a);
  CHECK(loaded.status == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(contains_item(loaded.plan.missing_loader, test_guid_b, texture_manifest_type()));
  CHECK(contains_item(loaded.plan.missing_cache, test_guid_a, material_manifest_type()));
  CHECK(contains_item(loaded.plan.missing_cache, test_guid_c, missing_manifest_type()));
  CHECK(contains_item(loaded.plan.missing_loader, test_guid_c, missing_manifest_type()));
  CHECK(contains_failure(loaded.failed_items, test_guid_b, texture_manifest_type(),
                         {caors::error_domain::resources, caors::errc::not_found}));
  CHECK(contains_failure(loaded.failed_items, test_guid_a, material_manifest_type(),
                         {caors::error_domain::resources, caors::errc::not_found}));
  CHECK(contains_failure(loaded.failed_items, test_guid_c, missing_manifest_type(),
                         {caors::error_domain::resources, caors::errc::not_found}));
  CHECK(texture_cache.empty());
  CHECK(material_loader.load_count == 0);
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

  caors::resources::preload_result loaded = database.preload<texture_resource>(test_guid_a);
  CHECK(loaded.status == caors::error{caors::error_domain::resources, caors::errc::cycle_detected});
  CHECK(loaded.plan.cycle.size() == 3);
  CHECK(loaded.plan.cycle.front() == loaded.plan.cycle.back());
  CHECK(contains_guid(loaded.plan.cycle, test_guid_a));
  CHECK(contains_guid(loaded.plan.cycle, test_guid_b));
  CHECK(loader.load_count == 0);
  CHECK(cache.empty());
}
