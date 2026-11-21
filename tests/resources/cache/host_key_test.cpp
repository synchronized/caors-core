// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/memory/tracking.h"
#include "caors/resources/cache.h"
#include <catch2/catch_all.hpp>
#include <string>

namespace {
struct texture {
  explicit texture(int value = 0) : value(value) {}
  int value;
};
struct material {
  int value = 0;
};
struct collision_hash {
  std::size_t operator()(const std::string&) const noexcept { return 0; }
};
struct variant_key {
  std::string path;
  bool srgb;
  bool operator==(const variant_key&) const = default;
};
struct variant_hash {
  std::size_t operator()(const variant_key&) const noexcept { return 0; }
};
class failure_resource final : public caors::memory_resource {
public:
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  int remaining = -1;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override {
    // 避免在 MSVC Debug 容器代理的 noexcept 小型分配中注入失败。
    if (size > 32 && remaining >= 0 && remaining-- == 0)
      return nullptr;
    return tracking.try_allocate(size, alignment);
  }
  void do_deallocate(void* value, std::size_t size, std::size_t alignment) noexcept override {
    tracking.deallocate(value, size, alignment);
  }
  bool do_is_equal(const caors::memory_resource& other) const noexcept override {
    return this == &other;
  }
};
}

TEST_CASE("resources - host path keys own identity independently of plain objects",
          "[resources][host-key]") {
  caors::resources::cache<texture, std::string, collision_hash> cache;
  std::string path = "textures/wood.png";
  auto first = cache.create_handle(path, 1);
  REQUIRE(first);
  path.assign("changed");
  REQUIRE(cache.find("textures/wood.png") == first.get());
  auto second = cache.create_handle("textures/stone.png", 2);
  REQUIRE(second);
  CHECK(cache.size() == 2);
  CHECK_FALSE(cache.create_handle("textures/wood.png", 3));
  caors::resources::ref<texture, std::string> reference{std::string{"textures/wood.png"}};
  REQUIRE(cache.resolve(reference));
  CHECK(cache.is_current(reference));
  auto replacement = caors::make_handle<texture>(4);
  REQUIRE(cache.replace("textures/wood.png", replacement));
  CHECK(first->value == 1);
  CHECK_FALSE(cache.is_current(reference));
  REQUIRE(cache.resolve(reference));
  CHECK(reference->value == 4);
  cache.clear();
  CHECK_FALSE(cache.resolve(reference));
  REQUIRE(reference.key());
  CHECK(*reference.key() == "textures/wood.png");
  REQUIRE(cache.create_handle("textures/wood.png", 5));
  CHECK_FALSE(cache.is_current(reference));
}

TEST_CASE("resources - GUID and variant keys do not require object GUID fields",
          "[resources][host-key]") {
  caors::resources::cache<texture> guid_cache;
  auto guid = caors::resource_guid::parse("11223344-5566-7788-99aa-bbccddeeff00");
  REQUIRE(guid);
  REQUIRE(guid_cache.create_handle(*guid, 7));
  caors::resources::ref<texture> reference{*guid};
  REQUIRE(guid_cache.resolve(reference));
  CHECK(reference->value == 7);
  caors::resources::cache<texture, variant_key, variant_hash> variants;
  REQUIRE(variants.create_handle({"wood", true}, 8));
  REQUIRE(variants.create_handle({"wood", false}, 9));
  CHECK(variants.find({"wood", true})->value == 8);
  CHECK(variants.find({"wood", false})->value == 9);
  caors::resources::cache<material, std::string> materials;
  REQUIRE(materials.create_handle("wood"));
  CHECK(materials.size() == 1);
}

TEST_CASE("resources - host cache release preserves handles and allocator cleanup",
          "[resources][host-key]") {
  caors::tracking_resource memory(&caors::get_system_memory_resource());
  {
    caors::resources::cache<texture, std::string> cache(&memory);
    REQUIRE(cache.insert("item", caors::memory::make_unique<texture>(&memory, 11)));
    auto old = cache.find_handle("item");
    REQUIRE(cache.replace("item", caors::memory::make_unique<texture>(&memory, 12)));
    auto released = cache.release("item");
    REQUIRE(released);
    CHECK(released->value == 12);
    CHECK(old->value == 11);
    CHECK(cache.empty());
    CHECK_FALSE(cache.erase("missing"));
  }
  CHECK(memory.statistics().bytes_in_use == 0);
}

TEST_CASE("resources - host reference binding belongs to one cache even for shared objects",
          "[resources][host-key]") {
  caors::resources::cache<texture, std::string> first, second;
  auto value = caors::make_handle<texture>(13);
  REQUIRE(first.insert("same", value));
  REQUIRE(second.insert("same", value));
  caors::resources::ref<texture, std::string> reference{std::string{"same"}};
  REQUIRE(first.resolve(reference));
  CHECK(first.is_current(reference));
  CHECK_FALSE(second.is_current(reference));
  REQUIRE(second.resolve(reference));
  CHECK(second.is_current(reference));
  CHECK_FALSE(first.is_current(reference));
}

TEST_CASE("resources - host key insertion failure preserves published bindings and cleanup",
          "[resources][host-key][memory]") {
  for (int failure = 0; failure < 5; ++failure) {
    failure_resource memory;
    {
      caors::resources::cache<texture, std::string> cache(&memory);
      auto original = cache.create_handle("existing", 1);
      REQUIRE(original);
      const auto generation = cache.generation_of("existing");
      auto next = caors::make_handle<texture>(2);
      bool inserted = false;
      memory.remaining = failure;
      try {
        inserted = cache.insert("new", next);
      } catch (const std::bad_alloc&) {
      }
      memory.remaining = -1;
      CHECK(cache.find("existing") == original.get());
      CHECK(cache.generation_of("existing") == generation);
      CHECK(cache.contains("new") == inserted);
      CHECK(cache.size() == (inserted ? 2 : 1));
    }
    CHECK(memory.tracking.statistics().bytes_in_use == 0);
  }
}
