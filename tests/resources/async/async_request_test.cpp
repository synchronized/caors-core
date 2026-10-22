// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

#include "caors/memory/tracking.h"
#include "caors/tasks.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <thread>

using namespace caors::tests::resources;

namespace {

class async_resource final : public caors::resource {
public:
  using caors::resource::resource;

  int value = 0;
};

class async_loader final : public caors::resources::sync_loader_adapter<async_resource> {
public:
  [[nodiscard]] caors::resources::load_result<async_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    load_count.fetch_add(1);
    if (load_thread != nullptr) {
      *load_thread = std::this_thread::get_id();
    }
    if (fail_next) {
      fail_next = false;
      return {{caors::error_domain::resources, caors::errc::not_found}, {}};
    }

    auto loaded = caors::memory::make_unique<async_resource>(resource, guid);
    loaded->value = next_value;
    return {{}, std::move(loaded)};
  }

  std::atomic<int> load_count{0};
  std::thread::id* load_thread = nullptr;
  int next_value = 0;
  bool fail_next = false;
};

class async_material_resource final : public caors::resource {
public:
  using caors::resource::resource;
};

class async_material_loader final
    : public caors::resources::sync_loader_adapter<async_material_resource> {
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

class progress_async_loader final : public caors::resources::sync_loader_adapter<async_resource> {
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

class progress_texture_loader final
    : public caors::resources::sync_loader_adapter<texture_resource> {
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

class progress_material_loader final
    : public caors::resources::sync_loader_adapter<async_material_resource> {
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

struct waiter_probe {
  std::function<void()> action;
};

void notify_waiter_probe(const std::shared_ptr<void>& erased) noexcept {
  const auto probe = std::static_pointer_cast<waiter_probe>(erased);
  probe->action();
}

[[nodiscard]] caors::resources::detail::request_waiter
make_waiter_probe(std::function<void()> action) {
  return {std::make_shared<waiter_probe>(waiter_probe{std::move(action)}), &notify_waiter_probe};
}

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

TEST_CASE("resources - request states use caller long lived resource", "[resources]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::resources::load_request<async_resource> request =
        caors::resources::load_request<async_resource>::failed(
            {caors::error_domain::resources, caors::errc::not_found}, &tracking);
    CHECK(tracking.statistics().bytes_in_use > 0);
  }
  CHECK(tracking.statistics().bytes_in_use == 0);

  {
    caors::resources::preload_request request = caors::resources::preload_request::failed(
        {caors::error_domain::resources, caors::errc::not_found}, &tracking);
    CHECK(tracking.statistics().bytes_in_use > 0);
  }
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("resources - load request notifies every waiter outside state lock", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 17;
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;
  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());
  auto state = caors::resources::detail::load_request_access<async_resource>::state(request);
  int notified = 0;

  REQUIRE(caors::resources::detail::load_request_access<async_resource>::add_waiter(
      state, make_waiter_probe([&] {
        ++notified;
        CHECK(request.loaded());
        CHECK(request.result().succeeded());
      })));
  REQUIRE(caors::resources::detail::load_request_access<async_resource>::add_waiter(
      state, make_waiter_probe([&] { ++notified; })));

  CHECK(completions.drain() == 1);
  CHECK(notified == 2);
  CHECK_FALSE(caors::resources::detail::load_request_access<async_resource>::add_waiter(
      state, make_waiter_probe([] {})));
}

TEST_CASE("resources - load request notifies waiters on failure and cancellation", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.fail_next = true;
  holding_executor held;
  caors::tasks::execution_context completions;
  caors::resources::load_request<async_resource> failed = caors::resources::request_load_into(
      cache, loader, test_guid_a, held, completions.get_executor());
  int failed_notifications = 0;
  REQUIRE(caors::resources::detail::load_request_access<async_resource>::add_waiter(
      caors::resources::detail::load_request_access<async_resource>::state(failed),
      make_waiter_probe([&] { ++failed_notifications; })));

  CHECK_FALSE(held.run_once());
  CHECK(failed.failed());
  CHECK(failed_notifications == 1);

  holding_executor cancelled_executor;
  caors::resources::load_request<async_resource> cancelled = caors::resources::request_load_into(
      cache, loader, test_guid_b, cancelled_executor, completions.get_executor());
  int cancelled_notifications = 0;
  REQUIRE(caors::resources::detail::load_request_access<async_resource>::add_waiter(
      caors::resources::detail::load_request_access<async_resource>::state(cancelled),
      make_waiter_probe([&] { ++cancelled_notifications; })));

  REQUIRE(cancelled.cancel());
  CHECK(cancelled_notifications == 1);
  CHECK_FALSE(cancelled_executor.run_once());
}

TEST_CASE("resources - preload request notifies waiters for every terminal path", "[resources]") {
  const caors::resources::preload_item item{test_guid_a, texture_manifest_type()};

  auto make_request = [&] {
    caors::resources::preload_plan plan;
    plan.load_order.push_back(item);
    return caors::resources::preload_request::from_plan(std::move(plan));
  };

  caors::resources::preload_request completed = make_request();
  auto completed_state = caors::resources::detail::preload_request_access::state(completed);
  int completed_notifications = 0;
  REQUIRE(caors::resources::detail::preload_request_access::add_waiter(
      completed_state, make_waiter_probe([&] {
        ++completed_notifications;
        CHECK(completed.loaded());
        CHECK(completed.result().succeeded());
      })));
  caors::resources::detail::preload_request_access::complete_item(completed_state, item);
  CHECK(completed_notifications == 1);

  caors::resources::preload_request failed = make_request();
  auto failed_state = caors::resources::detail::preload_request_access::state(failed);
  int failed_notifications = 0;
  REQUIRE(caors::resources::detail::preload_request_access::add_waiter(
      failed_state, make_waiter_probe([&] { ++failed_notifications; })));
  caors::resources::detail::preload_request_access::fail_item(
      failed_state, item, {caors::error_domain::resources, caors::errc::not_found});
  CHECK(failed_notifications == 1);

  caors::resources::preload_request cancelled = make_request();
  auto cancelled_state = caors::resources::detail::preload_request_access::state(cancelled);
  int cancelled_notifications = 0;
  REQUIRE(caors::resources::detail::preload_request_access::add_waiter(
      cancelled_state, make_waiter_probe([&] { ++cancelled_notifications; })));
  REQUIRE(cancelled.cancel());
  CHECK(cancelled_notifications == 1);
}

TEST_CASE("resources - waiter registration racing terminal transition does not lose notification",
          "[resources]") {
  for (int iteration = 0; iteration < 128; ++iteration) {
    caors::resources::load_request<async_resource> request =
        caors::resources::detail::load_request_access<async_resource>::make();
    auto state = caors::resources::detail::load_request_access<async_resource>::state(request);
    std::atomic<int> notifications{0};
    std::atomic<bool> registered{false};

    std::thread register_thread{[&] {
      registered.store(caors::resources::detail::load_request_access<async_resource>::add_waiter(
                           state, make_waiter_probe([&] { notifications.fetch_add(1); })),
                       std::memory_order_release);
    }};
    std::thread cancel_thread{[&] { (void)request.cancel(); }};
    register_thread.join();
    cancel_thread.join();

    CHECK(request.cancelled());
    CHECK(notifications.load() == (registered.load(std::memory_order_acquire) ? 1 : 0));
  }
}

TEST_CASE("resources - async load commits through completion executor", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 7;
  caors::tasks::executor executor = caors::tasks::inline_executor();
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

TEST_CASE("resources - async helpers poll single request through execution context",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 19;
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

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

TEST_CASE("resources - async helpers poll request batches through execution context",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 23;
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

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
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  REQUIRE(request.status() == caors::resources::request_status::queued);
  CHECK(completions.pending() == 0);

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
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

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
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  async_resource* existing = cache.create(test_guid_a);
  REQUIRE(existing != nullptr);
  existing->value = 11;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.loaded());
  REQUIRE(request.try_get());
  CHECK(request.try_get().get() == existing);
  CHECK(completions.pending() == 0);
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
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

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
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_reload_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.failed());
  CHECK(request.error() == caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK(request.result().status ==
        caors::error{caors::error_domain::resources, caors::errc::not_found});
  CHECK_FALSE(request.cancel());
  CHECK(cache.find(test_guid_a) == initial);
  CHECK(cache.find(test_guid_a)->value == 3);
  CHECK(completions.pending() == 0);
}

TEST_CASE("resources - async load can be cancelled before background starts", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  holding_executor executor;
  caors::tasks::execution_context completions;

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
  CHECK(completions.pending() == 0);
  CHECK_FALSE(cache.contains(test_guid_a));
  CHECK_FALSE(request.cancel());
}

TEST_CASE("resources - async load cancellation prevents queued commit", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 17;
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count.load() == 1);
  CHECK(completions.pending() == 1);

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
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

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
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> first =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::load_request<async_resource> second =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::loading);
  CHECK(second.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count.load() == 1);
  CHECK(completions.pending() == 1);

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

TEST_CASE("resources - database async load cancellation is isolated between subscriptions",
          "[resources]") {
  require_async_resource_metadata();

  caors::resource_cache<async_resource> cache;
  async_loader loader;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  holding_executor executor;
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> first =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::load_request<async_resource> second =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::queued);
  CHECK(second.status() == caors::resources::request_status::queued);
  REQUIRE(first.cancel());
  CHECK_FALSE(second.cancelled());
  auto first_copy = first;
  CHECK(first_copy.cancelled());
  CHECK_FALSE(executor.run_once());
  CHECK(loader.load_count.load() == 1);
  CHECK(completions.drain() == 1);
  CHECK(second.loaded());
  CHECK(first.cancelled());
  CHECK(cache.contains(test_guid_a));
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
  caors::tasks::execution_context completions;

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

  REQUIRE(second.cancel_shared());
  CHECK(third.cancelled());
}

TEST_CASE("resources - database reload starts independent requests and publishes the latest",
          "[resources]") {
  require_async_resource_metadata();

  caors::resource_cache<async_resource> cache;
  async_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->value = 1;

  async_loader loader;
  loader.next_value = 22;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> first =
      database.request_reload<async_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::load_request<async_resource> second =
      database.request_reload<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::loading);
  CHECK(second.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count.load() == 2);
  CHECK(completions.pending() == 2);

  CHECK(completions.drain() == 2);

  REQUIRE(first.loaded());
  REQUIRE(second.loaded());
  REQUIRE(first.try_get());
  CHECK(first.try_get().get() != second.try_get().get());
  CHECK(first.try_get()->value == 22);
  CHECK(second.try_get().get() == cache.find(test_guid_a));
  CHECK(loader.load_count.load() == 2);
}

TEST_CASE("resources - database async load reports missing registration", "[resources]") {
  caors::resource_database database;
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> request =
      database.request_load<async_resource>(test_guid_a, executor, completions.get_executor());

  CHECK(request.failed());
  CHECK(request.error() == caors::error{caors::error_domain::resources, caors::errc::not_found});
}

TEST_CASE("resources - async load can run background stage on thread pool", "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  loader.next_value = 13;
  std::thread::id load_thread;
  loader.load_thread = &load_thread;
  const std::thread::id completion_thread = std::this_thread::get_id();
  caors::tasks::thread_pool pool{1};
  caors::tasks::executor executor = pool.get_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<async_resource> request = caors::resources::request_load_into(
      cache, loader, test_guid_a, executor, completions.get_executor());

  pool.shutdown();
  CHECK(load_thread != completion_thread);
  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK_FALSE(cache.contains(test_guid_a));
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

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::preload_request request =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(request.total_count() == 2);
  CHECK(request.loaded_count() == 0);
  CHECK(request.pending_count() == 2);
  CHECK(completions.pending() == 2);

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

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::preload_request request =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(request.status() == caors::resources::request_status::loading);
  CHECK(request.total_count() == 2);
  CHECK(request.progress() == Catch::Approx(0.5));
  CHECK(request.progress_stage() == "material");
  CHECK(completions.pending() == 2);

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

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::load_request<texture_resource> direct =
      database.request_load<texture_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::preload_request preload =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(direct.status() == caors::resources::request_status::loading);
  CHECK(preload.status() == caors::resources::request_status::loading);
  CHECK(preload.total_count() == 1);
  CHECK(preload.pending_count() == 1);
  CHECK(loader.load_count == 1);
  CHECK(completions.pending() == 1);

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
  caors::tasks::execution_context completions;

  caors::resources::load_request<texture_resource> direct =
      database.request_load<texture_resource>(test_guid_a, executor, completions.get_executor());
  caors::resources::preload_request preload =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(direct.status() == caors::resources::request_status::queued);
  CHECK(preload.status() == caors::resources::request_status::loading);
  REQUIRE(direct.cancel_shared());
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

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::preload_request first =
      database.request_preload(test_guid_a, executor, completions.get_executor());
  caors::resources::preload_request second =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  CHECK(first.status() == caors::resources::request_status::loading);
  CHECK(second.status() == caors::resources::request_status::loading);
  CHECK(loader.load_count == 1);
  CHECK(completions.pending() == 1);

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

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

  caors::resources::preload_request preload =
      database.request_preload(test_guid_a, executor, completions.get_executor());

  REQUIRE(preload.loaded());
  CHECK(preload.total_count() == 0);
  CHECK(preload.loaded_count() == 0);
  CHECK(preload.pending_count() == 0);
  CHECK(preload.progress() == 1.0);
  CHECK(loader.load_count == 0);
  CHECK(completions.pending() == 0);

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

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

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

  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::execution_context completions;

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

TEST_CASE("resources - all subscriptions cancel without cache commit", "[resources]") {
  require_async_resource_metadata();
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  holding_executor background;
  caors::tasks::execution_context completions;
  auto a =
      database.request_load<async_resource>(test_guid_a, background, completions.get_executor());
  auto b =
      database.request_load<async_resource>(test_guid_a, background, completions.get_executor());
  REQUIRE(a.cancel());
  REQUIRE(b.cancel());
  CHECK_FALSE(background.run_once());
  CHECK(loader.load_count == 0);
  CHECK(completions.drain() == 0);
  CHECK_FALSE(cache.contains(test_guid_a));
}

TEST_CASE("resources - subscription cancellation races completion with one terminal notification",
          "[resources]") {
  require_async_resource_metadata();
  for (int round = 0; round != 64; ++round) {
    caors::resource_cache<async_resource> cache;
    async_loader loader;
    caors::resource_database database;
    REQUIRE(database.register_type(cache, loader).succeeded());
    caors::tasks::execution_context completions;
    auto a = database.request_load<async_resource>(test_guid_a, caors::tasks::inline_executor(),
                                                   completions.get_executor());
    auto b = database.request_load<async_resource>(test_guid_a, caors::tasks::inline_executor(),
                                                   completions.get_executor());
    std::atomic<int> notifications{0};
    REQUIRE(caors::resources::detail::load_request_access<async_resource>::add_waiter(
        caors::resources::detail::load_request_access<async_resource>::state(a),
        make_waiter_probe([&] { ++notifications; })));
    std::atomic<bool> start{false};
    std::thread canceller{[&] {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      (void)a.cancel();
    }};
    start.store(true, std::memory_order_release);
    CHECK(completions.drain() == 1);
    canceller.join();
    CHECK(notifications == 1);
    CHECK(a.done());
    CHECK(b.loaded());
    CHECK(loader.load_count == 1);
  }
}

namespace {
class external_resource_queue final : public caors::tasks::external_executor_context {
public:
  caors::error post(caors::tasks::task next) override {
    std::lock_guard lock{mutex_};
    if (closed_) {
      return {caors::error_domain::core, caors::errc::frozen};
    }
    queued_.push_back(std::move(next));
    return {};
  }
  void drain() {
    std::vector<caors::tasks::task> ready;
    {
      std::lock_guard lock{mutex_};
      ready.swap(queued_);
      active_ += ready.size();
    }
    for (auto& next : ready) {
      (void)next.run();
      next.reset();
      {
        std::lock_guard lock{mutex_};
        --active_;
      }
      stopped_.notify_all();
    }
  }
  void close() noexcept override {
    std::vector<caors::tasks::task> discarded;
    {
      std::unique_lock lock{mutex_};
      closed_ = true;
      discarded.swap(queued_);
      stopped_.wait(lock, [&] { return active_ == 0; });
    }
  }

private:
  std::mutex mutex_;
  std::condition_variable stopped_;
  std::vector<caors::tasks::task> queued_;
  std::size_t active_ = 0;
  bool closed_ = false;
};
}

TEST_CASE("resources - host queues preserve independent cancellation and commit ownership",
          "[resources]") {
  require_async_resource_metadata();
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  std::thread::id load_thread;
  loader.load_thread = &load_thread;
  caors::resource_database database;
  REQUIRE(database.register_type(cache, loader).succeeded());
  auto background = std::make_shared<external_resource_queue>();
  auto completion = std::make_shared<external_resource_queue>();
  caors::tasks::external_executor worker{background};
  caors::tasks::external_executor owner{completion};
  auto a = database.request_load<async_resource>(test_guid_a, worker.get_executor(),
                                                 owner.get_executor());
  auto b = database.request_load<async_resource>(test_guid_a, worker.get_executor(),
                                                 owner.get_executor());
  REQUIRE(a.cancel());
  std::thread thread{[&] { background->drain(); }};
  thread.join();
  CHECK(load_thread != std::this_thread::get_id());
  CHECK(loader.load_count == 1);
  CHECK_FALSE(cache.contains(test_guid_a));
  completion->drain();
  CHECK(a.cancelled());
  CHECK(b.loaded());
  CHECK(cache.contains(test_guid_a));
}

TEST_CASE("resources - discarded host work always reaches a terminal state", "[resources]") {
  require_async_resource_metadata();
  for (bool discard_completion : {false, true}) {
    caors::resource_cache<async_resource> cache;
    async_loader loader;
    auto background = std::make_shared<external_resource_queue>();
    auto completion = std::make_shared<external_resource_queue>();
    caors::tasks::external_executor worker{background};
    caors::tasks::external_executor owner{completion};
    auto request = caors::resources::request_load_into(cache, loader, test_guid_a,
                                                       worker.get_executor(), owner.get_executor());
    if (discard_completion) {
      background->drain();
      owner.close();
    } else {
      worker.close();
    }
    CHECK(request.failed());
    CHECK(request.error() == caors::error{caors::error_domain::resources, caors::errc::cancelled});
    CHECK_FALSE(cache.contains(test_guid_a));
  }
}

TEST_CASE("resources - request table lookup does not expose shared cancellation implicitly",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  holding_executor background;
  caors::tasks::execution_context completion;
  caors::resources::load_request_table<async_resource> table;
  auto first = caors::resources::request_load_into(cache, loader, test_guid_a, background,
                                                   completion.get_executor(), table);
  auto second = table.find_pending(test_guid_a);
  REQUIRE(second);
  REQUIRE(second.cancel());
  CHECK_FALSE(first.cancelled());
  CHECK_FALSE(background.run_once());
  CHECK(completion.drain() == 1);
  CHECK(first.loaded());
  CHECK(second.cancelled());
}

#if CAORS_HAS_EXCEPTIONS
namespace {
class failing_commit_memory final : public caors::memory_resource {
public:
  int remaining = -1;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override {
    if (remaining == 0) {
      remaining = -1;
      return nullptr;
    }
    if (remaining > 0) {
      --remaining;
    }
    return caors::get_system_memory_resource().try_allocate(size, alignment);
  }
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override {
    caors::get_system_memory_resource().deallocate(ptr, size, alignment);
  }
  bool do_is_equal(const caors::memory_resource& other) const noexcept override {
    return this == &other;
  }
};
}
TEST_CASE("resources - cache allocation failure completes all independent subscribers",
          "[resources]") {
  failing_commit_memory memory;
  caors::resource_cache<async_resource> cache{&memory};
  async_loader loader;
  caors::resources::load_request_table<async_resource> table{&memory};
  caors::tasks::execution_context completion;
  auto a = caors::resources::request_load_into(cache, loader, test_guid_a,
                                               caors::tasks::inline_executor(),
                                               completion.get_executor(), table);
  auto b = caors::resources::request_load_into(cache, loader, test_guid_a,
                                               caors::tasks::inline_executor(),
                                               completion.get_executor(), table);
  // 单次注入：允许控制块分配，随后使 cache map 节点分配失败；错误报告仍可使用资源。
  memory.remaining = 1;
  CHECK(completion.drain() == 1);
  memory.remaining = -1;
  CHECK(a.failed());
  CHECK(b.failed());
  CHECK(a.error() == caors::error{caors::error_domain::resources, caors::errc::out_of_memory});
  CHECK_FALSE(cache.contains(test_guid_a));
}
#endif

TEST_CASE("resources - a new subscription replaces an operation already marked abandoned",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  holding_executor old_background;
  holding_executor new_background;
  caors::tasks::execution_context completion;
  caors::resources::load_request_table<async_resource> table;
  auto a = caors::resources::request_load_into(cache, loader, test_guid_a, old_background,
                                               completion.get_executor(), table);
  auto pending =
      caors::resources::detail::load_request_access<async_resource>::find_pending_operation(
          table, test_guid_a);
  auto child = caors::resources::detail::load_request_access<async_resource>::state(a);
  auto operation = child->operation.lock();
  REQUIRE(operation);
  // 固定最后一个订阅退出后、旧操作发布 cancelled 之前的窗口。
  {
    std::lock_guard lock{child->mutex};
    child->status = caors::resources::request_status::cancelled;
    child->status_error = {caors::error_domain::resources, caors::errc::cancelled};
  }
  {
    std::lock_guard lock{operation->mutex};
    operation->abandoned = true;
  }
  CHECK_FALSE(
      caors::resources::detail::load_request_access<async_resource>::subscribe(pending, true));
  auto b = caors::resources::request_load_into(cache, loader, test_guid_a, new_background,
                                               completion.get_executor(), table);
  CHECK_FALSE(b.done());
  CHECK_FALSE(old_background.run_once());
  CHECK_FALSE(new_background.run_once());
  CHECK(completion.drain() == 1);
  CHECK(b.loaded());
  CHECK(loader.load_count == 1);
}

TEST_CASE("resources - late independent subscription inherits current loader progress",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  progress_async_loader loader;
  caors::resources::load_request_table<async_resource> table;
  caors::tasks::execution_context completion;
  auto a = caors::resources::request_load_into(cache, loader, test_guid_a,
                                               caors::tasks::inline_executor(),
                                               completion.get_executor(), table);
  auto b = caors::resources::request_load_into(cache, loader, test_guid_a,
                                               caors::tasks::inline_executor(),
                                               completion.get_executor(), table);
  CHECK(a.progress() > 0.0);
  CHECK(b.progress() == a.progress());
  CHECK(b.progress_stage() == a.progress_stage());
  CHECK(completion.drain() == 1);
  CHECK(a.loaded());
  CHECK(b.loaded());
}

TEST_CASE("resources - a terminal callback may release the cancelling request handle",
          "[resources]") {
  caors::resource_cache<async_resource> cache;
  async_loader loader;
  holding_executor background;
  caors::tasks::execution_context completion;
  auto request = caors::resources::request_load_into(cache, loader, test_guid_a, background,
                                                     completion.get_executor());
  auto state = caors::resources::detail::load_request_access<async_resource>::state(request);
  REQUIRE(caors::resources::detail::load_request_access<async_resource>::add_waiter(
      state, make_waiter_probe([&] { request = {}; })));
  CHECK(request.cancel());
  CHECK_FALSE(request);
  CHECK_FALSE(background.run_once());
  CHECK(loader.load_count == 0);
}
