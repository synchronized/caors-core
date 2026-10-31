// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "../../resources/resources_test_common.h"
using namespace caors::tests::resources;

TEST_CASE("assets - new VFS source captures routes for default JSON loader",
          "[assets][vfs-context]") {
  require_serialized_resource_metadata();
  caors::tasks::execution_context worker, completion;
  caors::io::file_service routes;
  auto old = caors::io::make_memory_backend();
  auto next = caors::io::make_memory_backend();
  REQUIRE_FALSE(old->store("data", R"({"width":111})"));
  REQUIRE_FALSE(next->store("data", R"({"width":222})"));
  REQUIRE_FALSE(routes.mount("assets", old));
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  caors::assets::project_builder project{assets};
  const caors::string_name type{"new.vfs.json"};
  REQUIRE_FALSE(caors::assets::json::register_type_factory<serialized_resource>(project, type));
  caors::assets::project_config config;
  config.resource_types.push_back(
      {type, caors::string_name{"json"}, caors::string_name{"memory"}, {}});
  const caors::string_name source_name{"new.vfs.source"};
  REQUIRE_FALSE(project.bind_source(source_name, routes));
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
  REQUIRE(project.unbind_source(source_name));
  CHECK_FALSE(project.provider().contains(test_guid_a));
  REQUIRE(routes.unmount("assets"));
  REQUIRE_FALSE(routes.mount("assets", next));
  for (int i = 0; i < 32 && !request.done(); ++i) {
    (void)worker.drain();
    (void)completion.drain();
  }
  REQUIRE(request.loaded());
  CHECK(request.try_get()->width == 111);
  REQUIRE_FALSE(project.bind_source(source_name, routes));
  REQUIRE_FALSE(
      project.provider().bind(test_guid_a, *project.find_source(source_name), "assets/data"));
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
    caors::io::file_service routes;
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

TEST_CASE("assets - service binding unlinks owned source without invalidating captured requests",
          "[assets][service-binding]") {
  caors::io::file_service files;
  auto backend = caors::io::make_memory_backend();
  REQUIRE_FALSE(backend->store("data", "owned"));
  REQUIRE_FALSE(files.mount("assets", backend));
  caors::io::memory_file_system borrowed_files;
  caors::resources::file_source borrowed{borrowed_files};
  caors::assets::asset_manager manager;
  caors::assets::project_builder project{manager};
  REQUIRE_FALSE(project.bind_source(caors::string_name{"borrowed"}, borrowed));
  REQUIRE_FALSE(project.provider().bind(test_guid_b, borrowed, "data"));
  const caors::string_name name{"service.binding"}, alias{"service.alias"};
  CHECK(project.bind_source({}, files).value() == caors::errc::invalid_argument);
  REQUIRE_FALSE(project.bind_source(name, files));
  auto* source = project.find_source(name);
  REQUIRE(source);
  CHECK(project.bind_source(name, files).value() == caors::errc::already_exists);
  REQUIRE_FALSE(project.bind_source(alias, *source));
  REQUIRE_FALSE(project.provider().bind(test_guid_a, *source, "assets/data"));
  auto captured = project.provider().snapshot();
  REQUIRE(project.unbind_source(name));
  CHECK(project.provider().contains(test_guid_a));
  CHECK(project.find_source(alias) == source);
  REQUIRE(project.unbind_source(alias));
  CHECK_FALSE(project.provider().contains(test_guid_a));
  CHECK(project.provider().contains(test_guid_b));
  CHECK_FALSE(project.unbind_source(alias));
  auto location = captured->resolve(test_guid_a);
  REQUIRE(location);
  REQUIRE(read_snapshot_bytes(location.value));
  REQUIRE_FALSE(project.bind_source(name, files));
  REQUIRE_FALSE(project.provider().bind(test_guid_a, *project.find_source(name), "assets/data"));
  project.clear_sources();
  CHECK(project.source_count() == 0);
  CHECK_FALSE(project.provider().contains(test_guid_a));
  CHECK(project.provider().contains(test_guid_b));
}
TEST_CASE("assets - service adapters release on repeated unbind and survive config rebuild",
          "[assets][service-binding]") {
  caors::tracking_resource memory;
  caors::io::file_service files;
  caors::assets::asset_manager manager{&memory};
  caors::assets::project_builder project{manager};
  const caors::string_name name{"service.release"};
  REQUIRE_FALSE(project.bind_source(name, files));
  caors::assets::project_config config;
  REQUIRE_FALSE(project.build(config));
  CHECK(project.find_source(name));
  REQUIRE(project.unbind_source(name));
  const auto baseline = memory.statistics().bytes_in_use;
  for (int i = 0; i < 32; ++i) {
    REQUIRE_FALSE(project.bind_source(name, files));
    CHECK(memory.statistics().bytes_in_use > baseline);
    REQUIRE(project.unbind_source(name));
    CHECK(memory.statistics().bytes_in_use == baseline);
  }
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("assets - service binding allocation failure leaves no retained adapter",
          "[assets][service-binding][memory]") {
  class failing_memory final : public caors::memory_resource {
  public:
    int remaining = -1;
    std::size_t live = 0;

  private:
    void* do_try_allocate(std::size_t bytes, std::size_t alignment) noexcept override {
      if (remaining >= 0 && remaining-- == 0)
        return nullptr;
      auto* result = caors::get_system_memory_resource().try_allocate(bytes, alignment);
      if (result)
        live += bytes;
      return result;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept override {
      live -= bytes;
      caors::get_system_memory_resource().deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const caors::memory_resource& other) const noexcept override {
      return this == &other;
    }
  } memory;
  caors::io::file_service files;
  {
    caors::assets::asset_manager manager{&memory};
    caors::assets::project_builder project{manager};
    const caors::string_name name{"service.failure"};
    REQUIRE_FALSE(project.bind_source(name, files));
    REQUIRE(project.unbind_source(name));
    const auto baseline = memory.live;
    for (int failure = 0; failure < 4; ++failure) {
      memory.remaining = failure;
      try {
        auto result = project.bind_source(name, files);
        REQUIRE_FALSE(result);
      } catch (const std::bad_alloc&) {
        CHECK(project.source_count() == 0);
        CHECK_FALSE(project.find_source(name));
      }
      memory.remaining = -1;
      if (project.find_source(name))
        REQUIRE(project.unbind_source(name));
      CHECK(memory.live == baseline);
    }
  }
  CHECK(memory.live == 0);
  // 冷表还可能在首次插入时分配桶，覆盖该失败点与完整销毁。
  for (int failure = 0; failure < 4; ++failure) {
    {
      caors::assets::asset_manager manager{&memory};
      caors::assets::project_builder project{manager};
      memory.remaining = failure;
      try {
        auto result = project.bind_source(caors::string_name{"service.cold"}, files);
        REQUIRE_FALSE(result);
      } catch (const std::bad_alloc&) {
        CHECK(project.source_count() == 0);
      }
      memory.remaining = -1;
    }
    CHECK(memory.live == 0);
  }
}
#endif

namespace {
template <class Builder>
concept accepts_temporary_service = requires(Builder project) {
  project.bind_source(caors::string_name{}, caors::io::file_service{});
};
static_assert(!accepts_temporary_service<caors::assets::project_builder>);
}
