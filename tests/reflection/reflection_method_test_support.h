// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "reflection_test_support.h"

namespace caors::tests::reflection_method {

using namespace test_reflection;

struct method_actor {
  int value = 0;

  void add(int amount) { value += amount; }

  void add_to(int& target) { target += value; }

  [[nodiscard]] int score() const { return value * 2; }
};

struct duplicate_method_actor {
  void first() {}
  void second() {}
};

[[nodiscard]] inline caors::reflection::type require_method_type() {
  const caors::error result =
      ((void)caors::meta::class_<method_actor>("caors.test.reflection.method_actor"),
       caors::reflection::class_<method_actor>())
          .field("value", &method_actor::value)
          .method("add", &method_actor::add)
          .method("add_to", &method_actor::add_to)
          .method("score", &method_actor::score)
          .commit();
  REQUIRE(
      (result == caors::error{} || result == reflection_error(caors::errc::duplicate_reflection)));

  auto reflected = caors::reflection::find("caors.test.reflection.method_actor");
  REQUIRE(reflected);
  return reflected;
}

[[nodiscard]] inline caors::reflection::method require_method(const caors::reflection::type& type,
                                                              std::string_view name) {
  auto method = type.find_method(name);
  REQUIRE(method);
  return method;
}

} // namespace caors::tests::reflection_method
