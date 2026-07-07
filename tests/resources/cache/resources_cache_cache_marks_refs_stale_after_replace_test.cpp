// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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
