// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_cache_reload_replaces_existing_resources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

namespace {

class reload_resource : public caors::resource {
public:
  using caors::resource::resource;

  ~reload_resource() override { ++destroyed; }

  int width = 0;
  inline static int destroyed = 0;
};

class reload_resource_loader final : public caors::resources::loader<reload_resource> {
public:
  [[nodiscard]] caors::resources::load_result<reload_resource>
  load(caors::resource_guid guid, caors::memory_resource* resource = nullptr) override {
    ++load_count;
    if (status.failed()) {
      return {status, {}};
    }

    auto loaded = caors::memory::make_unique<reload_resource>(resource);
    loaded->set_guid(forced_guid ? forced_guid : guid);
    loaded->width = width;
    return {{}, std::move(loaded)};
  }

  int width = 0;
  int load_count = 0;
  caors::resource_guid forced_guid{};
  caors::error status{};
};

} // namespace

TEST_CASE("resources - cache can replace existing resources", "[resources]") {
  reload_resource::destroyed = 0;

  caors::resource_cache<reload_resource> cache;
  reload_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->width = 128;

  auto replacement = caors::memory::make_unique<reload_resource>(cache.resource());
  replacement->set_guid(test_guid_a);
  replacement->width = 256;

  CHECK(cache.replace(std::move(replacement)));

  reload_resource* current = cache.find(test_guid_a);
  REQUIRE(current != nullptr);
  CHECK(current->width == 256);
  CHECK(cache.index().find(test_guid_a) == current);
  CHECK(reload_resource::destroyed == 1);
}

TEST_CASE("resources - reload_handle_into replaces existing cache entries", "[resources]") {
  reload_resource::destroyed = 0;

  caors::resource_cache<reload_resource> cache;
  reload_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->width = 128;

  caors::resource_ref<reload_resource> reference{test_guid_a};
  REQUIRE(cache.resolve(reference));
  CHECK(reference.get() == initial);

  reload_resource_loader loader;
  loader.width = 512;

  caors::resources::handle_result<reload_resource> reloaded =
      caors::resources::reload_handle_into(cache, loader, test_guid_a);

  REQUIRE(reloaded.succeeded());
  REQUIRE(reloaded.value);
  CHECK(loader.load_count == 1);
  CHECK(reloaded.value.get() == cache.find(test_guid_a));
  CHECK(reloaded.value->width == 512);
  CHECK(reload_resource::destroyed == 1);

  REQUIRE(cache.resolve(reference));
  CHECK(reference.get() == reloaded.value.get());
}

TEST_CASE("resources - reload_handle_into keeps old cache entry when loading fails",
          "[resources]") {
  reload_resource::destroyed = 0;

  caors::resource_cache<reload_resource> cache;
  reload_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->width = 128;

  reload_resource_loader loader;
  loader.status = {caors::error_domain::resources, caors::errc::backend_failure};

  caors::resources::handle_result<reload_resource> reloaded =
      caors::resources::reload_handle_into(cache, loader, test_guid_a);

  CHECK(reloaded.failed());
  CHECK(reloaded.status ==
        caors::error{caors::error_domain::resources, caors::errc::backend_failure});
  CHECK(cache.find(test_guid_a) == initial);
  CHECK(initial->width == 128);
  CHECK(reload_resource::destroyed == 0);
}

TEST_CASE("resources - reload_handle_into rejects missing and mismatched resources",
          "[resources]") {
  caors::resource_cache<reload_resource> cache;
  reload_resource_loader loader;

  caors::resources::handle_result<reload_resource> missing =
      caors::resources::reload_handle_into(cache, loader, test_guid_a);
  CHECK(missing.failed());
  CHECK(missing.status == caors::error{caors::error_domain::resources, caors::errc::not_found});

  reload_resource* initial = cache.create(test_guid_a);
  REQUIRE(initial != nullptr);
  initial->width = 128;

  loader.width = 512;
  loader.forced_guid = test_guid_b;

  caors::resources::handle_result<reload_resource> mismatched =
      caors::resources::reload_handle_into(cache, loader, test_guid_a);

  CHECK(mismatched.failed());
  CHECK(mismatched.status ==
        caors::error{caors::error_domain::resources, caors::errc::inconsistent_type});
  CHECK(cache.find(test_guid_a) == initial);
  CHECK(initial->width == 128);
}
