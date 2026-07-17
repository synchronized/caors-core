// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_method_class_builder_rejects_duplicate_method_names_test.cpp

#include "meta_method_test_support.h"

using namespace test_meta;
using namespace caors::tests::meta_method;
TEST_CASE("meta - class builder rejects duplicate method signatures", "[meta]") {
  CHECK(
      ((void)caors::meta::class_<duplicate_method_actor>("caors.test.meta.duplicate_method_actor"),
       caors::meta::class_<duplicate_method_actor>())
          .method("update", &duplicate_method_actor::first)
          .method("update", &duplicate_method_actor::second)
          .commit() == meta_error(caors::errc::duplicate_method));
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

TEST_CASE("meta - method set groups overloaded methods", "[meta]") {
  const caors::error result =
      ((void)caors::meta::class_<overloaded_method_actor>(
           "caors.test.meta.overloaded_method_actor"),
       caors::meta::class_<overloaded_method_actor>())
          .method("update", static_cast<void (overloaded_method_actor::*)(int)>(
                                &overloaded_method_actor::update))
          .method("update", static_cast<void (overloaded_method_actor::*)(float)>(
                                &overloaded_method_actor::update))
          .method("update", static_cast<int (overloaded_method_actor::*)() const>(
                                &overloaded_method_actor::update))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_method)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.overloaded_method_actor");
  REQUIRE(reflected);
  CHECK(reflected.method_count() == 1);

  const caors::meta::method_set update = reflected.find_method("update");
  REQUIRE(update);
  CHECK(update.name() == "update");
  CHECK(update.overload_count() == 3);
  CHECK_FALSE(update.single());

  overloaded_method_actor instance{10};
  caors::meta::instance object = caors::meta::view(instance);
  int int_amount = 4;
  std::array<caors::any_ref, 1> int_arguments{caors::any_ref::from(int_amount)};
  CHECK_FALSE(update.invoke(object, int_arguments));
  CHECK(instance.value == 14);

  float float_amount = 2.0F;
  std::array<caors::any_ref, 1> float_arguments{caors::any_ref::from(float_amount)};
  CHECK_FALSE(update.invoke(object, float_arguments));
  CHECK(instance.value == 16);

  const caors::meta::method getter = update.resolve({});
  REQUIRE(getter);
  CHECK(getter.const_method());

  const caors::any result_value = update.invoke(caors::meta::view_const(instance));
  REQUIRE(result_value);
  const int* value = result_value.try_get<int>();
  REQUIRE(value != nullptr);
  CHECK(*value == 16);
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
  caors::meta::instance object = caors::meta::view(instance);
  int amount = 4;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(amount)};

  CHECK_FALSE(add.invoke(object, arguments));
  CHECK(instance.value == 7);

  caors::any result = score.invoke(object);
  REQUIRE(result);
  const int* value = result.try_get<int>();
  REQUIRE(value != nullptr);
  CHECK(*value == 14);

  const method_actor& const_instance = instance;
  auto const_result = score.try_invoke<int>(caors::meta::view_const(const_instance));
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
  caors::meta::instance object = caors::meta::view(instance);
  float wrong_argument = 2.0F;
  std::array<caors::any_ref, 1> wrong_arguments{caors::any_ref::from(wrong_argument)};
  CHECK_FALSE(add.invoke(object, wrong_arguments));
  CHECK(instance.value == 5);

  const method_actor& const_instance = instance;
  caors::meta::instance const_object = caors::meta::view_const(const_instance);
  int amount = 3;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(amount)};
  CHECK_FALSE(add.invoke(const_object, arguments));
  CHECK(instance.value == 5);

  const int readonly_target = 1;
  std::array<caors::any_ref, 1> readonly_arguments{caors::any_ref::from_const(readonly_target)};
  CHECK_FALSE(add_to.invoke(object, readonly_arguments));

  int target = 2;
  std::array<caors::any_ref, 1> writable_arguments{caors::any_ref::from(target)};
  CHECK_FALSE(add_to.invoke(caors::meta::instance{}, writable_arguments));
  CHECK_FALSE(add_to.invoke(object, {}));
  CHECK_FALSE(add_to.invoke(object, wrong_arguments));
  CHECK_FALSE(add_to.invoke(const_object, writable_arguments));

  CHECK_FALSE(add_to.invoke(object, writable_arguments));
  CHECK(target == 7);
}
