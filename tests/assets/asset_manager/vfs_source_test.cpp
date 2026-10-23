// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "../../resources/resources_test_common.h"
using namespace caors::tests::resources;

TEST_CASE("assets - new VFS source captures routes for default JSON loader",
          "[assets][vfs-context]") {
  require_serialized_resource_metadata();
  caors::tasks::execution_context worker, completion;
  caors::io::vfs routes;
  auto old = caors::io::make_memory_backend();
  auto next = caors::io::make_memory_backend();
  REQUIRE_FALSE(old->store("data", R"({"width":111})"));
  REQUIRE_FALSE(next->store("data", R"({"width":222})"));
  REQUIRE_FALSE(routes.mount("assets", old));
  caors::resources::vfs_source source{routes};
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  caors::assets::project_builder project{assets};
  const caors::string_name type{"new.vfs.json"};
  REQUIRE_FALSE(caors::assets::json::register_type_factory<serialized_resource>(project, type));
  caors::assets::project_config config;
  config.resource_types.push_back(
      {type, caors::string_name{"json"}, caors::string_name{"memory"}, {}});
  const caors::string_name source_name{"new.vfs.source"};
  REQUIRE_FALSE(project.bind_source(source_name, source));
  caors::assets::project_resource_config entry;
  entry.guid = test_guid_a;
  entry.source = source_name;
  entry.location = caors::io::location{"assets/data"};
  entry.type = type;
  config.resources.push_back(std::move(entry));
  REQUIRE_FALSE(project.build(config));
  REQUIRE(config.file_systems.empty());
  REQUIRE(project.provider().contains(test_guid_a));
  auto request = assets.request_load<serialized_resource>(test_guid_a);
  REQUIRE(routes.unmount("assets"));
  REQUIRE_FALSE(routes.mount("assets", next));
  for (int i = 0; i < 32 && !request.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(request.loaded());
  CHECK(request.try_get()->width == 111);
  auto reload = assets.request_reload<serialized_resource>(test_guid_a);
  for (int i = 0; i < 32 && !reload.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(reload.loaded());
  CHECK(reload.try_get()->width == 222);
}

TEST_CASE("assets - source snapshot retains exact backend and pin without legacy transport",
          "[assets][vfs-context]") {
  std::weak_ptr<caors::io::backend> backend_lifetime;
  std::weak_ptr<int> host_lifetime;
  std::shared_ptr<const caors::resources::provider_snapshot> captured;
  caors::io::backend* original = nullptr;
  {
    caors::io::vfs routes;
    auto backend = caors::io::make_memory_backend();
    original = backend.get();
    backend_lifetime = backend;
    auto pin = std::make_shared<int>(42);
    host_lifetime = pin;
    REQUIRE_FALSE(backend->store("data", "owned"));
    REQUIRE_FALSE(routes.mount("assets", backend, {}, pin));
    caors::resources::vfs_source source{routes};
    caors::resources::provider provider;
    REQUIRE_FALSE(provider.bind(test_guid_a, source, "assets/data"));
    REQUIRE_FALSE(provider.bind(test_guid_b, source, "assets/data"));
    captured = provider.snapshot();
    auto first = captured->resolve(test_guid_a);
    auto second = captured->resolve(test_guid_b);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->backend.get() == original);
    CHECK(second->backend.get() == original);
    CHECK(first->source_id == second->source_id);
    CHECK(first->path.string_view() == "data");
    routes.clear();
    provider.clear();
  }
  CHECK_FALSE(backend_lifetime.expired());
  CHECK_FALSE(host_lifetime.expired());
  auto location = captured->resolve(test_guid_a);
  REQUIRE(location);
  captured.reset();
  auto bytes = read_snapshot_bytes(location.value);
  REQUIRE(bytes);
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes->data()), bytes->size()} == "owned");
  location.value = {};
  CHECK(backend_lifetime.expired());
  CHECK(host_lifetime.expired());
}
TEST_CASE("assets - provider resolves captured paths once and preserves per GUID errors",
          "[assets][vfs-context]") {
  class counted_source final : public caors::resources::source {
  public:
    std::shared_ptr<int> captures = std::make_shared<int>(0);
    std::shared_ptr<int> resolutions = std::make_shared<int>(0);
    std::shared_ptr<caors::io::memory_backend> backend = caors::io::make_memory_backend();
    bool malformed = false;
    bool exists(caors::io::location_view) const override { return false; }
    caors::io::byte_buffer_result read(caors::io::location_view, caors::memory_resource*) override {
      return {{caors::error_domain::resources, caors::errc::unsupported}, {}};
    }
    caors::result<caors::resources::captured_source> capture() const override {
      ++*captures;
      return {{},
              [backend = backend, count = resolutions, malformed = malformed](
                  caors::io::location_view path) -> caors::result<caors::io::backend_location> {
                ++*count;
                if (malformed)
                  return {{}, {}};
                if (path.string_view() == "missing")
                  return {{caors::error_domain::io, caors::errc::not_found}, {}};
                return {{}, {backend, caors::io::path{path.string_view()}, 123}};
              }};
    }
  } source;
  caors::resources::provider provider;
  REQUIRE_FALSE(provider.bind(test_guid_a, source, "data"));
  REQUIRE_FALSE(provider.bind(test_guid_b, source, "missing"));
  auto captured = provider.snapshot();
  CHECK(*source.captures == 1);
  CHECK(*source.resolutions == 2);
  CHECK(captured->resolve(test_guid_a));
  CHECK(captured->resolve(test_guid_b).error().value() == caors::errc::not_found);
  CHECK(captured->resolve(test_guid_a));
  CHECK(*source.resolutions == 2);
  source.malformed = true;
  auto malformed = provider.snapshot();
  CHECK(malformed->resolve(test_guid_a).error().value() == caors::errc::backend_failure);
  CHECK(captured->resolve(test_guid_a));
}
