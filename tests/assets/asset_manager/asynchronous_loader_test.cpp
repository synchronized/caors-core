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

class delayed_loader final : public caors::resources::loader<texture> {
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
  CHECK(caors::assets::project_builder{f.assets}.build(config).value() == caors::errc::frozen);
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
  REQUIRE_FALSE(caors::assets::project_builder{f.assets}.build(config));
  CHECK(copy.request_load<texture>(test_guid_a).error().value() == caors::errc::frozen);
  auto renewed = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor());
  CHECK(renewed.request_load<texture>(test_guid_a).loaded());
}

TEST_CASE("assets - async capability wins requests and keeps synchronous capability",
          "[assets][native-async]") {
  fixture f;
  CHECK(f.assets.load_handle<texture>({}).status.value() == caors::errc::invalid_argument);
  CHECK(f.assets.reload_handle<texture>({}).status.value() == caors::errc::invalid_argument);
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
  REQUIRE_FALSE(caors::assets::project_builder{f.assets}.build(config));
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
  class other_loader final : public caors::resources::sync_loader_adapter<observed_resource> {
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

#if CAORS_HAS_BACKGROUND_THREADS
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

#endif

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

#if CAORS_HAS_BACKGROUND_THREADS
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
#endif

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
  class factory_loader final : public caors::resources::loader<texture> {
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
      caors::assets::project_builder{assets}, caors::string_name{"group.factory"},
      caors::string_name{"memory"}, caors::string_name{"native"}));
  REQUIRE_FALSE(caors::assets::project_builder{assets}.build(config));
  auto group = assets.create_group(worker.get_executor(), completion.get_executor());
  auto request = group.request_load<texture>(test_guid_a);
  REQUIRE(worker.drain() == 1);
  REQUIRE(completion.drain() == 1);
  CHECK(request.loaded());
  CHECK(assets.load_handle<texture>(test_guid_b).status.value() == caors::errc::unsupported);
}

TEST_CASE("assets - preload cancellation and abandonment signal only unused native operations",
          "[assets][native-async]") {
  for (bool drop_batch : {false, true}) {
    fixture f;
    REQUIRE_FALSE(f.assets.manifest().bind(test_guid_a, "a", caors::string_name{"group.texture"}));
    auto first = f.assets.request_preload(test_guid_a);
    auto second = f.assets.request_preload(test_guid_a);
    REQUIRE(f.worker.drain() == 1);
    if (drop_batch) {
      first = {};
    } else {
      REQUIRE(first.cancel());
    }
    CHECK_FALSE(f.loader.pending.front().context.stop.stop_requested());
    REQUIRE(second.cancel());
    CHECK(f.loader.pending.front().context.stop.stop_requested());
    f.loader.finish();
    CHECK_FALSE(f.cache.contains(test_guid_a));
    CHECK_FALSE(f.assets.database().has_active_operations());
  }
}

TEST_CASE("assets - preload failure cancels remaining I/O without cancelling standalone demand",
          "[assets][native-async]") {
  fixture f;
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_a, "a", caors::string_name{"group.texture"}));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_b, "b", caors::string_name{"group.texture"}));
  REQUIRE(f.assets.dependency_graph().add(test_guid_a, test_guid_b));
  auto batch = f.assets.request_preload(test_guid_a);
  auto independent = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 2);
  f.loader.finish({caors::error_domain::io, caors::errc::not_found});
  CHECK(batch.failed());
  REQUIRE(f.loader.pending.size() == 1);
  CHECK_FALSE(f.loader.pending.front().context.stop.stop_requested());
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(independent.loaded());
}

namespace {
std::string snapshot_text(const caors::resources::async_load_context& context,
                          caors::resource_guid guid) {
  auto sources = context.snapshot_as<caors::resources::provider_snapshot>();
  REQUIRE(sources);
  auto file = sources->resolve(guid);
  REQUIRE(file);
  auto bytes = file->backend->read_all(file->path);
  REQUIRE(bytes);
  return {reinterpret_cast<const char*>(bytes->data()), bytes->size()};
}
std::shared_ptr<caors::io::memory_file_system> snapshot_files(std::string_view text) {
  auto files = std::make_shared<caors::io::memory_file_system>();
  REQUIRE_FALSE(files->write_all("data", text));
  return files;
}
}
TEST_CASE("assets - first GUID operation freezes source before worker and remount",
          "[assets][vfs]") {
  fixture f;
  auto router = std::make_shared<caors::io::mount_file_system>();
  REQUIRE_FALSE(router->mount("assets", snapshot_files("old")));
  caors::resources::file_source source{router};
  REQUIRE_FALSE(
      caors::assets::project_builder{f.assets}.provider().bind(test_guid_a, source, "assets/data"));
  auto first = f.assets.request_load<texture>(test_guid_a);
  router->clear();
  REQUIRE_FALSE(router->mount("assets", snapshot_files("new")));
  auto shared = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 1);
  CHECK(snapshot_text(f.loader.pending.front().context, test_guid_a) == "old");
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(first.loaded());
  CHECK(shared.loaded());
  auto reloaded = f.assets.request_reload<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  CHECK(snapshot_text(f.loader.pending.front().context, test_guid_a) == "new");
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(reloaded.loaded());
}
TEST_CASE("assets - dependency preload shares one owning provider snapshot", "[assets][vfs]") {
  fixture f;
  auto router = std::make_shared<caors::io::mount_file_system>();
  REQUIRE_FALSE(router->mount("assets", snapshot_files("old")));
  caors::resources::file_source source{router};
  REQUIRE_FALSE(
      caors::assets::project_builder{f.assets}.provider().bind(test_guid_a, source, "assets/data"));
  REQUIRE_FALSE(
      caors::assets::project_builder{f.assets}.provider().bind(test_guid_b, source, "assets/data"));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_a, "a", caors::string_name{"group.texture"}));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_b, "b", caors::string_name{"group.texture"}));
  REQUIRE(f.assets.dependency_graph().add(test_guid_a, test_guid_b));
  auto batch = GENERATE(false, true) ? f.assets.request_preload(test_guid_a)
                                     : f.assets.request_preload<texture>(test_guid_a);
  REQUIRE(f.worker.poll_one() == 1);
  router->clear();
  REQUIRE_FALSE(router->mount("assets", snapshot_files("new")));
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 2);
  CHECK(f.loader.pending[0].context.snapshot == f.loader.pending[1].context.snapshot);
  for (auto& op : f.loader.pending)
    CHECK(snapshot_text(op.context, op.context.guid) == "old");
  caors::assets::project_builder{f.assets}.provider().clear();
  CHECK(snapshot_text(f.loader.pending.front().context, test_guid_a) == "old");
  f.loader.finish();
  f.loader.finish();
  REQUIRE(f.completion.drain() == 2);
  CHECK(batch.loaded());
}

TEST_CASE("assets - configured builtins expose owning snapshots after manager destruction",
          "[assets][vfs]") {
  std::shared_ptr<const caors::resources::provider_snapshot> snapshot;
  {
    caors::assets::asset_manager assets;
    caors::assets::project_config config;
    auto json = R"({"file_systems":[{"name":"memory","type":"memory_file_system"},
      {"name":"vfs","type":"resolver","routes":[{"prefix":"caors://assets","target":"memory"}]}],
      "sources":[{"name":"assets","file_system":"vfs"}]})";
    REQUIRE_FALSE(caors::assets::json::load_project_config(config, json));
    REQUIRE_FALSE(caors::assets::project_builder{assets}.build(config));
    auto* memory =
        caors::assets::project_builder{assets}.find_file_system(caors::string_name{"memory"});
    REQUIRE(memory);
    REQUIRE_FALSE(memory->write_all("data", std::string_view{"factory"}));
    auto* source = caors::assets::project_builder{assets}.find_source(caors::string_name{"assets"});
    REQUIRE(source);
    REQUIRE_FALSE(caors::assets::project_builder{assets}.provider().bind(test_guid_a, *source,
                                                                         "caors://assets/data"));
    snapshot = caors::assets::project_builder{assets}.provider().snapshot();
  }
  auto file = snapshot->resolve(test_guid_a);
  REQUIRE(file);
  auto opened = caors::io::request_open(std::move(file.value), caors::tasks::inline_executor());
  REQUIRE(opened);
  REQUIRE(opened->result());
  REQUIRE(*opened->result());
  auto stream = opened->result()->value;
  auto read = caors::io::read_all_bounded(stream, 7, caors::tasks::inline_executor());
  REQUIRE(read);
  REQUIRE(read->result());
  REQUIRE(*read->result());
  CHECK(read->result()->value.size() == 7);
  auto closed = caors::io::request_close(stream, caors::tasks::inline_executor());
  REQUIRE(closed);
  REQUIRE(closed->result());
  CHECK(*closed->result());
}
TEST_CASE("assets - configured owning route cycles fail before publishing edges", "[assets][vfs]") {
  caors::assets::asset_manager assets;
  caors::assets::project_config config;
  REQUIRE_FALSE(caors::assets::json::load_project_config(config, R"({"file_systems":[
    {"name":"a","type":"mount_file_system","routes":[{"prefix":"a","target":"b"}]},
    {"name":"b","type":"mount_file_system","routes":[{"prefix":"b","target":"a"}]}]})"));
  CHECK(caors::assets::project_builder{assets}.build(config).value() ==
        caors::errc::cycle_detected);
}

TEST_CASE("assets - refresh chooses new source while old load remains independently usable",
          "[assets][refresh]") {
  fixture f;
  auto router = std::make_shared<caors::io::mount_file_system>();
  REQUIRE_FALSE(router->mount("assets", snapshot_files("old")));
  caors::resources::file_source source{router};
  REQUIRE_FALSE(
      caors::assets::project_builder{f.assets}.provider().bind(test_guid_a, source, "assets/data"));
  auto old = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(router->unmount("assets"));
  REQUIRE_FALSE(router->mount("assets", snapshot_files("new")));
  auto fresh = f.assets.create_group(f.worker.get_executor(), f.completion.get_executor())
                   .request_refresh<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 2);
  CHECK(snapshot_text(f.loader.pending[0].context, test_guid_a) == "old");
  CHECK(snapshot_text(f.loader.pending[1].context, test_guid_a) == "new");
  auto joined = f.assets.request_load<texture>(test_guid_a);
  CHECK(f.worker.pending() == 0);
  std::swap(f.loader.pending[0], f.loader.pending[1]);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  REQUIRE(fresh.loaded());
  REQUIRE(joined.loaded());
  CHECK(fresh.try_get().get() == joined.try_get().get());
  auto published = fresh.try_get();
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  REQUIRE(old.loaded());
  CHECK(old.try_get().get() != published.get());
  CHECK(f.cache.find(test_guid_a) == published.get());
}

TEST_CASE("assets - failed latest refresh never restores an older publication intent",
          "[assets][refresh]") {
  fixture f;
  auto original = f.cache.create_handle(test_guid_a, test_guid_a);
  REQUIRE(original);
  auto older = f.assets.request_refresh<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  auto latest = f.assets.request_refresh<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 2);
  CHECK(f.assets.request_load<texture>(test_guid_a).try_get().get() == original.get());
  std::swap(f.loader.pending[0], f.loader.pending[1]);
  f.loader.finish({caors::error_domain::io, caors::errc::not_found});
  REQUIRE(latest.failed());
  CHECK(f.cache.find(test_guid_a) == original.get());
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  REQUIRE(older.loaded());
  CHECK(older.try_get().get() != original.get());
  CHECK(f.cache.find(test_guid_a) == original.get());
  auto retry = f.assets.request_refresh<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  REQUIRE(retry.loaded());
  CHECK(f.cache.find(test_guid_a) == retry.try_get().get());
  CHECK(original->guid() == test_guid_a);
}

TEST_CASE("assets - refresh supersedes an already queued reload commit", "[assets][refresh]") {
  fixture f;
  auto original = f.cache.create_handle(test_guid_a, test_guid_a);
  auto old = f.assets.request_reload<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  f.loader.finish();
  REQUIRE(f.completion.pending() == 1);
  auto fresh = f.assets.request_refresh<texture>(test_guid_a);
  REQUIRE(f.completion.drain() == 1);
  REQUIRE(old.loaded());
  CHECK(f.cache.find(test_guid_a) == original.get());
  REQUIRE(f.worker.drain() == 1);
  auto shared = f.assets.request_reload<texture>(test_guid_a);
  CHECK(f.worker.pending() == 0);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  REQUIRE(shared.loaded());
  CHECK(shared.try_get().get() == fresh.try_get().get());
  CHECK(f.cache.find(test_guid_a) == fresh.try_get().get());
}

TEST_CASE("assets - rejected and cancelled refresh preserve cache and supersede older work",
          "[assets][refresh]") {
  for (int mode : {0, 1, 2}) {
    fixture f;
    auto original = f.cache.create_handle(test_guid_a, test_guid_a);
    auto older = f.assets.request_refresh<texture>(test_guid_a);
    REQUIRE(f.worker.drain() == 1);
    caors::tasks::execution_context worker;
    caors::tasks::execution_context completion;
    auto latest = f.assets.request_refresh<texture>(
        test_guid_a, mode == 0 ? caors::tasks::executor{} : worker.get_executor(),
        completion.get_executor());
    if (mode == 0) {
      CHECK(latest.failed());
    } else {
      REQUIRE(worker.drain() == 1);
      REQUIRE(f.loader.pending.size() == 2);
      std::swap(f.loader.pending[0], f.loader.pending[1]);
      if (mode == 1)
        REQUIRE(latest.cancel());
      f.loader.finish();
      if (mode == 2)
        completion.clear();
      else
        (void)completion.drain();
      CHECK(latest.done());
    }
    f.loader.finish();
    REQUIRE(f.completion.drain() == 1);
    REQUIRE(older.loaded());
    CHECK(f.cache.find(test_guid_a) == original.get());
  }
}

TEST_CASE("assets - refresh root does not silently refresh cached dependency closure",
          "[assets][refresh]") {
  fixture f;
  auto old_root = f.cache.create_handle(test_guid_a, test_guid_a);
  auto dependency = f.cache.create_handle(test_guid_b, test_guid_b);
  REQUIRE(old_root);
  REQUIRE(dependency);
  REQUIRE(f.assets.dependency_graph().add(test_guid_a, test_guid_b));
  auto request = f.assets.request_refresh<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 1);
  CHECK(f.loader.pending.front().context.guid == test_guid_a);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  REQUIRE(request.loaded());
  CHECK(f.cache.find(test_guid_a) != old_root.get());
  CHECK(f.cache.find(test_guid_b) == dependency.get());
  CHECK(f.loader.starts == 1);
}

namespace {
struct host_snapshot final : caors::resources::load_snapshot {
  explicit host_snapshot(int value) : revision(value) {}
  const int revision;
};
} // namespace

TEST_CASE("assets - load pipeline retains a typed host snapshot without VFS",
          "[assets][native-async][snapshot]") {
  fixture f;
  auto snapshot = std::make_shared<host_snapshot>(7);
  std::weak_ptr<const host_snapshot> lifetime = snapshot;
  // 直接验证内部批次传递边界，不把原型注入入口作为新的公共 manager API。
  auto request = caors::resources::detail::request_load_into_impl<texture>(
      f.cache, f.loader, test_guid_a, f.worker.get_executor(), f.completion.get_executor(), nullptr,
      true, {}, {}, {}, snapshot);
  snapshot.reset();
  CHECK_FALSE(lifetime.expired());
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 1);
  const auto& context = f.loader.pending.front().context;
  auto host = context.snapshot_as<host_snapshot>();
  REQUIRE(host);
  CHECK(host->revision == 7);
  CHECK_FALSE(context.snapshot_as<caors::resources::provider_snapshot>());
  host.reset();
  f.loader.finish();
  CHECK(lifetime.expired());
  REQUIRE(f.completion.drain() == 1);
  CHECK(request.loaded());
}

TEST_CASE("assets - absent batch data has no implicit VFS requirement",
          "[assets][native-async][snapshot]") {
  caors::resources::async_load_context context;
  CHECK_FALSE(context.snapshot_as<host_snapshot>());
  CHECK_FALSE(context.snapshot_as<caors::resources::provider_snapshot>());
}

TEST_CASE("assets - host capture shares preload batch and refreshes only new operations",
          "[assets][native-async][snapshot]") {
  fixture f;
  int captures = 0;
  int revision = 1;
  REQUIRE_FALSE(f.assets.database().set_snapshot_capture([&] {
    ++captures;
    return std::make_shared<host_snapshot>(revision);
  }));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_a, "a", caors::string_name{"group.texture"}));
  REQUIRE_FALSE(f.assets.manifest().bind(test_guid_b, "b", caors::string_name{"group.texture"}));
  REQUIRE(f.assets.dependency_graph().add(test_guid_a, test_guid_b));
  auto batch = GENERATE(false, true) ? f.assets.request_preload(test_guid_a)
                                     : f.assets.request_preload<texture>(test_guid_a);
  CHECK(captures == 1);
  revision = 2;
  auto subscriber = f.assets.request_load<texture>(test_guid_a);
  CHECK(captures == 1);
  CHECK(f.assets.database().set_snapshot_capture({}) ==
        caors::error{caors::error_domain::resources, caors::errc::frozen});
  REQUIRE(f.worker.drain() == 2);
  REQUIRE(f.loader.pending.size() == 2);
  CHECK(f.loader.pending[0].context.snapshot == f.loader.pending[1].context.snapshot);
  for (const auto& operation : f.loader.pending) {
    auto host = operation.context.snapshot_as<host_snapshot>();
    REQUIRE(host);
    CHECK(host->revision == 1);
    CHECK_FALSE(operation.context.snapshot_as<caors::resources::provider_snapshot>());
  }
  f.loader.finish();
  f.loader.finish();
  REQUIRE(f.completion.drain() == 2);
  CHECK(batch.loaded());
  CHECK(subscriber.loaded());
  auto cached = f.assets.request_load<texture>(test_guid_a);
  CHECK(cached.loaded());
  CHECK(captures == 1);
  auto refreshed = f.assets.request_refresh<texture>(test_guid_a);
  CHECK(captures == 2);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.front().context.snapshot_as<host_snapshot>());
  CHECK(f.loader.pending.front().context.snapshot_as<host_snapshot>()->revision == 2);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(refreshed.loaded());
  REQUIRE_FALSE(f.assets.database().set_snapshot_capture({}));
}

TEST_CASE("assets - explicit empty host capture does not fall back to VFS",
          "[assets][native-async][snapshot]") {
  fixture f;
  if (GENERATE(false, true)) {
    REQUIRE_FALSE(f.assets.database().set_snapshot_capture({}));
  } else {
    REQUIRE_FALSE(f.assets.database().set_snapshot_capture(
        [] { return std::shared_ptr<const caors::resources::load_snapshot>{}; }));
  }
  auto request = f.assets.request_load<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 1);
  CHECK_FALSE(f.loader.pending.front().context.snapshot);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(request.loaded());
  REQUIRE_FALSE(caors::assets::project_builder{f.assets}.use_provider_snapshot());
  auto reload = f.assets.request_reload<texture>(test_guid_a);
  REQUIRE(f.worker.drain() == 1);
  CHECK(f.loader.pending.front().context.snapshot_as<caors::resources::provider_snapshot>());
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(reload.loaded());
}

TEST_CASE("resources - standalone database loads without a provider or capture strategy",
          "[resources][native-async][snapshot]") {
  fixture f;
  caors::resources::database database;
  REQUIRE_FALSE(database.register_type(f.cache, f.loader, caors::string_name{"bare.texture"}));
  auto request = database.request_load<texture>(test_guid_a, f.worker.get_executor(),
                                                f.completion.get_executor());
  REQUIRE(f.worker.drain() == 1);
  REQUIRE(f.loader.pending.size() == 1);
  CHECK_FALSE(f.loader.pending.front().context.snapshot);
  f.loader.finish();
  REQUIRE(f.completion.drain() == 1);
  CHECK(request.loaded());
  CHECK_FALSE(database.has_active_operations());
}

namespace {
struct loader_prefix {
  virtual ~loader_prefix() = default;
  int marker = 9;
};
class offset_sync_loader final : public loader_prefix,
                                 public caors::resources::sync_loader_adapter<texture> {
public:
  caors::resources::load_result<texture> load(caors::resource_guid guid,
                                              caors::memory_resource* resource = nullptr) override {
    ++calls;
    return {{}, caors::memory::make_unique<texture>(resource, guid)};
  }
  int calls = 0;
};
template <typename T>
concept has_synchronous_load = requires(T& value) { value.load(test_guid_a); };
static_assert(!has_synchronous_load<caors::resources::loader<texture>>);
static_assert(has_synchronous_load<caors::resources::sync_loader_adapter<texture>>);
} // namespace

TEST_CASE("assets - synchronous adapter preserves adjusted loader pointers across rebuild",
          "[assets][native-async][adapter]") {
  caors::tasks::execution_context worker;
  caors::tasks::execution_context completion;
  caors::resources::cache<texture> cache;
  offset_sync_loader loader;
  caors::assets::asset_manager assets{{worker.get_executor(), completion.get_executor()}};
  REQUIRE_FALSE(assets.register_type(cache, loader, caors::string_name{"adapter.texture"}));
  REQUIRE_FALSE(caors::assets::project_builder{assets}.build(caors::assets::project_config{}));
  auto request = assets.request_load<texture>(test_guid_a);
  CHECK(loader.calls == 0);
  REQUIRE(worker.drain() == 1);
  CHECK(loader.calls == 1);
  CHECK_FALSE(request.done());
  REQUIRE(completion.drain() == 1);
  CHECK(request.loaded());
  CHECK(assets.load_handle<texture>(test_guid_b).succeeded());
  CHECK(loader.calls == 2);
}
