// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources.h"

#include "caors/core/containers.h"

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
