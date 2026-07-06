// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

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
