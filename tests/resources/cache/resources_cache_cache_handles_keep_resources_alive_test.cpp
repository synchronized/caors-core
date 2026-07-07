// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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

class handled_resource_loader final : public caors::resource_loader<handled_resource> {
public:
  [[nodiscard]] caors::resource_load_result<handled_resource>
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

  caors::resource_handle_result<handled_resource> loaded =
      caors::load_handle_into(cache, loader, test_guid_a);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value != nullptr);
  CHECK(loaded.value->value == 11);
  CHECK(loaded.value.get() == cache.find_handle(test_guid_a).get());
  CHECK(loaded.value.use_count() == 2);
  CHECK(loader.load_count == 1);

  caors::resource_handle_result<handled_resource> cached =
      caors::load_handle_into(cache, loader, test_guid_a);

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

  caors::resource_handle_result<handled_resource> reloaded =
      caors::reload_handle_into(cache, loader, test_guid_a);

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
