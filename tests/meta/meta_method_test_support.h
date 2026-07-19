// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "meta_property_test_support.h"

namespace caors::tests::meta_method {

using namespace test_meta;

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

struct overloaded_method_actor {
  int value = 0;

  void update(int amount) { value += amount; }
  void update(float amount) { value += static_cast<int>(amount); }
  [[nodiscard]] int update() const { return value; }
};

struct rvalue_method_actor {
  int value = 0;

  void take(int&& amount) { value = amount; }
};

struct invoke_result_actor {
  int value = 0;

  void set(int next) { value = next; }
  int score() const { return value * 3; }
  int& ref() { return value; }
  const int& cref() const { return value; }
};

[[nodiscard]] inline caors::meta::type require_method_type() {
  const caors::error result =
      caors::registration::type_<method_actor>("caors.test.meta.method_actor")
          .meta()
          .property("value", &method_actor::value)
          .method("add", &method_actor::add)
          .method("add_to", &method_actor::add_to)
          .method("score", &method_actor::score)
          .commit();
  REQUIRE((result == caors::error{} || result == meta_error(caors::errc::duplicate_id)));

  auto reflected = caors::meta::find("caors.test.meta.method_actor");
  REQUIRE(reflected);
  return reflected;
}

[[nodiscard]] inline caors::meta::method require_method(const caors::meta::type& type,
                                                        std::string_view name) {
  auto method_set = type.find_method(name);
  REQUIRE(method_set);
  auto method = method_set.single();
  REQUIRE(method);
  return method;
}

} // namespace caors::tests::meta_method
