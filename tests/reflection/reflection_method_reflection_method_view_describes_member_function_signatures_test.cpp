// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_method_test_support.h"

using namespace test_reflection;
using namespace caors::tests::reflection_method;
TEST_CASE("reflection - method view describes member function signatures", "[reflection]") {
  const caors::reflection::type reflected = require_method_type();

  CHECK(reflected.method_count() == 3);

  const caors::reflection::method add = require_method(reflected, "add");
  CHECK(add.name() == "add");
  CHECK_FALSE(add.const_method());
  CHECK_FALSE(static_cast<bool>(add.return_type()));
  REQUIRE(add.parameter_count() == 1);
  CHECK(add.parameter_type(0) == registered_type_id("int"));
  CHECK(add.parameter_type_name(0) == "int");

  const caors::reflection::method score = require_method(reflected, "score");
  CHECK(score.name() == "score");
  CHECK(score.const_method());
  CHECK(score.return_type() == registered_type_id("int"));
  CHECK(score.return_type_name() == "int");
  CHECK(score.parameter_count() == 0);

  CHECK_FALSE(reflected.method_at(100));
  CHECK_FALSE(reflected.find_method("missing"));
}
