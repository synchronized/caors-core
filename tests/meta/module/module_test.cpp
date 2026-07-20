// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <array>

#include <catch2/catch_all.hpp>

namespace {

int module_test_counter = 7;
const int module_test_limit = 99;

int module_test_add(int left, int right) { return left + right; }

struct module_static_fixture {
  inline static int value = 4;

  static int multiply(int factor) { return value * factor; }
};

} // namespace

TEST_CASE("meta - module registers global properties and functions", "[meta]") {
  module_test_counter = 7;

  auto builder = caors::meta::module("caors.test.meta.module.global")
                     .property("counter", &module_test_counter)
                     .property("limit", &module_test_limit)
                     .function("add", &module_test_add);
  REQUIRE(builder.succeeded());

  const caors::meta::module_view module = caors::meta::find_module("caors.test.meta.module.global");
  REQUIRE(module);
  CHECK(module.property_count() == 2);
  CHECK(module.method_count() == 1);

  const caors::meta::property counter = module.find_property("counter");
  REQUIRE(counter);
  CHECK_FALSE(counter.get_owner_type());
  CHECK(counter.writable());

  caors::any counter_value = counter.get_any();
  REQUIRE(counter_value);
  REQUIRE(counter_value.try_get<int>() != nullptr);
  CHECK(*counter_value.try_get<int>() == 7);

  CHECK(counter.try_set_any(caors::any::from(11)));
  CHECK(module_test_counter == 11);

  const caors::meta::property limit = module.find_property("limit");
  REQUIRE(limit);
  CHECK_FALSE(limit.writable());
  CHECK_FALSE(limit.try_set_any(caors::any::from(12)));
  CHECK(module_test_limit == 99);

  const caors::meta::method_set add = module.find_method("add");
  REQUIRE(add);
  CHECK(add.overload_count() == 1);
  const caors::meta::method add_method = add.single();
  REQUIRE(add_method);
  CHECK_FALSE(add_method.get_owner_type());
  REQUIRE(add_method.parameter_count() == 2);

  int left = 3;
  int right = 5;
  std::array<caors::argument, 2> arguments{caors::arg(left), caors::arg(right)};
  const caors::meta::invoke_result add_result = add.invoke_result_of(arguments);
  REQUIRE(add_result.has_value());
  REQUIRE(add_result.try_get<int>() != nullptr);
  CHECK(*add_result.try_get<int>() == 8);
}

TEST_CASE("meta - module registers static properties and functions", "[meta]") {
  module_static_fixture::value = 4;

  auto builder = caors::meta::module("caors.test.meta.module.static")
                     .property("value", &module_static_fixture::value)
                     .function("multiply", &module_static_fixture::multiply);
  REQUIRE(builder.succeeded());

  const caors::meta::module_view module = caors::meta::find_module("caors.test.meta.module.static");
  REQUIRE(module);

  const caors::meta::property value = module.find_property("value");
  REQUIRE(value);
  CHECK(value.try_set_any(caors::any::from(6)));
  CHECK(module_static_fixture::value == 6);

  int factor = 7;
  std::array<caors::argument, 1> arguments{caors::arg(factor)};
  const caors::meta::invoke_result result =
      module.find_method("multiply").invoke_result_of(arguments);
  REQUIRE(result.has_value());
  REQUIRE(result.try_get<int>() != nullptr);
  CHECK(*result.try_get<int>() == 42);
}

TEST_CASE("meta - module registers lambda and function object callables", "[meta]") {
  int base = 10;
  int referenced = 5;

  struct multiplier {
    int factor = 1;

    int operator()(int value) const { return value * factor; }
  };

  auto builder = caors::meta::module("caors.test.meta.module.callable")
                     .function("add_base", [base](int value) { return base + value; })
                     .function("set_reference", [&referenced](int value) { referenced = value; })
                     .function("reference", [&referenced]() -> int& { return referenced; })
                     .function("multiply", multiplier{3});
  REQUIRE(builder.succeeded());

  const caors::meta::module_view module =
      caors::meta::find_module("caors.test.meta.module.callable");
  REQUIRE(module);
  CHECK(module.method_count() == 4);

  int value = 7;
  std::array<caors::argument, 1> arguments{caors::arg(value)};

  caors::meta::invoke_result add_result =
      module.find_method("add_base").invoke_result_of(arguments);
  REQUIRE(add_result.has_value());
  REQUIRE(add_result.value().try_get<int>() != nullptr);
  CHECK(*add_result.value().try_get<int>() == 17);

  caors::meta::invoke_result void_result =
      module.find_method("set_reference").invoke_result_of(arguments);
  REQUIRE(void_result.succeeded());
  CHECK(void_result.kind() == caors::meta::invoke_result_kind::void_value);
  CHECK(referenced == 7);

  caors::meta::invoke_result ref_result = module.find_method("reference").invoke_result_of();
  REQUIRE(ref_result.has_reference());
  REQUIRE(ref_result.ref().try_get_mutable<int>() != nullptr);
  *ref_result.ref().try_get_mutable<int>() = 12;
  CHECK(referenced == 12);

  caors::meta::invoke_result multiply_result =
      module.find_method("multiply").invoke_result_of(arguments);
  REQUIRE(multiply_result.has_value());
  REQUIRE(multiply_result.try_get<int>() != nullptr);
  CHECK(*multiply_result.try_get<int>() == 21);
}
