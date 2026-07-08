// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

#include "caors/tasks.h"

#include <atomic>

using namespace caors::tests::resources;

namespace {

class async_resource final : public caors::resource {
public:
  using caors::resource::resource;

  int value = 0;
};

class async_loader final : public caors::resource_loader<async_resource> {
public:
  [[nodiscard]] caors::resource_load_result<async_resource>
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

class holding_executor final : public caors::tasks::executor {
public:
  [[nodiscard]] caors::error submit(caors::tasks::task next) override {
    if (!next) {
      return {caors::error_domain::core, caors::errc::invalid_argument};
    }

    queued = std::move(next);
    return {};
  }

  [[nodiscard]] caors::error run_once() {
    caors::tasks::task current = std::move(queued);
    if (!current) {
      return {caors::error_domain::core, caors::errc::invalid_argument};
    }

    return current.run();
  }

  caors::tasks::task queued{};
};

[[nodiscard]] bool meta_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

void require_async_resource_metadata() {
  REQUIRE(meta_result_ok(
      caors::meta::type_<async_resource>("caors.test.resources.async_resource").result()));
}

} // namespace

TEST_CASE("resources - async load commits loaded resource through completion queue",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 7;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resource_load_request<async_resource> request =
      caors::request_load_into(cache, loader, test_guid_a, executor, completions);

  CHECK(request.valid());
  CHECK(request.status() == caors::resource_request_status::loading);
  CHECK_FALSE(cache.contains(test_guid_a));
  CHECK(completions.drain() == 1);

  REQUIRE(request.loaded());
  REQUIRE(request.get());
  caors::resource_handle_result<async_resource> loaded = request.result();
  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);
  CHECK(loaded.value.get() == request.get().get());
  CHECK(request.get()->guid() == test_guid_a);
  CHECK(request.get()->value == 7);
  CHECK(cache.find(test_guid_a) == request.get().get());
  CHECK(loader.load_count.load() == 1);
  CHECK_FALSE(request.cancel());
}

TEST_CASE("resources - async load returns cached resource immediately", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  async_resource* existing = cache.create(test_guid_a);
  REQUIRE(existing != nullptr);
  existing->value = 11;

  caors::resource_load_request<async_resource> request =
      caors::request_load_into(cache, loader, test_guid_a, executor, completions);

  CHECK(request.loaded());
  REQUIRE(request.get());
  CHECK(request.get().get() == existing);
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

  caors::resource_load_request<async_resource> request =
      caors::request_reload_into(cache, loader, test_guid_a, executor, completions);

  CHECK(request.status() == caors::resource_request_status::loading);
  CHECK(completions.drain() == 1);

  REQUIRE(request.loaded());
  REQUIRE(request.get());
  CHECK(request.get().get() == cache.find(test_guid_a));
  CHECK(request.get()->value == 2);
  CHECK(old_handle->value == 1);
  CHECK(old_handle.get() != request.get().get());
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

  caors::resource_load_request<async_resource> request =
      caors::request_reload_into(cache, loader, test_guid_a, executor, completions);

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

  caors::resource_load_request<async_resource> request =
      caors::request_load_into(cache, loader, test_guid_a, executor, completions);

  CHECK(request.status() == caors::resource_request_status::queued);
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

  caors::resource_load_request<async_resource> request =
      caors::request_load_into(cache, loader, test_guid_a, executor, completions);

  CHECK(request.status() == caors::resource_request_status::loading);
  CHECK(loader.load_count.load() == 1);
  CHECK(completions.size() == 1);

  REQUIRE(request.cancel());
  CHECK(request.cancelled());
  CHECK(request.get() == nullptr);
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

  caors::resource_load_request<async_resource> request =
      database.request_load<async_resource>(test_guid_a, executor, completions);

  CHECK(request.status() == caors::resource_request_status::loading);
  CHECK(completions.drain() == 1);
  REQUIRE(request.loaded());
  CHECK(database.find<async_resource>(test_guid_a) == request.get().get());
  CHECK(request.get()->value == 9);
}

TEST_CASE("resources - database async load reports missing registration", "[resources]") {
  caors::resource_database database;
  caors::tasks::inline_executor executor;
  caors::tasks::completion_queue completions;

  caors::resource_load_request<async_resource> request =
      database.request_load<async_resource>(test_guid_a, executor, completions);

  CHECK(request.failed());
  CHECK(request.error() == caors::error{caors::error_domain::resources, caors::errc::not_found});
}

TEST_CASE("resources - async load can run background stage on thread pool", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 13;
  caors::tasks::thread_pool_executor executor{1};
  caors::tasks::completion_queue completions;

  caors::resource_load_request<async_resource> request =
      caors::request_load_into(cache, loader, test_guid_a, executor, completions);

  executor.shutdown();
  CHECK(completions.drain() == 1);

  REQUIRE(request.loaded());
  CHECK(request.get()->value == 13);
  CHECK(cache.find(test_guid_a) == request.get().get());
}
