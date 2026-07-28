// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_method_registration_rejects_duplicate_method_names_test.cpp

#include "meta_method_test_support.h"

using namespace test_meta;
using namespace caors::tests::meta_method;
TEST_CASE("meta - class builder rejects duplicate method signatures", "[meta]") {
  CHECK(caors::registration::type_<duplicate_method_actor>("caors.test.meta.duplicate_method_actor")
            .meta()
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
      caors::registration::type_<overloaded_method_actor>("caors.test.meta.overloaded_method_actor")
          .meta()
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
  std::array<caors::argument, 1> int_arguments{caors::arg(int_amount)};
  CHECK(update.invoke_result_of(object, int_arguments).kind() ==
        caors::meta::invoke_result_kind::void_value);
  CHECK(instance.value == 14);

  float float_amount = 2.0F;
  std::array<caors::argument, 1> float_arguments{caors::arg(float_amount)};
  CHECK(update.invoke_result_of(object, float_arguments).kind() ==
        caors::meta::invoke_result_kind::void_value);
  CHECK(instance.value == 16);

  const caors::meta::method getter = update.resolve(std::span<const caors::meta::argument>{});
  REQUIRE(getter);
  CHECK(getter.const_method());

  caors::meta::invoke_result result_value =
      update.invoke_result_of(caors::meta::view_const(instance));
  REQUIRE(result_value.has_value());
  const int* value = result_value.try_get<int>();
  REQUIRE(value != nullptr);
  CHECK(*value == 16);
}

TEST_CASE("meta - method set uses const context during invoke overload resolution", "[meta]") {
  const caors::error result =
      caors::registration::type_<const_context_overload_actor>(
          "caors.test.meta.const_context_overload_actor")
          .meta()
          .method("read", static_cast<int (const_context_overload_actor::*)()>(
                              &const_context_overload_actor::read))
          .method("read", static_cast<int (const_context_overload_actor::*)() const>(
                              &const_context_overload_actor::read))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.const_context_overload_actor");
  REQUIRE(reflected);
  const caors::meta::method_set read = reflected.find_method("read");
  REQUIRE(read);

  const_context_overload_actor instance{5};
  const caors::meta::invoke_result result_value =
      read.invoke_result_of(caors::meta::view_const(instance));
  REQUIRE(result_value.has_value());
  REQUIRE(result_value.try_get<int>() != nullptr);
  CHECK(*result_value.try_get<int>() == 15);
  CHECK(instance.value == 5);

  const caors::meta::invoke_value_result value_result =
      read.invoke(caors::meta::view_const(instance));
  REQUIRE(value_result.has_value());
  CHECK(value_result.value().is_i64());
  CHECK(value_result.value().as_i64() == 15);
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
  std::array<caors::argument, 1> arguments{caors::arg(amount)};

  CHECK(add.invoke_result_of(object, arguments).kind() ==
        caors::meta::invoke_result_kind::void_value);
  CHECK(instance.value == 7);

  caors::meta::invoke_result result = score.invoke_result_of(object);
  REQUIRE(result.has_value());
  const int* value = result.try_get<int>();
  REQUIRE(value != nullptr);
  CHECK(*value == 14);

  const method_actor& const_instance = instance;
  auto const_result = score.try_invoke<int>(caors::meta::view_const(const_instance));
  REQUIRE(const_result.has_value());
  CHECK(*const_result == 14);
}

TEST_CASE("meta - method view invokes rvalue reference parameters through arguments", "[meta]") {
  const caors::error result =
      caors::registration::type_<rvalue_method_actor>("caors.test.meta.rvalue_method_actor")
          .meta()
          .method("take", &rvalue_method_actor::take)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_method)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.rvalue_method_actor");
  REQUIRE(reflected);
  const caors::meta::method take = reflected.find_method("take").single();
  REQUIRE(take);

  rvalue_method_actor instance{};
  caors::meta::instance object = caors::meta::view(instance);

  int lvalue = 12;
  std::array<caors::meta::argument, 1> lvalue_arguments{caors::meta::arg(lvalue)};
  CHECK_FALSE(take.invoke_result_of(object, lvalue_arguments));
  CHECK(instance.value == 0);

  std::array<caors::meta::argument, 1> rvalue_arguments{caors::meta::move_arg(21)};
  CHECK(take.invoke_result_of(object, rvalue_arguments).kind() ==
        caors::meta::invoke_result_kind::void_value);
  CHECK(instance.value == 21);
}

TEST_CASE("meta - method invoke result preserves void value and reference returns", "[meta]") {
  const caors::error result =
      caors::registration::type_<invoke_result_actor>("caors.test.meta.invoke_result_actor")
          .meta()
          .method("set", &invoke_result_actor::set)
          .method("score", &invoke_result_actor::score)
          .method("ref", &invoke_result_actor::ref)
          .method("cref", &invoke_result_actor::cref)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.invoke_result_actor");
  REQUIRE(reflected);

  invoke_result_actor instance{5};
  caors::meta::instance object = caors::meta::view(instance);

  int next = 7;
  std::array<caors::argument, 1> arguments{caors::arg(next)};
  const caors::meta::invoke_result set_result =
      reflected.find_method("set").invoke_result_of(object, arguments);
  REQUIRE(set_result);
  CHECK(set_result.kind() == caors::meta::invoke_result_kind::void_value);
  CHECK(instance.value == 7);

  caors::meta::invoke_result score_result =
      reflected.find_method("score").invoke_result_of(caors::meta::view_const(instance));
  REQUIRE(score_result);
  CHECK(score_result.kind() == caors::meta::invoke_result_kind::value);
  const int* score = score_result.try_get<int>();
  REQUIRE(score != nullptr);
  CHECK(*score == 21);

  caors::meta::invoke_result ref_result = reflected.find_method("ref").invoke_result_of(object);
  REQUIRE(ref_result);
  CHECK(ref_result.kind() == caors::meta::invoke_result_kind::reference);
  int* value_ref = ref_result.try_get_mutable<int>();
  REQUIRE(value_ref != nullptr);
  *value_ref = 11;
  CHECK(instance.value == 11);
  CHECK(reflected.find_method("ref").invoke_result_of(object).has_reference());

  caors::meta::invoke_result cref_result =
      reflected.find_method("cref").invoke_result_of(caors::meta::view_const(instance));
  REQUIRE(cref_result);
  CHECK(cref_result.kind() == caors::meta::invoke_result_kind::reference);
  CHECK(cref_result.try_get<int>() != nullptr);
  CHECK(cref_result.try_get_mutable<int>() == nullptr);
}

TEST_CASE("meta - method default arguments require explicit match option", "[meta]") {
  const caors::error result =
      caors::registration::type_<default_argument_actor>("caors.test.meta.default_argument_actor")
          .meta()
          .method("add", &default_argument_actor::add,
                  caors::meta::registration::default_argument(8))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.default_argument_actor");
  REQUIRE(reflected);
  const caors::meta::method_set add = reflected.find_method("add");
  REQUIRE(add);

  int left = 5;
  std::array<caors::argument, 1> arguments{caors::arg(left)};
  CHECK_FALSE(add.resolve(arguments));
  default_argument_actor instance{};
  CHECK_FALSE(add.invoke_result_of(caors::meta::view(instance), arguments));

  caors::meta::argument_match_options options{};
  options.allow_default_arguments = true;
  const caors::meta::method resolved = add.resolve(arguments, options);
  REQUIRE(resolved);
  CHECK(resolved.default_argument_count() == 1);

  caors::meta::invoke_result invoke_result =
      add.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 13);
  CHECK(instance.value == 13);

  default_argument_actor value_instance{};
  std::array<caors::meta::value, 1> value_arguments{caors::meta::value{left}};
  CHECK_FALSE(add.invoke(caors::meta::view(value_instance), value_arguments));

  const caors::meta::invoke_value_result value_result =
      add.invoke(caors::meta::view(value_instance), value_arguments, options);
  REQUIRE(value_result.has_value());
  CHECK(value_result.value().is_i64());
  CHECK(value_result.value().as_i64() == 13);
  CHECK(value_instance.value == 13);
}

TEST_CASE("meta - method default arguments reject mismatched values", "[meta]") {
  const caors::error result = caors::registration::type_<default_argument_mismatch_actor>(
                                  "caors.test.meta.default_argument_mismatch_actor")
                                  .meta()
                                  .method("add", &default_argument_mismatch_actor::add,
                                          caors::meta::registration::default_argument(1.0F))
                                  .commit();

  CHECK(result == meta_error(caors::errc::invalid_field));
}

TEST_CASE("meta - method named arguments require explicit match option", "[meta]") {
  const caors::error result =
      caors::registration::type_<named_argument_actor>("caors.test.meta.named_argument_actor")
          .meta()
          .method("add", &named_argument_actor::add,
                  caors::meta::registration::parameter_name("left"),
                  caors::meta::registration::parameter_name("right"))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.named_argument_actor");
  REQUIRE(reflected);
  const caors::meta::method_set add = reflected.find_method("add");
  REQUIRE(add);

  int left = 5;
  int right = 8;
  std::array<caors::meta::named_argument, 2> arguments{
      caors::meta::narg("right", right),
      caors::meta::narg("left", left),
  };
  CHECK_FALSE(add.resolve(arguments));

  caors::meta::argument_match_options options{};
  options.allow_named_arguments = true;
  const caors::meta::method resolved = add.resolve(arguments, options);
  REQUIRE(resolved);
  CHECK(resolved.get_parameter_name(0) == "left");
  CHECK(resolved.get_parameter_name(1) == "right");

  named_argument_actor instance{};
  caors::meta::invoke_result invoke_result =
      add.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 13);
  CHECK(instance.value == 13);
}

TEST_CASE("meta - method named arguments can use registered defaults", "[meta]") {
  const caors::error result = caors::registration::type_<named_default_argument_actor>(
                                  "caors.test.meta.named_default_argument_actor")
                                  .meta()
                                  .method("add", &named_default_argument_actor::add,
                                          caors::meta::registration::parameter_name("left"),
                                          caors::meta::registration::parameter_name("right"),
                                          caors::meta::registration::parameter_name("scale"),
                                          caors::meta::registration::default_argument(2))
                                  .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.named_default_argument_actor");
  REQUIRE(reflected);
  const caors::meta::method_set add = reflected.find_method("add");
  REQUIRE(add);

  int left = 4;
  int right = 6;
  std::array<caors::meta::named_argument, 2> arguments{
      caors::meta::narg("right", right),
      caors::meta::narg("left", left),
  };

  caors::meta::argument_match_options options{};
  options.allow_named_arguments = true;
  CHECK_FALSE(add.resolve(arguments, options));

  options.allow_default_arguments = true;
  named_default_argument_actor instance{};
  caors::meta::invoke_result invoke_result =
      add.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 20);
  CHECK(instance.value == 20);
}

TEST_CASE("meta - method named defaults and string conversion compose explicitly", "[meta]") {
  const caors::error result = caors::registration::type_<named_default_conversion_actor>(
                                  "caors.test.meta.named_default_conversion_actor")
                                  .meta()
                                  .method("add", &named_default_conversion_actor::add,
                                          caors::meta::registration::parameter_name("left"),
                                          caors::meta::registration::parameter_name("right"),
                                          caors::meta::registration::parameter_name("scale"),
                                          caors::meta::registration::default_argument(2))
                                  .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.named_default_conversion_actor");
  REQUIRE(reflected);
  const caors::meta::method_set add = reflected.find_method("add");
  REQUIRE(add);

  int left = 4;
  caors::core::string right{"6"};
  std::array<caors::meta::named_argument, 2> arguments{
      caors::meta::move_narg("right", right),
      caors::meta::narg("left", left),
  };

  caors::meta::argument_match_options options{};
  options.allow_named_arguments = true;
  options.allow_default_arguments = true;
  CHECK_FALSE(add.resolve(arguments, options));

  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;
  const caors::meta::method_match_result match = add.match(arguments, options);
  REQUIRE(match);
  CHECK(match.kind() == caors::meta::method_match_kind::conversion);

  named_default_conversion_actor instance{};
  const caors::meta::invoke_result invoke_result =
      add.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 20);
  CHECK(instance.value == 20);
}

TEST_CASE("meta - method default arguments keep overload ambiguity explicit", "[meta]") {
  const caors::error result =
      caors::registration::type_<default_argument_overload_actor>(
          "caors.test.meta.default_argument_overload_actor")
          .meta()
          .method("choose", static_cast<int (default_argument_overload_actor::*)(int)>(
                                &default_argument_overload_actor::choose))
          .method("choose",
                  static_cast<int (default_argument_overload_actor::*)(int, int)>(
                      &default_argument_overload_actor::choose),
                  caors::meta::registration::default_argument(1))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.default_argument_overload_actor");
  REQUIRE(reflected);
  const caors::meta::method_set choose = reflected.find_method("choose");
  REQUIRE(choose);

  int value = 9;
  std::array<caors::argument, 1> arguments{caors::arg(value)};
  CHECK(choose.resolve(arguments));

  caors::meta::argument_match_options options{};
  options.allow_default_arguments = true;
  CHECK_FALSE(choose.resolve(arguments, options));
  default_argument_overload_actor instance{};
  CHECK_FALSE(choose.invoke_result_of(caors::meta::view(instance), arguments, options));
}

TEST_CASE("meta - method invoke result owns move-only by-value returns", "[meta]") {
  const caors::error result =
      caors::registration::type_<move_only_return_actor>("caors.test.meta.move_only_return_actor")
          .meta()
          .method("make", &move_only_return_actor::make)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.move_only_return_actor");
  REQUIRE(reflected);
  const caors::meta::method make = reflected.find_method("make").single();
  REQUIRE(make);

  int value = 31;
  std::array<caors::argument, 1> arguments{caors::arg(value)};
  move_only_return_actor instance{};
  caors::meta::invoke_result invoke_result =
      make.invoke_result_of(caors::meta::view(instance), arguments);
  REQUIRE(invoke_result.has_value());
  auto* returned = invoke_result.try_get_mutable<move_only_return_value>();
  REQUIRE(returned != nullptr);
  CHECK(returned->value == 31);
}

TEST_CASE("meta - method invoke result diagnoses unsupported by-value returns", "[meta]") {
  const caors::error result =
      caors::registration::type_<immovable_return_actor>("caors.test.meta.immovable_return_actor")
          .meta()
          .method("make", &immovable_return_actor::make)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.immovable_return_actor");
  REQUIRE(reflected);
  const caors::meta::method make = reflected.find_method("make").single();
  REQUIRE(make);

  int value = 41;
  std::array<caors::argument, 1> arguments{caors::arg(value)};
  immovable_return_actor instance{};
  const caors::meta::invoke_result invoke_result =
      make.invoke_result_of(caors::meta::view(instance), arguments);
  REQUIRE_FALSE(invoke_result);
  CHECK(invoke_result.failure_reason() ==
        caors::meta::invoke_failure_reason::unsupported_return_value);

  std::array<caors::meta::value, 1> value_arguments{caors::meta::value{value}};
  const caors::meta::invoke_value_result value_result =
      make.invoke(caors::meta::view(instance), value_arguments);
  REQUIRE_FALSE(value_result);
  CHECK(value_result.failure_reason() ==
        caors::meta::invoke_failure_reason::unsupported_return_value);
}

TEST_CASE("meta - method numeric safe conversion requires explicit policy", "[meta]") {
  const caors::error result = caors::registration::type_<numeric_conversion_actor>(
                                  "caors.test.meta.numeric_conversion_actor")
                                  .meta()
                                  .method("take_i64", &numeric_conversion_actor::take_i64)
                                  .method("take_double", &numeric_conversion_actor::take_double)
                                  .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.numeric_conversion_actor");
  REQUIRE(reflected);
  const caors::meta::method_set take_i64 = reflected.find_method("take_i64");
  REQUIRE(take_i64);

  int input = 17;
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  CHECK_FALSE(take_i64.resolve(arguments));

  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::numeric_safe;
  const caors::meta::method resolved = take_i64.resolve(arguments, options);
  REQUIRE(resolved);

  numeric_conversion_actor instance{};
  caors::meta::invoke_result invoke_result =
      take_i64.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 17);
  CHECK(instance.value == 17);

  const caors::meta::method_set take_double = reflected.find_method("take_double");
  REQUIRE(take_double);
  float precise = 4.0F;
  std::array<caors::argument, 1> floating_arguments{caors::arg(precise)};
  invoke_result =
      take_double.invoke_result_of(caors::meta::view(instance), floating_arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 8);
  CHECK(instance.value == 8);
}

TEST_CASE("meta - method numeric safe conversion rejects narrowing", "[meta]") {
  const caors::error result =
      caors::registration::type_<numeric_narrowing_actor>("caors.test.meta.numeric_narrowing_actor")
          .meta()
          .method("take_i64", &numeric_narrowing_actor::take_i64)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.numeric_narrowing_actor");
  REQUIRE(reflected);
  const caors::meta::method_set take_i64 = reflected.find_method("take_i64");
  REQUIRE(take_i64);

  double input = 17.0;
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::numeric_safe;
  CHECK_FALSE(take_i64.resolve(arguments, options));
}

TEST_CASE("meta - method numeric safe conversion keeps exact overload priority", "[meta]") {
  const caors::error result =
      caors::registration::type_<numeric_conversion_overload_actor>(
          "caors.test.meta.numeric_conversion_overload_actor")
          .meta()
          .method("select", static_cast<int (numeric_conversion_overload_actor::*)(int)>(
                                &numeric_conversion_overload_actor::select))
          .method("select", static_cast<int (numeric_conversion_overload_actor::*)(long long)>(
                                &numeric_conversion_overload_actor::select))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.numeric_conversion_overload_actor");
  REQUIRE(reflected);
  const caors::meta::method_set select = reflected.find_method("select");
  REQUIRE(select);

  int input = 7;
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::numeric_safe;

  numeric_conversion_overload_actor instance{};
  caors::meta::invoke_result invoke_result =
      select.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 7);
  CHECK(instance.value == 7);
}

TEST_CASE("meta - method numeric safe conversion reports overload ambiguity", "[meta]") {
  const caors::error result =
      caors::registration::type_<numeric_conversion_ambiguous_actor>(
          "caors.test.meta.numeric_conversion_ambiguous_actor")
          .meta()
          .method("select", static_cast<int (numeric_conversion_ambiguous_actor::*)(long long)>(
                                &numeric_conversion_ambiguous_actor::select))
          .method("select", static_cast<int (numeric_conversion_ambiguous_actor::*)(double)>(
                                &numeric_conversion_ambiguous_actor::select))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.numeric_conversion_ambiguous_actor");
  REQUIRE(reflected);
  const caors::meta::method_set select = reflected.find_method("select");
  REQUIRE(select);

  int input = 7;
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::numeric_safe;

  CHECK_FALSE(select.resolve(arguments, options));
  const caors::meta::method_match_result match = select.match(arguments, options);
  CHECK(match.kind() == caors::meta::method_match_kind::ambiguous);
  CHECK(match.is_ambiguous());
  numeric_conversion_ambiguous_actor instance{};
  const caors::meta::invoke_result invoke_failure =
      select.invoke_result_of(caors::meta::view(instance), arguments, options);
  CHECK_FALSE(invoke_failure);
  CHECK(invoke_failure.failure_reason() == caors::meta::invoke_failure_reason::ambiguous_overload);
}

TEST_CASE("meta - method string parse conversion requires explicit policy", "[meta]") {
  const caors::error result =
      caors::registration::type_<string_parse_conversion_actor>(
          "caors.test.meta.string_parse_conversion_actor")
          .meta()
          .method("take_int", &string_parse_conversion_actor::take_int)
          .method("take_bool", &string_parse_conversion_actor::take_bool)
          .method("take_double", &string_parse_conversion_actor::take_double)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.string_parse_conversion_actor");
  REQUIRE(reflected);
  const caors::meta::method_set take_int = reflected.find_method("take_int");
  REQUIRE(take_int);

  caors::core::string input{"42"};
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  CHECK_FALSE(take_int.resolve(arguments));

  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;
  const caors::meta::method resolved = take_int.resolve(arguments, options);
  REQUIRE(resolved);

  string_parse_conversion_actor instance{};
  caors::meta::invoke_result invoke_result =
      take_int.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 42);
  CHECK(instance.value == 42);

  const caors::meta::method_set take_bool = reflected.find_method("take_bool");
  REQUIRE(take_bool);
  caors::core::string_name bool_input{"true"};
  std::array<caors::argument, 1> bool_arguments{caors::arg(bool_input)};
  invoke_result = take_bool.invoke_result_of(caors::meta::view(instance), bool_arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 1);

  const caors::meta::method_set take_double = reflected.find_method("take_double");
  REQUIRE(take_double);
  std::string_view double_input{"3.5"};
  std::array<caors::argument, 1> double_arguments{caors::arg(double_input)};
  invoke_result =
      take_double.invoke_result_of(caors::meta::view(instance), double_arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 7);
}

TEST_CASE("meta - method string parse conversion rejects invalid text", "[meta]") {
  const caors::error result =
      caors::registration::type_<string_parse_conversion_actor>(
          "caors.test.meta.string_parse_conversion_actor")
          .meta()
          .method("take_int", &string_parse_conversion_actor::take_int)
          .method("take_bool", &string_parse_conversion_actor::take_bool)
          .method("take_double", &string_parse_conversion_actor::take_double)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.string_parse_conversion_actor");
  REQUIRE(reflected);
  const caors::meta::method_set take_int = reflected.find_method("take_int");
  REQUIRE(take_int);

  caors::core::string input{"not-number"};
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;
  CHECK_FALSE(take_int.resolve(arguments, options));

  string_parse_conversion_actor instance{};
  const caors::meta::invoke_result invoke_failure =
      take_int.invoke_result_of(caors::meta::view(instance), arguments, options);
  CHECK_FALSE(invoke_failure);
  CHECK(invoke_failure.failure_reason() == caors::meta::invoke_failure_reason::argument_mismatch);
}

TEST_CASE("meta - method string parse conversion reports overload ambiguity", "[meta]") {
  const caors::error result =
      caors::registration::type_<string_parse_ambiguous_actor>(
          "caors.test.meta.string_parse_ambiguous_actor")
          .meta()
          .method("select", static_cast<int (string_parse_ambiguous_actor::*)(int)>(
                                &string_parse_ambiguous_actor::select))
          .method("select", static_cast<int (string_parse_ambiguous_actor::*)(bool)>(
                                &string_parse_ambiguous_actor::select))
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.string_parse_ambiguous_actor");
  REQUIRE(reflected);
  const caors::meta::method_set select = reflected.find_method("select");
  REQUIRE(select);

  caors::core::string input{"1"};
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;

  CHECK_FALSE(select.resolve(arguments, options));
  const caors::meta::method_match_result match = select.match(arguments, options);
  CHECK(match.kind() == caors::meta::method_match_kind::ambiguous);

  string_parse_ambiguous_actor instance{};
  const caors::meta::invoke_result invoke_failure =
      select.invoke_result_of(caors::meta::view(instance), arguments, options);
  CHECK_FALSE(invoke_failure);
  CHECK(invoke_failure.failure_reason() == caors::meta::invoke_failure_reason::ambiguous_overload);
}

TEST_CASE("meta - method string parse conversion works through value arguments", "[meta]") {
  const caors::error result =
      caors::registration::type_<string_parse_conversion_actor>(
          "caors.test.meta.string_parse_conversion_actor")
          .meta()
          .method("take_int", &string_parse_conversion_actor::take_int)
          .method("take_bool", &string_parse_conversion_actor::take_bool)
          .method("take_double", &string_parse_conversion_actor::take_double)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.string_parse_conversion_actor");
  REQUIRE(reflected);
  const caors::meta::method_set take_int = reflected.find_method("take_int");
  REQUIRE(take_int);

  std::array<caors::meta::value, 1> arguments{caors::meta::value{caors::core::string{"24"}}};
  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;

  string_parse_conversion_actor instance{};
  const caors::meta::invoke_value_result invoke_result =
      take_int.invoke(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result);
  REQUIRE(invoke_result.value().is_i64());
  CHECK(invoke_result.value().as_i64() == 24);
  CHECK(instance.value == 24);
}

TEST_CASE("meta - method string parse conversion supports named scalars and enum", "[meta]") {
  auto enum_builder =
      caors::registration::enum_<string_parse_state>("caors.test.meta.string_parse_state");
  enum_builder.value("idle", string_parse_state::idle)
      .value("running", string_parse_state::running);
  const caors::error enum_result = enum_builder.commit();
  REQUIRE((enum_result == caors::error{} || enum_result == meta_error(caors::errc::duplicate_id)));

  const caors::error actor_result =
      caors::registration::type_<string_parse_extended_actor>(
          "caors.test.meta.string_parse_extended_actor")
          .meta()
          .method("take_label", &string_parse_extended_actor::take_label)
          .method("take_guid", &string_parse_extended_actor::take_guid)
          .method("take_type_guid", &string_parse_extended_actor::take_type_guid)
          .method("take_state", &string_parse_extended_actor::take_state)
          .commit();
  REQUIRE(
      (actor_result == caors::error{} || actor_result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.string_parse_extended_actor");
  REQUIRE(reflected);

  caors::meta::argument_match_options options{};
  options.conversion_policy = caors::meta::argument_conversion_policy::string_parse;

  string_parse_extended_actor instance{};
  caors::core::string label{"runtime.label"};
  const caors::meta::invoke_result label_result =
      reflected.find_method("take_label")
          .invoke_result_of(caors::meta::view(instance), std::array{caors::arg(label)}, options);
  REQUIRE(label_result.has_value());
  REQUIRE(label_result.try_get<int>() != nullptr);
  CHECK(*label_result.try_get<int>() == 1);
  CHECK(instance.label == "runtime.label");

  const caors::core::string guid_text{"2f6d1f64-7a3a-4db2-9a2d-123456789abc"};
  const caors::meta::invoke_result guid_result =
      reflected.find_method("take_guid")
          .invoke_result_of(caors::meta::view(instance), std::array{caors::arg(guid_text)},
                            options);
  REQUIRE(guid_result.has_value());
  CHECK(instance.guid.string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  const caors::core::string type_guid_text{"5ed39a97-7b20-4d87-86ac-e62a4a615b10"};
  const caors::meta::invoke_result type_guid_result =
      reflected.find_method("take_type_guid")
          .invoke_result_of(caors::meta::view(instance), std::array{caors::arg(type_guid_text)},
                            options);
  REQUIRE(type_guid_result.has_value());
  CHECK(instance.type.value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");

  const caors::core::string state_text{"running"};
  const caors::meta::invoke_result state_result =
      reflected.find_method("take_state")
          .invoke_result_of(caors::meta::view(instance), std::array{caors::arg(state_text)},
                            options);
  REQUIRE(state_result.has_value());
  CHECK(instance.state == string_parse_state::running);
}

TEST_CASE("meta - method custom argument conversion is explicit", "[meta]") {
  const caors::error source_result =
      caors::registration::type_<custom_argument_value>("caors.test.meta.custom_argument_value")
          .meta()
          .capability(custom_argument_value_conversion)
          .commit();
  REQUIRE(
      (source_result == caors::error{} || source_result == meta_error(caors::errc::duplicate_id)));

  const caors::error actor_result = caors::registration::type_<custom_argument_conversion_actor>(
                                        "caors.test.meta.custom_argument_conversion_actor")
                                        .meta()
                                        .method("take", &custom_argument_conversion_actor::take)
                                        .commit();
  REQUIRE(
      (actor_result == caors::error{} || actor_result == meta_error(caors::errc::duplicate_id)));

  const caors::meta::type reflected =
      caors::meta::find("caors.test.meta.custom_argument_conversion_actor");
  REQUIRE(reflected);
  const caors::meta::method_set take = reflected.find_method("take");
  REQUIRE(take);

  custom_argument_value input{23};
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  CHECK_FALSE(take.resolve(arguments));
  CHECK(take.match(arguments).kind() == caors::meta::method_match_kind::none);

  caors::meta::argument_match_options options{};
  options.allow_custom_conversions = true;
  const caors::meta::method_match_result match = take.match(arguments, options);
  REQUIRE(match);
  CHECK(match.kind() == caors::meta::method_match_kind::conversion);

  custom_argument_conversion_actor instance{};
  const caors::meta::invoke_result invoke_result =
      take.invoke_result_of(caors::meta::view(instance), arguments, options);
  REQUIRE(invoke_result.has_value());
  REQUIRE(invoke_result.try_get<int>() != nullptr);
  CHECK(*invoke_result.try_get<int>() == 23);
  CHECK(instance.value == 23);
}

TEST_CASE("meta - method invoke result reports direct call failures", "[meta]") {
  const caors::meta::type reflected = require_method_type();
  const caors::meta::method add = require_method(reflected, "add");

  method_actor instance{};
  float wrong = 1.0F;
  std::array<caors::argument, 1> arguments{caors::arg(wrong)};
  const caors::meta::invoke_result mismatch =
      add.invoke_result_of(caors::meta::view(instance), arguments);
  CHECK_FALSE(mismatch);
  CHECK(mismatch.failure_reason() == caors::meta::invoke_failure_reason::argument_mismatch);

  const caors::meta::invoke_result const_call =
      add.invoke_result_of(caors::meta::view_const(instance), std::span<const caors::argument>{});
  CHECK_FALSE(const_call);
  CHECK(const_call.failure_reason() == caors::meta::invoke_failure_reason::const_violation);
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
  std::array<caors::argument, 1> wrong_arguments{caors::arg(wrong_argument)};
  CHECK_FALSE(add.invoke_result_of(object, wrong_arguments));
  CHECK(instance.value == 5);

  const method_actor& const_instance = instance;
  caors::meta::instance const_object = caors::meta::view_const(const_instance);
  int amount = 3;
  std::array<caors::argument, 1> arguments{caors::arg(amount)};
  CHECK_FALSE(add.invoke_result_of(const_object, arguments));
  CHECK(instance.value == 5);

  const int readonly_target = 1;
  std::array<caors::argument, 1> readonly_arguments{caors::carg(readonly_target)};
  CHECK_FALSE(add_to.invoke_result_of(object, readonly_arguments));

  int target = 2;
  std::array<caors::argument, 1> writable_arguments{caors::arg(target)};
  CHECK_FALSE(add_to.invoke_result_of(caors::meta::instance{}, writable_arguments));
  CHECK_FALSE(add_to.invoke_result_of(object, std::span<const caors::meta::argument>{}));
  CHECK_FALSE(add_to.invoke_result_of(object, wrong_arguments));
  CHECK_FALSE(add_to.invoke_result_of(const_object, writable_arguments));

  CHECK(add_to.invoke_result_of(object, writable_arguments).kind() ==
        caors::meta::invoke_result_kind::void_value);
  CHECK(target == 7);
}
