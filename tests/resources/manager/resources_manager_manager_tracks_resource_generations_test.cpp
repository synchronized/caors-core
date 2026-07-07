// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - manager tracks resource binding generations", "[resources]") {
  caors::resource_manager manager;
  texture_resource texture{test_guid_a};

  REQUIRE(manager.bind(&texture));
  const caors::resource_generation first_generation = manager.generation_of(test_guid_a);
  CHECK(first_generation != caors::invalid_resource_generation);
  CHECK(manager.bind(&texture));
  CHECK(manager.generation_of(test_guid_a) == first_generation);

  caors::resource_ref<texture_resource> reference{test_guid_a};
  REQUIRE(manager.resolve(reference));
  CHECK(reference.generation() == first_generation);
  CHECK(manager.is_current(reference));

  CHECK(manager.unbind(test_guid_a));
  CHECK(manager.generation_of(test_guid_a) == caors::invalid_resource_generation);
  CHECK_FALSE(manager.is_current(reference));

  REQUIRE(manager.bind(&texture));
  const caors::resource_generation second_generation = manager.generation_of(test_guid_a);
  CHECK(second_generation != caors::invalid_resource_generation);
  CHECK(second_generation != first_generation);

  REQUIRE(manager.resolve(reference));
  CHECK(reference.generation() == second_generation);
  CHECK(manager.is_current(reference));
}
