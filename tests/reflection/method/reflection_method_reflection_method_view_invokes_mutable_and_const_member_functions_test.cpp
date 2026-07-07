// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_method_test_support.h"

using namespace test_reflection;
using namespace caors::tests::reflection_method;
TEST_CASE("reflection - method view invokes mutable and const member functions", "[reflection]") {
  const caors::reflection::type reflected = require_method_type();
  const caors::reflection::method add = require_method(reflected, "add");
  const caors::reflection::method score = require_method(reflected, "score");

  method_actor instance{3};
  int amount = 4;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(amount)};

  CHECK_FALSE(add.invoke(&instance, arguments));
  CHECK(instance.value == 7);

  caors::any result = score.invoke(&instance);
  REQUIRE(result);
  const int* value = result.try_get<int>();
  REQUIRE(value != nullptr);
  CHECK(*value == 14);

  const method_actor& const_instance = instance;
  auto const_result = score.try_invoke<int>(&const_instance);
  REQUIRE(const_result.has_value());
  CHECK(*const_result == 14);
}
