// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TESTS_REFLECTION_TEST_SUPPORT_H_
#define CAORS_TESTS_REFLECTION_TEST_SUPPORT_H_
#include "caors/adapters/stl.h"
#include "caors/object/base.h"
#include "caors/reflection.h"
#include "caors/traits/type_name.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace test_reflection {

template <typename T> struct custom_inline_wrapper {
  T value{};
};

struct named_field_value {
  int value = 0;
};

struct named_field_owner {
  named_field_value value{};
};

template <typename T> struct custom_sequence {
  caors::vector<T> values{};
};

} // namespace test_reflection

namespace caors::traits {

template <typename T> struct wrapper_traits<test_reflection::custom_inline_wrapper<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = T;
};

template <> struct type_name<test_reflection::named_field_value> {
  static constexpr std::string_view value = "caors.test.reflection.named_field_value";
};

} // namespace caors::traits

namespace caors::traits {

template <typename T> struct sequence_traits<test_reflection::custom_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "test.custom_sequence";
  }

  [[nodiscard]] static std::size_t
  size(const test_reflection::custom_sequence<T>& values) noexcept {
    return values.values.size();
  }

  [[nodiscard]] static const element_type*
  element(const test_reflection::custom_sequence<T>& values, std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(test_reflection::custom_sequence<T>& values,
                                                     std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  static void clear(test_reflection::custom_sequence<T>& values) { values.values.clear(); }

  [[nodiscard]] static bool resize(test_reflection::custom_sequence<T>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool append(test_reflection::custom_sequence<T>& values,
                                   const element_type& value) {
    if constexpr (std::is_copy_constructible_v<T>) {
      values.values.push_back(value);
      return true;
    } else {
      return false;
    }
  }
};

} // namespace caors::traits

namespace test_reflection {

struct actor {
  int id = 0;
  float health = 0.0F;
};

struct registry_actor {
  int id = 0;
  float health = 0.0F;
};

struct explicit_meta_actor {
  int id = 0;
};

struct view_actor {
  int id = 0;
  float health = 0.0F;
};

struct parent_actor {
  int id = 0;
};

struct child_player {
  int level = 0;
};

struct duplicate_actor {
  int id = 0;
};

struct invalid_field_owner {
  int id = 0;
};

struct duplicate_field_owner {
  int id = 0;
  float health = 0.0F;
};

struct inconsistent_actor {
  int id = 0;
};

struct builder_actor {
  int id = 0;
  float health = 0.0F;
};

struct inventory {
  int count = 0;
};

struct reflected_object_ref_target : caors::object {};

struct inventory_holder {
  inventory items{};
};

struct vector_holder {
  caors::vector<int> values{};
};

struct list_holder {
  caors::list<int> values{};
};

struct array_holder {
  std::array<int, 3> values{};
};

struct span_holder {
  std::span<int> values{};
};

struct fixed_span_holder {
  std::span<int, 3> values;
};

struct pair_holder {
  std::pair<int, caors::string> value{};
};

struct tuple_holder {
  std::tuple<int, caors::string, float> value{};
};

struct variant_holder {
  std::variant<int, caors::string, float> value{};
};

struct custom_sequence_holder {
  test_reflection::custom_sequence<int> values{};
};

struct hash_map_holder {
  caors::hash_map<int, int> values{};
};

struct hash_map_ops_holder {
  int prefix = 0;
  caors::hash_map<int, int> values{};
};

struct hash_set_holder {
  caors::hash_set<int> values{};
};

struct readonly_actor {
  const int id = 7;
};

enum class reflected_color {
  red,
  blue,
};

constexpr caors::type_guid registry_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x01})};

constexpr caors::type_guid view_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x02})};

[[nodiscard]] inline caors::type_id registered_type_id(std::string_view name) {
  const auto type = caors::meta::find(name);
  REQUIRE(type);
  return type.id();
}

[[nodiscard]] constexpr caors::error reflection_error(caors::errc code) noexcept {
  return {caors::error_domain::reflection, code};
}

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

template <typename T> void require_registered_type(std::string_view name) {
  const caors::error status = caors::meta::type_<T>(name).result();
  REQUIRE((status == caors::error{} || status == meta_error(caors::errc::duplicate_id)));
}

template <typename T> void register_common_type(std::string_view name) {
  const caors::error status = caors::meta::type_<T>(name).result();
  (void)status;
}

[[maybe_unused]] const bool common_reflection_types_registered = [] {
  register_common_type<int>("int");
  register_common_type<float>("float");
  register_common_type<caors::string>("caors.string");
  register_common_type<reflected_color>("caors.test.reflection.color");
  register_common_type<int*>("int*");
  register_common_type<std::unique_ptr<int>>("std.unique_ptr<int>");
  register_common_type<std::shared_ptr<int>>("std.shared_ptr<int>");
  register_common_type<std::weak_ptr<int>>("std.weak_ptr<int>");
  register_common_type<std::optional<int>>("std.optional<int>");
  register_common_type<std::reference_wrapper<int>>("std.reference_wrapper<int>");
  register_common_type<custom_inline_wrapper<int>>("custom.inline<int>");
  register_common_type<reflected_object_ref_target>(
      "caors.test.reflection.reflected_object_ref_target");
  register_common_type<caors::object_ref<reflected_object_ref_target>>(
      "caors.object_ref<caors.test.reflection.reflected_object_ref_target>");
  register_common_type<caors::vector<int>>("caors.vector<int>");
  register_common_type<caors::list<int>>("caors.list<int>");
  register_common_type<std::array<int, 3>>("std.array<int,3>");
  register_common_type<std::span<int>>("std.span<int>");
  register_common_type<std::span<int, 3>>("std.span<int,3>");
  register_common_type<std::pair<int, caors::string>>("std.pair<int,caors.string>");
  register_common_type<std::tuple<int, caors::string, float>>("std.tuple<int,caors.string,float>");
  register_common_type<std::variant<int, caors::string, float>>(
      "std.variant<int,caors.string,float>");
  register_common_type<test_reflection::custom_sequence<int>>("test.custom_sequence<int>");
  register_common_type<caors::hash_map<int, int>>("caors.hash_map<int,int>");
  register_common_type<caors::hash_set<int>>("caors.hash_set<int>");
  register_common_type<inventory>("caors.test.reflection.inventory");
  return true;
}();

} // namespace test_reflection

#endif
