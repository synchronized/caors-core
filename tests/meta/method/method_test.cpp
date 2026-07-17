// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_method_class_builder_rejects_duplicate_method_names_test.cpp

#include "meta_method_test_support.h"

using namespace test_meta;
using namespace caors::tests::meta_method;
TEST_CASE("meta - class builder rejects duplicate method names", "[meta]") {
  CHECK(
      ((void)caors::meta::class_<duplicate_method_actor>("caors.test.meta.duplicate_method_actor"),
       caors::meta::class_<duplicate_method_actor>())
          .method("update", &duplicate_method_actor::first)
          .method("update", &duplicate_method_actor::second)
          .commit() == reflection_error(caors::errc::duplicate_method));
}

// 原测试文件：meta_method_method_view_describes_member_function_signatures_test.cpp

#include "meta_method_test_support.h"

using namespace test_meta;
using namespace caors::tests::meta_method;
TEST_CASE("meta - method view describes member function signatures", "[meta]") {
  const caors::meta::type reflected = require_method_type();

  CHECK(reflected.method_count() == 3);

  const caors::meta::method add = require_method(reflected, "add");
  CHECK(add.name() == "add");
  CHECK_FALSE(add.const_method());
  CHECK_FALSE(add.get_return_type());
  REQUIRE(add.parameter_count() == 1);
  REQUIRE(add.get_parameter_type(0).has_value());
  CHECK(add.get_parameter_type(0)->id() == registered_type_id("int"));
  CHECK(add.get_parameter_type(0)->name() == "int");

  const caors::meta::method score = require_method(reflected, "score");
  CHECK(score.name() == "score");
  CHECK(score.const_method());
  CHECK(score.get_return_type().id() == registered_type_id("int"));
  CHECK(score.get_return_type().name() == "int");
  CHECK(score.parameter_count() == 0);

  CHECK_FALSE(reflected.method_at(100));
  CHECK_FALSE(reflected.find_method("missing"));
}

// 原测试文件：meta_method_method_view_invokes_mutable_and_const_member_functions_test.cpp

#include "meta_method_test_support.h"

using namespace test_meta;
using namespace caors::tests::meta_method;
TEST_CASE("meta - method view invokes mutable and const member functions", "[meta]") {
  const caors::meta::type reflected = require_method_type();
  const caors::meta::method add = require_method(reflected, "add");
  const caors::meta::method score = require_method(reflected, "score");

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

// 原测试文件：meta_method_method_view_rejects_invalid_calls_test.cpp

#include "meta_method_test_support.h"

using namespace test_meta;
using namespace caors::tests::meta_method;
TEST_CASE("meta - method view rejects invalid calls", "[meta]") {
  const caors::meta::type reflected = require_method_type();
  const caors::meta::method add = require_method(reflected, "add");
  const caors::meta::method add_to = require_method(reflected, "add_to");

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
