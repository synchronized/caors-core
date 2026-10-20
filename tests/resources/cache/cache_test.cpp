// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_cache_cache_creates_and_indexes_resources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache creates and indexes resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  texture_resource* texture = cache.create(test_guid_a);
  REQUIRE(texture != nullptr);

  CHECK(cache.size() == 1);
  CHECK(cache.contains(test_guid_a));
  CHECK(texture->guid() == test_guid_a);
  CHECK(cache.find(test_guid_a) == texture);
  CHECK(cache.find_as<texture_resource>(test_guid_a) == texture);
  CHECK(cache.index().find(test_guid_a) == texture);
}

// 原测试文件：resources_cache_cache_erase_destroys_owned_resource_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache erase destroys owned resource", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.erase(test_guid_a));

  CHECK(observed_resource::destroyed == 1);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK_FALSE(cache.erase(test_guid_a));
}

// 原测试文件：resources_cache_cache_handles_keep_resources_alive_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

namespace {

class handled_resource : public caors::resource {
public:
  using caors::resource::resource;

  ~handled_resource() override { ++destroyed; }

  int value = 0;
  inline static int destroyed = 0;
};

class handled_resource_loader final : public caors::resources::loader<handled_resource> {
public:
  [[nodiscard]] caors::resources::load_result<handled_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    ++load_count;
    auto loaded = caors::memory::make_unique<handled_resource>(resource, guid);
    loaded->value = value;
    return {{}, std::move(loaded)};
  }

  int value = 0;
  int load_count = 0;
};

} // namespace

TEST_CASE("resources - cache handles keep erased resources alive", "[resources]") {
  handled_resource::destroyed = 0;

  caors::resource_cache<handled_resource> cache;
  caors::resource_handle<handled_resource> held = cache.create_handle(test_guid_a);
  REQUIRE(held != nullptr);
  CHECK(held.use_count() == 2);

  CHECK(cache.erase(test_guid_a));

  CHECK(cache.find(test_guid_a) == nullptr);
  CHECK(held->guid() == test_guid_a);
  CHECK(held.use_count() == 1);
  CHECK(handled_resource::destroyed == 0);

  held.reset();
  CHECK(handled_resource::destroyed == 1);
}

TEST_CASE("resources - load_handle_into returns cache-owned handles", "[resources]") {
  caors::resource_cache<handled_resource> cache;
  handled_resource_loader loader;
  loader.value = 11;

  caors::resources::handle_result<handled_resource> loaded =
      caors::resources::load_handle_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->value == 11);
  CHECK(loaded.value.get() == cache.find_handle(test_guid_a).get());
  CHECK(loaded.value.use_count() == 2);
  CHECK(loader.load_count == 1);

  caors::resources::handle_result<handled_resource> cached =
      caors::resources::load_handle_into(cache, loader, test_guid_a);

  REQUIRE(cached.succeeded());
  CHECK(cached.value.get() == loaded.value.get());
  CHECK(loader.load_count == 1);
}

TEST_CASE("resources - reload_handle_into keeps old external handles alive", "[resources]") {
  handled_resource::destroyed = 0;

  caors::resource_cache<handled_resource> cache;
  caors::resource_handle<handled_resource> old_handle = cache.create_handle(test_guid_a);
  REQUIRE(old_handle != nullptr);
  old_handle->value = 1;
  handled_resource* old_address = old_handle.get();

  handled_resource_loader loader;
  loader.value = 2;

  caors::resources::handle_result<handled_resource> reloaded =
      caors::resources::reload_handle_into(cache, loader, test_guid_a);

  REQUIRE(reloaded.succeeded());
  REQUIRE(reloaded.value != nullptr);
  CHECK(reloaded.value->value == 2);
  CHECK(reloaded.value.get() == cache.find(test_guid_a));
  CHECK(reloaded.value.get() != old_address);
  CHECK(old_handle->value == 1);
  CHECK(handled_resource::destroyed == 0);

  old_handle.reset();
  CHECK(handled_resource::destroyed == 1);

  reloaded.value.reset();
  CHECK(handled_resource::destroyed == 1);

  CHECK(cache.erase(test_guid_a));
  CHECK(handled_resource::destroyed == 2);
}

// 原测试文件：resources_cache_cache_inserts_owned_resources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache inserts owned resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  auto texture = caors::memory::make_unique<texture_resource>(nullptr, test_guid_a);
  texture_resource* stored = texture.get();

  REQUIRE(cache.insert(std::move(texture)));

  CHECK(cache.size() == 1);
  CHECK(cache.find(test_guid_a) == stored);
  CHECK(cache.index().find(test_guid_a) == stored);
  CHECK_FALSE(cache.insert({}));
}

// 原测试文件：resources_cache_cache_marks_refs_stale_after_replace_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache marks resolved refs stale after replace", "[resources]") {
  caors::resource_cache<texture_resource> cache;
  texture_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);

  caors::resource_ref<texture_resource> reference{test_guid_a};
  REQUIRE(cache.resolve(reference));
  CHECK(reference.get() == initial);
  CHECK(cache.is_current(reference));

  const caors::resource_generation first_generation = reference.generation();

  auto replacement = caors::memory::make_unique<texture_resource>(cache.resource(), test_guid_a);
  texture_resource* replacement_ptr = replacement.get();
  REQUIRE(cache.replace(std::move(replacement)));

  CHECK_FALSE(cache.is_current(reference));
  CHECK(reference.generation() == first_generation);

  REQUIRE(cache.resolve(reference));
  CHECK(reference.get() == replacement_ptr);
  CHECK(reference.generation() != first_generation);
  CHECK(cache.is_current(reference));
}

// 原测试文件：resources_cache_cache_rejects_invalid_duplicate_and_mismatched_resources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache rejects invalid duplicate and mismatched resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  CHECK(cache.create(caors::resource_guid::nil()) == nullptr);
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.create(test_guid_a) == nullptr);
  CHECK(cache.create(test_guid_b, test_guid_a) == nullptr);
  CHECK_FALSE(cache.contains(test_guid_b));
}

// 原测试文件：resources_cache_cache_release_transfers_ownership_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache release transfers ownership", "[resources]") {
  observed_resource::destroyed = 0;

  caors::resource_cache<observed_resource> cache;
  REQUIRE(cache.create(test_guid_a) != nullptr);

  auto released = cache.release(test_guid_a);

  REQUIRE(released != nullptr);
  CHECK(cache.empty());
  CHECK(cache.index().find(test_guid_a) == nullptr);
  CHECK(released->guid() == test_guid_a);
  CHECK(observed_resource::destroyed == 0);

  released.reset();
  CHECK(observed_resource::destroyed == 1);
}

// 原测试文件：resources_cache_cache_resolves_refs_through_internal_manager_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache resolves refs through internal manager", "[resources]") {
  caors::resource_cache<image_texture_resource> cache;
  image_texture_resource* image = cache.create(test_guid_a);
  REQUIRE(image != nullptr);

  caors::resource_ref<texture_resource> base_ref{test_guid_a};
  REQUIRE(cache.resolve(base_ref));

  CHECK(base_ref.resolved());
  CHECK(base_ref.get() == image);

  caors::resources::ref<image_texture_resource> derived_ref{test_guid_a};
  REQUIRE(cache.resolve(derived_ref));
  CHECK(derived_ref.get() == image);
}

// 原测试文件：resources_cache_cache_uses_requested_memory_resource_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - cache uses requested memory resource", "[resources]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::resource_cache<texture_resource> cache{&tracking};

  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.resource() == &tracking);
  CHECK(cache.index().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count > 0);
}

namespace {
class cache_failure_resource final : public caors::memory_resource {
public:
  int remaining = -1;

private:
  void* do_try_allocate(std::size_t bytes, std::size_t alignment) noexcept override {
    // MSVC Debug 容器代理的 noexcept 小型分配不属于本专项注入范围。
    if (bytes > 32 && remaining >= 0 && remaining-- == 0)
      return nullptr;
    return caors::get_system_memory_resource().try_allocate(bytes, alignment);
  }
  void do_deallocate(void* value, std::size_t bytes, std::size_t alignment) noexcept override {
    caors::get_system_memory_resource().deallocate(value, bytes, alignment);
  }
  bool do_is_equal(const caors::memory_resource& other) const noexcept override {
    return this == &other;
  }
};
}
TEST_CASE("resources - cache publication allocation failures leave both indices consistent",
          "[resources][refresh][memory]") {
  for (int failure = 0; failure < 10; ++failure) {
    cache_failure_resource memory;
    caors::resources::cache<texture_resource> cache{&memory};
    auto value = caors::resources::make_handle<texture_resource>(nullptr, test_guid_a);
    memory.remaining = failure;
    bool inserted = false;
    try {
      inserted = cache.insert(value);
    } catch (const std::bad_alloc&) {
    }
    memory.remaining = -1;
    CHECK(cache.contains(test_guid_a) == inserted);
    CHECK(cache.index().contains(test_guid_a) == inserted);
    if (!inserted) {
      CHECK_FALSE(cache.index().contains(value.get()));
      CHECK(cache.insert(value));
    }
    auto original = cache.find_handle(test_guid_a);
    const auto generation = cache.generation_of(test_guid_a);
    auto replacement = caors::resources::make_handle<texture_resource>(nullptr, test_guid_a);
    memory.remaining = failure;
    bool replaced = false;
    try {
      replaced = cache.replace(replacement);
    } catch (const std::bad_alloc&) {
    }
    memory.remaining = -1;
    auto* expected = replaced ? replacement.get() : original.get();
    CHECK(cache.find(test_guid_a) == expected);
    CHECK(cache.index().find(test_guid_a) == expected);
    CHECK(cache.index().contains(original.get()) == !replaced);
    if (!replaced)
      CHECK(cache.generation_of(test_guid_a) == generation);
  }
}
