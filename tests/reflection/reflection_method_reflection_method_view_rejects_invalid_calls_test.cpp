// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_method_test_support.h"

using namespace test_reflection;
using namespace caors::tests::reflection_method;
TEST_CASE("reflection - method view rejects invalid calls", "[reflection]") {
  const caors::reflection::type reflected = require_method_type();
  const caors::reflection::method add = require_method(reflected, "add");
  const caors::reflection::method add_to = require_method(reflected, "add_to");

  method_actor instance{5};
  float wrong_argument = 2.0F;
  std::array<caors::any_ref, 1> wrong_arguments{caors::any_ref::from(wrong_argument)};
  CHECK_FALSE(add.invoke(&instance, wrong_arguments));
  CHECK(instance.value == 5);

  const method_actor& const_instance = instance;
  int amount = 3;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(amount)};
  CHECK_FALSE(add.invoke(&const_instance, arguments));
  CHECK(instance.value == 5);

  const int readonly_target = 1;
  std::array<caors::any_ref, 1> readonly_arguments{caors::any_ref::from_const(readonly_target)};
  CHECK_FALSE(add_to.invoke(&instance, readonly_arguments));

  int target = 2;
  std::array<caors::any_ref, 1> writable_arguments{caors::any_ref::from(target)};
  CHECK_FALSE(add_to.invoke(static_cast<method_actor*>(nullptr), writable_arguments));
  CHECK_FALSE(add_to.invoke(&instance, {}));
  CHECK_FALSE(add_to.invoke(&instance, wrong_arguments));
  CHECK_FALSE(add_to.invoke(&const_instance, writable_arguments));

  CHECK_FALSE(add_to.invoke(&instance, writable_arguments));
  CHECK(target == 7);
}
