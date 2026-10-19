// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "../../resources/resources_test_common.h"
#include "caors/resources/coroutine.h"
#include "caors/tasks/coroutine.h"

#include <optional>
#include <thread>
#include <vector>

using namespace caors::tests::resources;

namespace {
using texture = texture_resource;

class delayed_loader final : public caors::resources::async_loader<texture> {
public:
  struct operation {
    caors::resources::async_load_context context;
    caors::resources::load_completion<texture> completion;
  };
  caors::error start(caors::resources::async_load_context context,
                     caors::resources::load_completion<texture> completion) override {
    ++starts;
    if (reject) {
      return {caors::error_domain::io, caors::errc::not_found};
    }
    if (drop) {
      return {};
    }
    if (immediate) {
      (void)completion.complete(
          {{}, caors::memory::make_unique<texture>(context.resource, context.guid)});
      return {};
    }
    pending.push_back({std::move(context), std::move(completion)});
    return {};
  }
  void finish(caors::error status = {}) {
    REQUIRE_FALSE(pending.empty());
    auto operation = std::move(pending.front());
    pending.erase(pending.begin());
    caors::resources::load_result<texture> result{status, {}};
    if (!status) {
      result.value =
          caors::memory::make_unique<texture>(operation.context.resource, operation.context.guid);
    }
    CHECK(operation.completion.complete(std::move(result)));
    CHECK_FALSE(operation.completion.complete(
        {{caors::error_domain::io, caors::errc::backend_failure}, {}}));
  }
  int starts = 0;
  bool reject = false;
  bool drop = false;
  bool immediate = false;
  std::vector<operation> pending;
};

struct fixture {
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::resources::cache<texture> cache;
  delayed_loader loader;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  fixture() {
    REQUIRE_FALSE(assets.register_type(cache, loader, caors::string_name{"group.texture"}));
  }
  ~fixture() {
    loader.pending.clear();
    (void)completion.drain();
    (void)worker.drain();
    (void)completion.drain();
  }
};
} // namespace

TEST_CASE("assets - native async loader releases worker while I/O waits",
          "[assets][native-async]") {
  fixture f;
  auto request = f.assets.request_load<texture>(test_guid_a);
  CHECK(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 1);
  CHECK_FALSE(request.done());
  CHECK(f.assets.database().has_active_operations());
  int observed = 0;
  REQUIRE_FALSE(f.worker.get_executor().post(caors::tasks::task{[&] { ++observed; }}));
  CHECK(f.worker.drain() == 1);
  CHECK(observed == 1);
  f.loader.pending.front().context.progress.report(0.5, "等待宿主读取");
  CHECK(request.progress() == 0.5);
  f.loader.finish();
  CHECK_FALSE(request.done());
  CHECK(f.completion.drain() == 1);
  CHECK(request.loaded());
  CHECK_FALSE(f.assets.database().has_active_operations());
  CHECK(f.assets.load_handle<texture>(test_guid_a).succeeded());
  CHECK(f.assets.request_load<texture>(test_guid_a).loaded());
}

TEST_CASE("assets - native async refusal and abandoned completion preserve terminal errors",
          "[assets][native-async]") {
  for (bool refusal : {false, true}) {
    fixture f;
    f.loader.reject = refusal;
    f.loader.drop = !refusal;
    auto request = f.assets.request_load<texture>(test_guid_a);
    CHECK(f.worker.drain() == 1);
    REQUIRE(request.failed());
    CHECK(request.error() ==
          (refusal ? caors::error{caors::error_domain::io, caors::errc::not_found}
                   : caors::error{caors::error_domain::resources, caors::errc::cancelled}));
    CHECK_FALSE(f.assets.database().has_active_operations());
  }
}

TEST_CASE("assets - group sharing preserves first scheduler and independent cancellation",
          "[assets][native-async]") {
  fixture f;
  caors::tasks::execution_context other_worker;
  caors::tasks::execution_context other_completion;
  auto first_group = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor());
  auto other_group =
      f.assets.create_group(other_worker.get_executor(), other_completion.get_executor());
  auto first = first_group.request_load<texture>(test_guid_a);
  auto second = other_group.request_load<texture>(test_guid_a);
  CHECK(f.worker.drain() == 1);
  CHECK(other_worker.pending() == 0);
  CHECK(f.loader.starts == 1);
  REQUIRE(first.cancel());
  REQUIRE_FALSE(f.loader.pending.front().context.stop.stop_requested());
  f.loader.finish();
  CHECK(other_completion.drain() == 0);
  CHECK_FALSE(second.done());
  CHECK(f.completion.drain() == 1);
  CHECK(first.cancelled());
  CHECK(second.loaded());
}

TEST_CASE("assets - cancelled I/O stays active until late callback and cleanup",
          "[assets][native-async]") {
  fixture f;
  auto request = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  bool stop_observed = false;
  std::stop_callback callback{f.loader.pending.front().context.stop, [&] {
                                stop_observed = true;
                                CHECK(request.cancelled());
                              }};
  REQUIRE(request.cancel());
  CHECK(stop_observed);
  CHECK(f.assets.database().has_active_operations());
  CHECK(f.assets.set_execution_options({f.worker.get_executor(), f.completion.get_executor()})
            .value() == caors::errc::frozen);
  caors::assets::project_config config;
  CHECK(f.assets.build(config).value() == caors::errc::frozen);
  f.loader.finish();
  CHECK(request.cancelled());
  CHECK_FALSE(f.cache.contains(test_guid_a));
  CHECK_FALSE(f.assets.database().has_active_operations());
  CHECK_FALSE(
      f.assets.set_execution_options({f.worker.get_executor(), f.completion.get_executor()}));
}

TEST_CASE(
    "assets - group destruction does not cancel request and runtime rebuild freezes old groups",
    "[assets][native-async]") {
  fixture f;
  auto group = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor());
  auto copy = group;
  auto request = group.request_load<texture>(test_guid_a);
  group = {};
  REQUIRE(f.worker.drain() == 1);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(request.loaded());
  REQUIRE_FALSE(
      f.assets.set_execution_options({f.worker.get_executor(), f.completion.get_executor()}));
  CHECK(copy.request_load<texture>(test_guid_a).loaded());
  caors::assets::project_config config;
  REQUIRE_FALSE(f.assets.build(config));
  CHECK(copy.request_load<texture>(test_guid_a).error().value() == caors::errc::frozen);
  auto renewed = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor());
  CHECK(renewed.request_load<texture>(test_guid_a).loaded());
}

TEST_CASE("assets - async capability wins requests and keeps synchronous capability",
          "[assets][native-async]") {
  fixture f;
  CHECK(f.assets.load_handle<texture>(test_guid_a).status.value() == caors::errc::unsupported);
  counting_texture_loader synchronous;
  REQUIRE_FALSE(f.assets.register_type(f.cache, synchronous, caors::string_name{"group.texture"}));
  auto request = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  CHECK(synchronous.load_count == 0);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(request.loaded());
  CHECK(f.assets.load_handle<texture>(test_guid_b).succeeded());
  CHECK(synchronous.load_count == 1);
  caors::assets::project_config config;
  REQUIRE_FALSE(f.assets.build(config));
  auto reload = f.assets.request_reload<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  f.loader.finish({caors::error_domain::io, caors::errc::not_found});
  CHECK(reload.failed());
  CHECK(f.cache.contains(test_guid_a));
}

TEST_CASE("assets - completion queue discard fails native async request",
          "[assets][native-async]") {
  fixture f;
  auto request = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  f.loader.finish();
  f.completion.clear();
  CHECK(request.failed());
  CHECK(request.error().value() == caors::errc::cancelled);
  CHECK_FALSE(f.cache.contains(test_guid_a));
  CHECK_FALSE(f.assets.database().has_active_operations());
}

TEST_CASE("assets - missing defaults still allow explicit groups", "[assets][native-async]") {
  caors::tasks::execution_context worker, completion;
  caors::resources::cache<texture> cache;
  delayed_loader loader;
  caors::assets::asset_manager assets;
  REQUIRE_FALSE(assets.register_type(cache, loader, caors::string_name{"group.texture"}));
  CHECK(assets.request_load<texture>(test_guid_a).error().value() == caors::errc::invalid_argument);
  CHECK(assets.request_preload(test_guid_a).error().value() == caors::errc::invalid_argument);
  CHECK(caors::assets::asset_group{}.request_load<texture>(test_guid_a).failed());
  auto group = assets.create_group(worker.get_executor(), completion.get_executor());
  auto request = group.request_load<texture>(test_guid_a);
  REQUIRE(worker.drain() == 1);
  loader.finish();
  REQUIRE(completion.drain() == 1);
  CHECK(request.loaded());
}

TEST_CASE("assets - group preload routes asynchronous manifest types", "[assets][native-async]") {
  fixture f;
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_a, "a", caors::string_name{"group.texture"}));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_b, "b", caors::string_name{"group.texture"}));
  REQUIRE(f.assets.dependency_graph().add(test_guid_a, test_guid_b));
  auto group = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor());
  auto batch = GENERATE(false, true) ? group.request_preload(test_guid_a)
                                     : group.request_preload<texture>(test_guid_a);
  CHECK(f.worker.drain() == 2);
  REQUIRE(f.loader.pending.size() == 2);
  f.loader.finish();
  f.loader.finish();
  CHECK(f.completion.drain() == 2);
  CHECK(batch.loaded());
  auto typed = group.request_preload<texture>(test_guid_a);
  CHECK(typed.loaded());
}

TEST_CASE("assets - coroutine waits native async result and synchronous completion",
          "[assets][native-async]") {
  fixture f;
  f.loader.immediate = GENERATE(false, true);
  auto request = f.assets.request_load<texture>(test_guid_a);
  bool resumed = false;
  auto await_result = [&]() -> caors::tasks::async_operation {
    auto result = co_await caors::resources::async_wait(request);
    resumed = result.succeeded();
    co_return result.status;
  };
  auto task = caors::tasks::spawn(caors::tasks::inline_executor(), await_result());
  CHECK_FALSE(task.done());
  REQUIRE(f.worker.drain() == 1);
  if (!f.loader.immediate) {
    f.loader.finish();
  }
  REQUIRE(f.completion.drain() == 1);
  CHECK(resumed);
  CHECK(task.completed());
}

TEST_CASE("assets - native async reload commits a new generation", "[assets][native-async]") {
  fixture f;
  auto group = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor());
  auto first = group.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  auto old = first.try_get();
  auto reloading = group.request_reload<texture>(test_guid_a);
  auto shared = group.request_reload<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(reloading.cancel());
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(shared.loaded());
  CHECK(shared.try_get().get() != old.get());
  CHECK(old);
  CHECK(reloading.cancelled());
}

TEST_CASE("assets - shared cancellation and late progress keep operation alive",
          "[assets][native-async]") {
  fixture f;
  auto first = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  f.loader.pending.front().context.progress.report(0.75, "延迟读取");
  auto second = f.assets.request_load<texture>(test_guid_a);
  CHECK(second.progress() == 0.75);
  REQUIRE(second.cancel_shared());
  CHECK(first.cancelled());
  CHECK(f.loader.pending.front().context.stop.stop_requested());
  CHECK(f.assets.database().has_active_operations());
  f.loader.finish();
  CHECK_FALSE(f.cache.contains(test_guid_a));
  CHECK_FALSE(f.assets.database().has_active_operations());
}

TEST_CASE("assets - mixed preload uses native async and synchronous loaders",
          "[assets][native-async]") {
  fixture f;
  class other_loader final : public caors::resources::loader<observed_resource> {
  public:
    caors::resources::load_result<observed_resource>
    load(caors::resource_guid guid, caors::memory_resource* resource) override {
      return {{}, caors::memory::make_unique<observed_resource>(resource, guid)};
    }
  } synchronous;
  caors::resources::cache<observed_resource> other_cache;
  REQUIRE_FALSE(
      f.assets.register_type(other_cache, synchronous, caors::string_name{"group.other"}));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_a, "a", caors::string_name{"group.texture"}));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_b, "b", caors::string_name{"group.other"}));
  REQUIRE(f.assets.dependency_graph().add(test_guid_a, test_guid_b));
  auto batch = f.assets.request_preload(test_guid_a);
  REQUIRE(f.worker.drain() == 2);
  CHECK_FALSE(batch.done());
  f.loader.finish();
  REQUIRE(f.completion.drain() == 2);
  CHECK(batch.loaded());
  CHECK(other_cache.contains(test_guid_b));
  CHECK(f.cache.contains(test_guid_a));
}

TEST_CASE("assets - completed I/O retains activity through owned progress channel",
          "[assets][native-async]") {
  fixture f;
  auto request = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  auto progress = f.loader.pending.front().context.progress;
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(request.loaded());
  CHECK(f.assets.database().has_active_operations());
  progress.report(1.0, "迟到进度");
  progress = {};
  CHECK_FALSE(f.assets.database().has_active_operations());
}

TEST_CASE("assets - native async completion on another thread is serialized to cache owner",
          "[assets][native-async]") {
  fixture f;
  auto request = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  auto operation = std::move(f.loader.pending.front());
  f.loader.pending.clear();
  std::jthread io{[operation = std::move(operation)]() mutable {
    (void)operation.completion.complete(
        {{},
         caors::memory::make_unique<texture>(operation.context.resource, operation.context.guid)});
  }};
  io.join();
  CHECK_FALSE(f.cache.contains(test_guid_a));
  CHECK_FALSE(request.done());
  REQUIRE(f.completion.drain() == 1);
  CHECK(request.loaded());
}

TEST_CASE("assets - changing defaults keeps existing group scheduling", "[assets][native-async]") {
  fixture f;
  caors::tasks::execution_context other_worker, other_completion;
  auto group = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor());
  REQUIRE_FALSE(f.assets.set_execution_options(
      {other_worker.get_executor(), other_completion.get_executor()}));
  auto group_request = group.request_load<texture>(test_guid_a);
  auto default_request = f.assets.request_load<texture>(test_guid_b);
  REQUIRE(f.worker.drain() == 1);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(group_request.loaded());
  CHECK_FALSE(default_request.done());
  REQUIRE(other_worker.drain() == 1);
  f.loader.finish();
  REQUIRE(other_completion.drain() == 1);
  CHECK(default_request.loaded());
}

TEST_CASE("assets - closed completion preserves frozen error and releases native result",
          "[assets][native-async]") {
  fixture f;
  auto completion = std::make_unique<caors::tasks::execution_context>();
  auto group = f.assets.create_group(f.worker.get_executor(), completion->get_executor());
  auto request = group.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  completion.reset();
  f.loader.finish();
  CHECK(request.error().value() == caors::errc::frozen);
  CHECK_FALSE(f.cache.contains(test_guid_a));
  CHECK_FALSE(f.assets.database().has_active_operations());
}

TEST_CASE("assets - native callback races subscription cancellation without committing",
          "[assets][native-async]") {
  for (int round = 0; round < 32; ++round) {
    fixture f;
    auto request = f.assets.request_load<texture>(test_guid_a);
    REQUIRE(f.worker.drain() == 1);
    auto operation = std::move(f.loader.pending.front());
    f.loader.pending.clear();
    std::jthread io{[operation = std::move(operation)]() mutable {
      (void)operation.completion.complete(
          {{},
           caors::memory::make_unique<texture>(operation.context.resource,
                                               operation.context.guid)});
    }};
    REQUIRE(request.cancel());
    io.join();
    (void)f.completion.drain();
    CHECK(request.cancelled());
    CHECK_FALSE(f.cache.contains(test_guid_a));
    CHECK_FALSE(f.assets.database().has_active_operations());
  }
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("assets - native completion allocation failure preserves out of memory",
          "[assets][native-async]") {
  bool fail_next = false;
  caors::memory::callback_memory_resource memory{
      [](std::size_t size, std::size_t alignment, void* context) -> void* {
        if (std::exchange(*static_cast<bool*>(context), false)) {
          return nullptr;
        }
        return caors::get_system_memory_resource().try_allocate(size, alignment);
      },
      [](void* ptr, std::size_t size, std::size_t alignment, void*) {
        caors::get_system_memory_resource().deallocate(ptr, size, alignment);
      },
      &fail_next};
  caors::tasks::execution_context worker, completion;
  caors::resources::cache<texture> cache{&memory};
  delayed_loader source;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}, &memory};
  REQUIRE_FALSE(assets.register_type(cache, source, caors::string_name{"group.texture"}));
  auto request = assets.request_load<texture>(test_guid_a);
  fail_next = true;
  REQUIRE(worker.drain() == 1);
  CHECK(request.failed());
  CHECK(request.error().value() == caors::errc::out_of_memory);
  CHECK(source.starts == 0);
  CHECK_FALSE(assets.database().has_active_operations());
}
#endif

TEST_CASE("assets - terminal callback may release native completion owner",
          "[assets][native-async]") {
  fixture f;
  REQUIRE_FALSE(
      f.assets.set_execution_options({f.worker.get_executor(), caors::tasks::inline_executor()}));
  auto request = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  auto await_result = [&]() -> caors::tasks::async_operation {
    auto result = co_await caors::resources::async_wait(request);
    f.loader.pending.clear();
    co_return result.status;
  };
  auto task = caors::tasks::spawn(caors::tasks::inline_executor(), await_result());
  REQUIRE(f.loader.pending.front().completion.complete(
      {{}, caors::memory::make_unique<texture>(nullptr, test_guid_a)}));
  CHECK(task.completed());
  CHECK(f.loader.pending.empty());
  CHECK_FALSE(f.assets.database().has_active_operations());
}

TEST_CASE("assets - generic owned type factory accepts native async loader",
          "[assets][native-async]") {
  class factory_loader final : public caors::resources::async_loader<texture> {
  public:
    explicit factory_loader(const caors::resources::provider&) {}
    caors::error start(caors::resources::async_load_context context,
                       caors::resources::load_completion<texture> completion) override {
      (void)completion.complete(
          {{}, caors::memory::make_unique<texture>(context.resource, context.guid)});
      return {};
    }
  };
  caors::tasks::execution_context worker, completion;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  caors::assets::project_config config;
  config.resource_types.push_back({caors::string_name{"group.factory"},
                                   caors::string_name{"native"},
                                   caors::string_name{"memory"},
                                   {}});
  REQUIRE_FALSE(caors::assets::register_cached_type_factory<texture, factory_loader>(
      assets, caors::string_name{"group.factory"}, caors::string_name{"memory"},
      caors::string_name{"native"}));
  REQUIRE_FALSE(assets.build(config));
  auto group = assets.create_group(worker.get_executor(), completion.get_executor());
  auto request = group.request_load<texture>(test_guid_a);
  REQUIRE(worker.drain() == 1);
  REQUIRE(completion.drain() == 1);
  CHECK(request.loaded());
  CHECK(assets.load_handle<texture>(test_guid_b).status.value() == caors::errc::unsupported);
}
