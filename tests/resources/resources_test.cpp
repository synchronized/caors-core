// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources.h"

#include "caors/core/containers.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

namespace {

constexpr caors::resource_guid test_guid_a{caors::guid::from_bytes(
    caors::guid::byte_array{0x6c, 0xd2, 0xda, 0x3d, 0xb5, 0x49, 0x47, 0x56, 0x8b, 0x8b, 0x0c, 0x84,
                            0x29, 0x3a, 0xf4, 0x10})};

constexpr caors::resource_guid test_guid_b{caors::guid::from_bytes(
    caors::guid::byte_array{0x0c, 0x2f, 0x82, 0xf8, 0x2b, 0x3e, 0x4f, 0x77, 0x96, 0x5c, 0x4f, 0x2d,
                            0x1c, 0x4a, 0x90, 0x7a})};

class texture_resource : public caors::resource {
public:
  using caors::resource::resource;
};

class image_texture_resource : public texture_resource {
public:
  using texture_resource::texture_resource;
};

class observed_resource : public caors::resource {
public:
  using caors::resource::resource;

  ~observed_resource() override { ++destroyed; }

  inline static int destroyed = 0;
};

} // namespace

TEST_CASE("resources - resource guid wraps stable guid", "[resources]") {
  CHECK(caors::resource_guid::nil().is_nil());
  CHECK_FALSE(static_cast<bool>(caors::resource_guid::nil()));
  CHECK(test_guid_a);
  CHECK(test_guid_a != test_guid_b);

  const auto parsed = caors::resource_guid::parse(test_guid_a.string());
  REQUIRE(parsed.has_value());
  CHECK(*parsed == test_guid_a);
  CHECK_FALSE(caors::resource_guid::parse("not-a-guid").has_value());

  caors::hash_map<caors::resource_guid, int, caors::resource_guid_hash> values;
  values.emplace(test_guid_a, 7);
  REQUIRE(values.contains(test_guid_a));
  CHECK(values[test_guid_a] == 7);
}

TEST_CASE("resources - resource stores stable identity", "[resources]") {
  texture_resource texture;

  CHECK_FALSE(texture.has_guid());
  CHECK(texture.guid() == caors::resource_guid::nil());

  texture.set_guid(test_guid_a);

  CHECK(texture.has_guid());
  CHECK(texture.guid() == test_guid_a);
}

TEST_CASE("resources - ref can hold unresolved guid", "[resources]") {
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK(reference);
  CHECK(reference.has_guid());
  CHECK_FALSE(reference.resolved());
  CHECK(reference.get() == nullptr);
  CHECK(reference.guid() == test_guid_a);

  reference.unresolve();
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref resolves compatible resource", "[resources]") {
  texture_resource texture{test_guid_a};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  REQUIRE(reference.resolve(&texture));

  CHECK(reference.resolved());
  CHECK(reference.get() == &texture);
  CHECK(reference->guid() == test_guid_a);
  CHECK((*reference).guid() == test_guid_a);

  reference.unresolve();

  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref rejects mismatched guid", "[resources]") {
  texture_resource texture{test_guid_b};
  caors::resources::ref<texture_resource> reference{test_guid_a};

  CHECK_FALSE(reference.resolve(&texture));
  CHECK_FALSE(reference.resolved());
  CHECK(reference.guid() == test_guid_a);
}

TEST_CASE("resources - ref supports derived to base conversion", "[resources]") {
  image_texture_resource image{test_guid_a};
  caors::resources::ref<image_texture_resource> derived{&image};
  caors::resource_ref<texture_resource> base{derived};

  CHECK(base.resolved());
  CHECK(base.get() == &image);
  CHECK(base.guid() == test_guid_a);

  base.reset();
  CHECK_FALSE(base);
  CHECK_FALSE(base.resolved());
  CHECK(base.guid() == caors::resource_guid::nil());
}

TEST_CASE("resources - manager binds and finds resources", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture{test_guid_a};

  CHECK(manager.empty());
  CHECK(manager.resource() != nullptr);

  REQUIRE(manager.bind(&texture));

  CHECK(manager.size() == 1);
  CHECK(manager.contains(test_guid_a));
  CHECK(manager.contains(&texture));
  CHECK(manager.find(test_guid_a) == &texture);
  CHECK(manager.find<texture_resource>(test_guid_a) == &texture);
  CHECK(manager.find<image_texture_resource>(test_guid_a) == nullptr);
  CHECK(manager.guid_of(&texture) == test_guid_a);
}

TEST_CASE("resources - manager can assign guid while binding", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture;

  REQUIRE(manager.bind(test_guid_a, &texture));

  CHECK(texture.guid() == test_guid_a);
  CHECK(manager.find(test_guid_a) == &texture);
}

TEST_CASE("resources - manager rejects conflicting bindings", "[resources]") {
  caors::resource_manager manager;
  texture_resource first{test_guid_a};
  texture_resource second{test_guid_b};

  REQUIRE(manager.bind(&first));

  CHECK(manager.bind(&first));
  CHECK_FALSE(manager.bind(test_guid_b, &first));
  CHECK_FALSE(manager.bind(test_guid_a, &second));
  CHECK_FALSE(manager.bind(caors::resource_guid::nil(), &second));
  CHECK_FALSE(manager.bind(test_guid_b, nullptr));

  CHECK(manager.size() == 1);
  CHECK(manager.find(test_guid_a) == &first);
}

TEST_CASE("resources - manager unbinds by guid and resource pointer", "[resources]") {
  caors::resource_manager manager;
  texture_resource first{test_guid_a};
  texture_resource second{test_guid_b};

  REQUIRE(manager.bind(&first));
  REQUIRE(manager.bind(&second));

  CHECK(manager.unbind(test_guid_a));
  CHECK_FALSE(manager.unbind(test_guid_a));
  CHECK_FALSE(manager.contains(test_guid_a));
  CHECK_FALSE(manager.contains(&first));
  CHECK(manager.find(test_guid_a) == nullptr);
  CHECK(manager.guid_of(&first) == caors::resource_guid::nil());

  CHECK(manager.unbind(&second));
  CHECK(manager.empty());
  CHECK(manager.find(test_guid_b) == nullptr);
}

TEST_CASE("resources - manager clears without destroying resources", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture{test_guid_a};

  REQUIRE(manager.bind(&texture));

  manager.clear();

  CHECK(manager.empty());
  CHECK(texture.guid() == test_guid_a);
  CHECK(manager.find(test_guid_a) == nullptr);
}

TEST_CASE("resources - manager resolves resource refs", "[resources]") {
  caors::resource_manager manager;
  image_texture_resource image{test_guid_a};
  texture_resource other{test_guid_b};

  REQUIRE(manager.bind(&image));
  REQUIRE(manager.bind(&other));

  caors::resource_ref<texture_resource> base_ref{test_guid_a};
  REQUIRE(manager.resolve(base_ref));

  CHECK(base_ref.resolved());
  CHECK(base_ref.get() == &image);
  CHECK(base_ref.guid() == test_guid_a);

  caors::resources::ref<image_texture_resource> derived_ref{test_guid_a};
  REQUIRE(manager.resolve(derived_ref));
  CHECK(derived_ref.get() == &image);

  caors::resources::ref<image_texture_resource> mismatched_type{test_guid_b};
  CHECK_FALSE(manager.resolve(mismatched_type));
  CHECK_FALSE(mismatched_type.resolved());
  CHECK(mismatched_type.guid() == test_guid_b);

  caors::resources::ref<texture_resource> missing{caors::resource_guid::nil()};
  CHECK_FALSE(manager.resolve(missing));
}

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

TEST_CASE("resources - cache rejects invalid duplicate and mismatched resources", "[resources]") {
  caors::resource_cache<texture_resource> cache;

  CHECK(cache.create(caors::resource_guid::nil()) == nullptr);
  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.create(test_guid_a) == nullptr);
  CHECK(cache.create(test_guid_b, test_guid_a) == nullptr);
  CHECK_FALSE(cache.contains(test_guid_b));
}

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

TEST_CASE("resources - cache uses requested memory resource", "[resources]") {
  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::resource_cache<texture_resource> cache{&tracking};

  REQUIRE(cache.create(test_guid_a) != nullptr);

  CHECK(cache.resource() == &tracking);
  CHECK(cache.index().resource() == &tracking);
  CHECK(tracking.statistics().allocation_count > 0);
}
