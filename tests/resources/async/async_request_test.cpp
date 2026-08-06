// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

#include "caors/tasks.h"

#include <algorithm>
#include <array>
#include <atomic>

using namespace caors::tests::resources;

namespace {

class async_resource final : public caors::resource {
public:
  using caors::resource::resource;

  int value = 0;
};

class async_loader final : public caors::resources::loader<async_resource> {
public:
  [[nodiscard]] caors::resources::load_result<async_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    load_count.fetch_add(1);
    if (fail_next) {
      fail_next = false;
      return {{caors::error_domain::resources, caors::errc::not_found}, {}};
    }

    auto loaded = caors::memory::make_unique<async_resource>(resource, guid);
    loaded->value = next_value;
    return {{}, std::move(loaded)};
  }

  std::atomic<int> load_count{0};
  int next_value = 0;
  bool fail_next = false;
};

class async_material_resource final : public caors::resource {
public:
  using caors::resource::resource;
};

class async_material_loader final : public caors::resources::loader<async_material_resource> {
public:
  [[nodiscard]] caors::resources::load_result<async_material_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    ++load_count;
    if (status.failed()) {
      return {status, {}};
    }

    auto loaded = caors::memory::make_unique<async_material_resource>(resource, guid);
    return {{}, std::move(loaded)};
  }

  int load_count = 0;
  caors::error status{};
};

class progress_async_loader final : public caors::resources::loader<async_resource> {
public:
  [[nodiscard]] caors::resources::load_result<async_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    auto loaded = caors::memory::make_unique<async_resource>(resource, guid);
    loaded->value = next_value;
    return {{}, std::move(loaded)};
  }

  [[nodiscard]] caors::resources::load_result<async_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource,
       caors::resources::progress_reporter reporter) override {
    reporter.report(0.25, "read");
    reporter(0.75, "decode");
    return load(guid, resource);
  }

  int next_value = 0;
};

class progress_texture_loader final : public caors::resources::loader<texture_resource> {
public:
  [[nodiscard]] caors::resources::load_result<texture_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    auto loaded = caors::memory::make_unique<texture_resource>(resource, guid);
    return {{}, std::move(loaded)};
  }

  [[nodiscard]] caors::resources::load_result<texture_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource,
       caors::resources::progress_reporter reporter) override {
    ++load_count;
    reporter.report(0.5, "texture");
    return load(guid, resource);
  }

  int load_count = 0;
};

class progress_material_loader final : public caors::resources::loader<async_material_resource> {
public:
  [[nodiscard]] caors::resources::load_result<async_material_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    auto loaded = caors::memory::make_unique<async_material_resource>(resource, guid);
    return {{}, std::move(loaded)};
  }

  [[nodiscard]] caors::resources::load_result<async_material_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource,
       caors::resources::progress_reporter reporter) override {
    ++load_count;
    reporter.report(0.5, "material");
    return load(guid, resource);
  }

  int load_count = 0;
};

[[nodiscard]] caors::string_name texture_manifest_type() {
  return caors::string_name{"caors.test.resources.async_texture"};
}

[[nodiscard]] caors::string_name material_manifest_type() {
  return caors::string_name{"caors.test.resources.async_material"};
}

class holding_executor final {
public:
  holding_executor() : executor(context.get_executor()) {}

  operator caors::tasks::executor() const noexcept { return executor; }

  [[nodiscard]] caors::error run_once() {
    if (context.poll_one() == 0) {
      return {caors::error_domain::core, caors::errc::invalid_argument};
    }
    return {};
  }

  caors::tasks::execution_context context;
  caors::tasks::executor executor;
};

[[nodiscard]] bool meta_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

void require_async_resource_metadata() {
  REQUIRE(meta_result_ok(
      caors::registration::type_<async_resource>("caors.test.resources.async_resource").commit()));
}

void require_async_preload_metadata() {
  REQUIRE(meta_result_ok(
      caors::registration::type_<texture_resource>("caors.test.resources.texture_resource")
          .commit()));
  REQUIRE(meta_result_ok(caors::registration::type_<async_material_resource>(
                             "caors.test.resources.async_material_resource")
                             .commit()));
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

} // namespace

TEST_CASE("resources - async load commits through completion executor", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 7;
  caors::tasks::inline_executor executor;
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.valid());
  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK_FALSE(cache.contains(test_guid_a));
  CHECK(completions.poll() == 1);

  REQUIRE(request.loaded());
  REQUIRE(request.try_get());
  caors::resources::handle_result<async_resource> loaded = request.result();
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);
  CHECK(loaded.value.get() == request.try_get().get());
  CHECK(request.try_get()->guid() == test_guid_a);
  CHECK(request.try_get()->value == 7);
  CHECK(cache.find(test_guid_a) == request.try_get().get());
  CHECK(loader.load_count.load() == 1);
  CHECK_FALSE(request.cancel());
}

TEST_CASE("resources - async helpers poll single request through completion queue", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 19;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);

  const caors::resources::request_poll_result result =
      caors::resources::drain_until_done(completions, request);

  CHECK(result.drained == 1);
  REQUIRE(result.done);
  REQUIRE(request.loaded());
  REQUIRE(request.try_get());
  CHECK(request.try_get()->value == 19);

  const caors::resources::request_poll_result cached =
      caors::resources::poll_request(completions, request);
  CHECK(cached.drained == 0);
  CHECK(cached.done);
}

TEST_CASE("resources - async helpers poll request batches through completion queue",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 23;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> first = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());
  caors::resources::load_request<async_resource> second = caors::resources::request_load_into(
      cache, loader, test_guid_b, executor, completions.get_executor());
  const std::array requests{first, second};

  CHECK_FALSE(caors::resources::all_requests_done(requests));

  const caors::resources::request_poll_result result =
      caors::resources::drain_until_all_done(completions, requests);

  CHECK(result.drained == 2);
  REQUIRE(result.done);
  REQUIRE(first.loaded());
  REQUIRE(second.loaded());
  CHECK(cache.contains(test_guid_a));
  CHECK(cache.contains(test_guid_b));
  CHECK(loader.load_count.load() == 2);
}

TEST_CASE("resources - async helpers report pending when no completion is queued", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  holding_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  REQUIRE(request.status() == caors::resources::request_status::queued);
  CHECK(completions.empty());

  const caors::resources::request_poll_result poll =
      caors::resources::poll_request(completions, request);
  CHECK(poll.drained == 0);
  CHECK_FALSE(poll.done);
  CHECK(request.status() == caors::resources::request_status::queued);

  const caors::resources::request_poll_result limited =
      caors::resources::drain_until_done(completions, request, 0);
  CHECK(limited.drained == 0);
  CHECK_FALSE(limited.done);

  REQUIRE(request.cancel());
}

TEST_CASE("resources - async load reports loader progress and stage", "[resources]") {
  caors::resource_cache<async_resource> cache;
  progress_async_loader loader;
  loader.next_value = 17;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(request.progress() == Catch::Approx(0.75));
  CHECK(std::string{request.progress_stage().data(), request.progress_stage().size()} == "decode");

  CHECK(completions.drain() == 1);

  REQUIRE(request.loaded());
  CHECK(request.progress() == Catch::Approx(1.0));
  CHECK(request.progress_stage() == "decode");
  REQUIRE(request.try_get());
  CHECK(request.try_get()->value == 17);
}

TEST_CASE("resources - async load returns cached resource immediately", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  async_resource* existing = cache.create(test_guid_a);
  REQUIRE(existing != nullptr);
  existing->value = 11;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.loaded());
  REQUIRE(request.try_get());
  CHECK(request.try_get().get() == existing);
  CHECK(completions.empty());
  CHECK(loader.load_count.load() == 0);
}

TEST_CASE("resources - async reload replaces cache and keeps old handle alive", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->value = 1;
  caors::resource_handle<async_resource> old_handle = cache.find_handle(test_guid_a);

  async_loader loader;
  loader.next_value = 2;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_reload_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(completions.drain() == 1);

  REQUIRE(request.loaded());
  REQUIRE(request.try_get());
  CHECK(request.try_get().get() == cache.find(test_guid_a));
  CHECK(request.try_get()->value == 2);
  CHECK(old_handle->value == 1);
  CHECK(old_handle.get() != request.try_get().get());
}

TEST_CASE("resources - async reload failure keeps existing resource", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->value = 3;

  async_loader loader;
  loader.fail_next = true;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_reload_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.failed());
  CHECK(request.error() == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(request.result().status ==
        caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK_FALSE(request.cancel());
  CHECK(cache.find(test_guid_a) == initial);
  CHECK(cache.find(test_guid_a)->value == 3);
  CHECK(completions.empty());
}

TEST_CASE("resources - async load can be cancelled before background starts", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  holding_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::queued);
  CHECK(request.result().status ==
        caors::error{caors::error_domain::resources, caors::errc::pending});

  REQUIRE(request.cancel());
  CHECK(request.cancelled());
  CHECK(request.done());
  CHECK(request.result().status ==
        caors::error{caors::error_domain::resources, caors::errc::cancelled});

  CHECK_FALSE(executor.run_once());
  CHECK(loader.load_count.load() == 0);
  CHECK(completions.empty());
  CHECK_FALSE(cache.contains(test_guid_a));
  CHECK_FALSE(request.cancel());
}

TEST_CASE("resources - async load cancellation prevents queued commit", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 17;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count.load() == 1);
  CHECK(completions.size() == 1);

  REQUIRE(request.cancel());
  CHECK(request.cancelled());
  CHECK(request.try_get() == nullptr);
  CHECK(request.result().status ==
        caors::error{caors::error_domain::resources, caors::errc::cancelled});

  CHECK(completions.drain() == 1);
  CHECK_FALSE(cache.contains(test_guid_a));
  CHECK(request.cancelled());
}

TEST_CASE("resources - database async load uses registered cache and loader", "[resources]") {
  require_async_resource_metadata();

  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 9;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(completions.drain() == 1);
  REQUIRE(request.loaded());
  CHECK(database.find<async_resource>(test_guid_a) == request.try_get().get());
  CHECK(request.try_get()->value == 9);
}

TEST_CASE("resources - database async load reuses pending request for same guid", "[resources]") {
  require_async_resource_metadata();

  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 21;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> first =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::load_request<async_resource> second =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::loading);
  CHECK(second.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count.load() == 1);
  CHECK(completions.size() == 1);

  CHECK(completions.drain() == 1);

  REQUIRE(first.loaded());
  REQUIRE(second.loaded());
  REQUIRE(first.try_get());
  CHECK(first.try_get().get() == second.try_get().get());
  CHECK(first.try_get()->value == 21);
  CHECK(loader.load_count.load() == 1);

  caors::resources::load_request<async_resource> cached =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());
  REQUIRE(cached.loaded());
  CHECK(cached.try_get().get() == first.try_get().get());
  CHECK(loader.load_count.load() == 1);
}

TEST_CASE("resources - database async load cancellation is shared by reused requests",
          "[resources]") {
  require_async_resource_metadata();

  caors::resource_cache<async_resource> cache;
  async_loader loader;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  holding_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> first =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::load_request<async_resource> second =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::queued);
  CHECK(second.status() == caors::resources::request_status::queued);
  REQUIRE(first.cancel());
  CHECK(second.cancelled());
  CHECK_FALSE(second.cancel());

  CHECK_FALSE(executor.run_once());
  CHECK(loader.load_count.load() == 0);
  CHECK_FALSE(cache.contains(test_guid_a));
}

TEST_CASE("resources - database async load keeps newer request after cancelled task exits",
          "[resources]") {
  require_async_resource_metadata();

  caors::resource_cache<async_resource> cache;
  async_loader loader;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  holding_executor cancelled_executor;
  holding_executor current_executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> first = database.request_load<async_resource>(
      test_guid_a, cancelled_executor, completions.get_executor());
  REQUIRE(first.cancel());

  caors::resources::load_request<async_resource> second = database.request_load<async_resource>(
      test_guid_a, current_executor, completions.get_executor());
  CHECK(second.status() == caors::resources::request_status::queued);

  CHECK_FALSE(cancelled_executor.run_once());
  CHECK(loader.load_count.load() == 0);

  caors::resources::load_request<async_resource> third = database.request_load<async_resource>(
      test_guid_a, current_executor, completions.get_executor());

  REQUIRE(second.cancel());
  CHECK(third.cancelled());
}

TEST_CASE("resources - database async reload reuses pending request for same guid", "[resources]") {
  require_async_resource_metadata();

  caors::resource_cache<async_resource> cache;
  async_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->value = 1;

  async_loader loader;
  loader.next_value = 22;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> first =
      database.request_reload<async_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::load_request<async_resource> second =
      database.request_reload<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::loading);
  CHECK(second.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count.load() == 1);
  CHECK(completions.size() == 1);

  CHECK(completions.drain() == 1);

  REQUIRE(first.loaded());
  REQUIRE(second.loaded());
  REQUIRE(first.try_get());
  CHECK(first.try_get().get() == second.try_get().get());
  CHECK(first.try_get()->value == 22);
  CHECK(first.try_get().get() == cache.find(test_guid_a));
  CHECK(loader.load_count.load() == 1);
}

TEST_CASE("resources - database async load reports missing registration", "[resources]") {
  caors::resource_database database;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(request.failed());
  CHECK(request.error() == caors::error{caors::error_domain::resources, caors::errc::not_found});
}

TEST_CASE("resources - async load can run background stage on thread pool", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 13;
  caors::tasks::thread_pool pool{1};
  caors::tasks::executor executor = pool.get_executor();
  caors::tasks::completion_queue completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  pool.shutdown();
  CHECK(completions.drain() == 1);

  REQUIRE(request.loaded());
  CHECK(request.try_get()->value == 13);
  CHECK(cache.find(test_guid_a) == request.try_get().get());
}

TEST_CASE("resources - database async preload dispatches manifest types", "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "materials/body.json", material_manifest_type()).succeeded());
  REQUIRE(manifest.bind(test_guid_b, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));

  caors::resource_cache<texture_resource> texture_cache;
  counting_texture_loader texture_loader;
  caors::resource_cache<async_material_resource> material_cache;
  async_material_loader material_loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(
      database.register_type(texture_cache, texture_loader, texture_manifest_type()).succeeded());
  REQUIRE(database.register_type(material_cache, material_loader, material_manifest_type())
              .succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::preload_request request =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(request.total_count() == 2);
  CHECK(request.loaded_count() == 0);
  CHECK(request.pending_count() == 2);
  CHECK(completions.size() == 2);

  CHECK(completions.drain() == 2);

  REQUIRE(request.loaded());
  CHECK(request.loaded_count() == 2);
  CHECK(request.failed_count() == 0);
  CHECK(request.pending_count() == 0);
  CHECK(request.progress() == 1.0);
  CHECK(texture_cache.contains(test_guid_b));
  CHECK(material_cache.contains(test_guid_a));
  CHECK(texture_loader.load_count == 1);
  CHECK(material_loader.load_count == 1);

  caors::resources::preload_result result = request.result();
  REQUIRE(result.succeeded());
  CHECK(contains_item(result.loaded, test_guid_b, texture_manifest_type()));
  CHECK(contains_item(result.loaded, test_guid_a, material_manifest_type()));
}

TEST_CASE("resources - database async preload aggregates loader progress", "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "materials/body.json", material_manifest_type()).succeeded());
  REQUIRE(manifest.bind(test_guid_b, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));

  caors::resource_cache<texture_resource> texture_cache;
  progress_texture_loader texture_loader;
  caors::resource_cache<async_material_resource> material_cache;
  progress_material_loader material_loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(
      database.register_type(texture_cache, texture_loader, texture_manifest_type()).succeeded());
  REQUIRE(database.register_type(material_cache, material_loader, material_manifest_type())
              .succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::preload_request request =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(request.total_count() == 2);
  CHECK(request.progress() == Catch::Approx(0.5));
  CHECK(request.progress_stage() == "material");
  CHECK(completions.size() == 2);

  CHECK(completions.drain() == 2);

  REQUIRE(request.loaded());
  CHECK(request.progress() == Catch::Approx(1.0));
  CHECK(request.loaded_count() == 2);
  CHECK(texture_loader.load_count == 1);
  CHECK(material_loader.load_count == 1);
}

TEST_CASE("resources - database async preload reuses pending direct load request", "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  REQUIRE(database.register_type(cache, loader, texture_manifest_type()).succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<texture_resource> direct =
      database.request_load<texture_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::preload_request preload =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(direct.status() == caors::resources::request_status::loading);
  CHECK(preload.status() == caors::resources::request_status::loading);
  CHECK(preload.total_count() == 1);
  CHECK(preload.pending_count() == 1);
  CHECK(loader.load_count == 1);
  CHECK(completions.size() == 1);

  CHECK(completions.drain() == 1);

  REQUIRE(direct.loaded());
  REQUIRE(preload.loaded());
  CHECK(preload.loaded_count() == 1);
  CHECK(preload.failed_count() == 0);
  CHECK(cache.contains(test_guid_a));
  CHECK(direct.try_get().get() == cache.find(test_guid_a));
  CHECK(loader.load_count == 1);

  caors::resources::preload_result result = preload.result();
  REQUIRE(result.succeeded());
  CHECK(contains_item(result.loaded, test_guid_a, texture_manifest_type()));
}

TEST_CASE("resources - database async preload observes shared load cancellation", "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  REQUIRE(database.register_type(cache, loader, texture_manifest_type()).succeeded());

  holding_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::load_request<texture_resource> direct =
      database.request_load<texture_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::preload_request preload =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(direct.status() == caors::resources::request_status::queued);
  CHECK(preload.status() == caors::resources::request_status::loading);
  REQUIRE(direct.cancel());
  CHECK(preload.failed());
  CHECK(preload.error() == caors::error{caors::error_domain::resources, caors::errc::cancelled});
  CHECK(preload.failed_count() == 1);
  CHECK(preload.pending_count() == 0);
  caors::resources::preload_result result = preload.result();
  CHECK(contains_failure(result.failed_items, test_guid_a, texture_manifest_type(),
                         {caors::error_domain::resources, caors::errc::cancelled}));

  CHECK_FALSE(executor.run_once());
  CHECK(loader.load_count == 0);
  CHECK_FALSE(cache.contains(test_guid_a));
}

TEST_CASE("resources - database async preload reuses pending preload request", "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_cache<texture_resource> cache;
  counting_texture_loader loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  REQUIRE(database.register_type(cache, loader, texture_manifest_type()).succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::preload_request first =
      database.request_preload(test_guid_a, executor, completions.get_executor());
  caors::resources::preload_request second =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::loading);
  CHECK(second.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count == 1);
  CHECK(completions.size() == 1);

  CHECK(completions.drain() == 1);

  REQUIRE(first.loaded());
  REQUIRE(second.loaded());
  CHECK(first.loaded_count() == 1);
  CHECK(second.loaded_count() == 1);
  CHECK(first.failed_count() == 0);
  CHECK(second.failed_count() == 0);
  CHECK(cache.contains(test_guid_a));
  CHECK(loader.load_count == 1);
}

TEST_CASE("resources - database async preload completes cached resources immediately",
          "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_cache<texture_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);
  counting_texture_loader loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  REQUIRE(database.register_type(cache, loader, texture_manifest_type()).succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::preload_request preload =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  REQUIRE(preload.loaded());
  CHECK(preload.total_count() == 0);
  CHECK(preload.loaded_count() == 0);
  CHECK(preload.pending_count() == 0);
  CHECK(preload.progress() == 1.0);
  CHECK(loader.load_count == 0);
  CHECK(completions.size() == 0);

  caors::resources::preload_result result = preload.result();
  REQUIRE(result.succeeded());
  CHECK(contains_item(result.plan.already_loaded, test_guid_a, texture_manifest_type()));
}

TEST_CASE("resources - database async preload cancellation skips queued commits", "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "materials/body.json", material_manifest_type()).succeeded());
  REQUIRE(manifest.bind(test_guid_b, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));

  caors::resource_cache<texture_resource> texture_cache;
  counting_texture_loader texture_loader;
  caors::resource_cache<async_material_resource> material_cache;
  async_material_loader material_loader;

  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(
      database.register_type(texture_cache, texture_loader, texture_manifest_type()).succeeded());
  REQUIRE(database.register_type(material_cache, material_loader, material_manifest_type())
              .succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::preload_request request =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  REQUIRE(request.cancel());
  CHECK(request.cancelled());
  CHECK(request.result().status ==
        caors::error{caors::error_domain::resources, caors::errc::cancelled});

  CHECK(completions.drain() == 2);
  CHECK_FALSE(texture_cache.contains(test_guid_b));
  CHECK_FALSE(material_cache.contains(test_guid_a));
  CHECK(request.pending_count() == 0);
  CHECK(request.progress() == 1.0);
}

TEST_CASE("resources - database async preload fails the batch when one item fails", "[resources]") {
  require_async_preload_metadata();

  caors::resource_manifest manifest;
  REQUIRE(manifest.bind(test_guid_a, "materials/body.json", material_manifest_type()).succeeded());
  REQUIRE(manifest.bind(test_guid_b, "textures/albedo.json", texture_manifest_type()).succeeded());

  caors::resource_dependency_graph graph;
  REQUIRE(graph.add(test_guid_a, test_guid_b));

  caors::resource_cache<texture_resource> texture_cache;
  counting_texture_loader texture_loader;
  caors::resource_cache<async_material_resource> material_cache;
  async_material_loader material_loader;
  material_loader.status = {caors::error_domain::resources, caors::errc::not_found};

  caors::resource_database database;
  database.set_manifest(manifest);
  database.set_dependency_graph(graph);
  REQUIRE(
      database.register_type(texture_cache, texture_loader, texture_manifest_type()).succeeded());
  REQUIRE(database.register_type(material_cache, material_loader, material_manifest_type())
              .succeeded());

  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resources::preload_request request =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  REQUIRE(request.failed());
  CHECK(request.error() == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(request.failed_count() == 1);
  CHECK(request.pending_count() == 0);

  CHECK(completions.drain() == 1);
  CHECK_FALSE(texture_cache.contains(test_guid_b));
  CHECK_FALSE(material_cache.contains(test_guid_a));
  caors::resources::preload_result result = request.result();
  CHECK(result.status == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(contains_failure(result.failed_items, test_guid_a, material_manifest_type(),
                         {caors::error_domain::resources, caors::errc::not_found}));
}
