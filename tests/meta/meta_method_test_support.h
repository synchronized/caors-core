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

struct default_argument_actor {
  int value = 0;

  int add(int left, int right) {
    value = left + right;
    return value;
  }
};

struct default_argument_mismatch_actor {
  int add(int left, int right) { return left + right; }
};

struct named_argument_actor {
  int value = 0;

  int add(int left, int right) {
    value = left + right;
    return value;
  }
};

struct named_default_argument_actor {
  int value = 0;

  int add(int left, int right, int scale) {
    value = (left + right) * scale;
    return value;
  }
};

struct default_argument_overload_actor {
  int value = 0;

  int choose(int value) { return value; }
  int choose(int value, int amount) { return value + amount; }
};

struct move_only_return_value {
  explicit move_only_return_value(int input) : value(input) {}
  move_only_return_value(const move_only_return_value&) = delete;
  move_only_return_value& operator=(const move_only_return_value&) = delete;
  move_only_return_value(move_only_return_value&& other) noexcept : value(other.value) {
    other.value = 0;
  }
  move_only_return_value& operator=(move_only_return_value&& other) noexcept {
    value = other.value;
    other.value = 0;
    return *this;
  }

  int value = 0;
};

struct move_only_return_actor {
  move_only_return_value make(int value) { return move_only_return_value{value}; }
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
