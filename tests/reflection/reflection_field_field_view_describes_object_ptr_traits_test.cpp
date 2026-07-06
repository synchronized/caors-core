// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_test_support.h"

using namespace test_reflection;

TEST_CASE("reflection - field view describes object ptr traits", "[reflection]") {
  const caors::reflection::type reflected = require_field_traits_owner_reflection();
  const caors::reflection::field object_ptr = require_field(reflected, "object_ptr");

  CHECK(object_ptr.kind() == caors::meta::type_kind::value);
  CHECK(object_ptr.wrapper_semantics() == caors::meta::wrapper_semantics::none);
  CHECK(object_ptr.value_type() ==
        registered_type_id("caors.object_ptr<caors.test.reflection.reflected_object_ptr_target>"));
  CHECK_FALSE(object_ptr.nullable());

  field_traits_owner owner{};
  reflected_object_ptr_target target{};
  owner.object_ptr.reset(&target);
  CHECK(object_ptr.get_object_ptr(&owner) == &target);
  owner.object_ptr.reset();
  CHECK(object_ptr.try_set_object_ptr(&owner, &target));
  CHECK(owner.object_ptr.get() == &target);
}
