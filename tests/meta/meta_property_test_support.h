// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TESTS_META_PROPERTY_TEST_SUPPORT_H_
#define CAORS_TESTS_META_PROPERTY_TEST_SUPPORT_H_
#include "caors/adapters/stl.h"
#include "caors/meta.h"
#include "caors/object/base.h"
#include "caors/object/ptr.h"
#include "caors/registration.h"
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

namespace test_meta {

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

} // namespace test_meta

namespace caors::traits {

template <typename T> struct wrapper_traits<test_meta::custom_inline_wrapper<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = T;
};

template <> struct type_name<test_meta::named_field_value> {
  static constexpr std::string_view value = "caors.test.meta.named_field_value";
};

} // namespace caors::traits

namespace caors::traits {

template <typename T> struct sequence_traits<test_meta::custom_sequence<T>> {
  static constexpr bool is_sequence = true;
  using element_type = std::remove_cv_t<T>;

  [[nodiscard]] static constexpr std::string_view type_name_prefix() noexcept {
    return "test.custom_sequence";
  }

  [[nodiscard]] static std::size_t size(const test_meta::custom_sequence<T>& values) noexcept {
    return values.values.size();
  }

  [[nodiscard]] static const element_type* element(const test_meta::custom_sequence<T>& values,
                                                   std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  [[nodiscard]] static element_type* mutable_element(test_meta::custom_sequence<T>& values,
                                                     std::size_t index) noexcept {
    return index < values.values.size() ? &values.values[index] : nullptr;
  }

  static void clear(test_meta::custom_sequence<T>& values) { values.values.clear(); }

  [[nodiscard]] static bool resize(test_meta::custom_sequence<T>& values, std::size_t size) {
    if constexpr (std::is_default_constructible_v<T>) {
      values.values.resize(size);
      return true;
    } else {
      return false;
    }
  }

  [[nodiscard]] static bool append(test_meta::custom_sequence<T>& values,
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

namespace test_meta {

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

  int base_score() const { return id + 1; }
};

struct child_player : parent_actor {
  int level = 0;

  int child_score() const { return level + 1; }
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

struct reflected_object_ptr_target : caors::object {};

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
  test_meta::custom_sequence<int> values{};
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

struct field_layout_actor {
  int id = 0;
  float health = 0.0F;
};

struct field_traits_owner {
  int value = 0;
  reflected_color color = reflected_color::red;
  int* raw = nullptr;
  std::unique_ptr<int> unique{};
  std::shared_ptr<int> shared{};
  std::weak_ptr<int> weak{};
  std::optional<int> optional{};
  std::reference_wrapper<int> wrapper{value};
  custom_inline_wrapper<int> custom_inline{};
  caors::object_ptr<reflected_object_ptr_target> object_ptr{};
  caors::vector<int> sequence{};
  std::span<int> span{};
  std::pair<int, caors::string> pair{};
  std::tuple<int, caors::string, float> tuple{};
  std::variant<int, caors::string, float> variant{};
};

struct read_write_actor {
  int id = 0;
  float health = 0.0F;
};

struct stored_fields_actor {
  int id = 0;
  float health = 0.0F;
};

[[nodiscard]] inline caors::meta::type require_reflected(std::string_view name) {
  auto reflected = caors::meta::find(name);
  REQUIRE(reflected);
  return reflected;
}

[[nodiscard]] inline caors::meta::property require_property(const caors::meta::type& type,
                                                            std::string_view name) {
  auto field = type.find_property(name);
  REQUIRE(field);
  return field;
}

constexpr caors::type_guid registry_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x01})};

constexpr caors::type_guid view_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x49, 0xd0, 0x23, 0x6f, 0x74, 0xb5, 0x41, 0x3f, 0x99, 0x37, 0xfa, 0x9f,
                            0xf8, 0xa6, 0x36, 0x02})};

[[nodiscard]] inline caors::type_id registered_type_id(std::string_view name) {
  const auto type = caors::meta::find(name);
  if (type) {
    return type.id();
  }

  if (name == "int") {
    return caors::meta::resolve<int>().id();
  }
  if (name == "float") {
    return caors::meta::resolve<float>().id();
  }
  if (name == "caors.string") {
    return caors::meta::resolve<caors::string>().id();
  }
  if (name == "caors.vector<int>") {
    return caors::meta::resolve<caors::vector<int>>().id();
  }
  if (name == "caors.list<int>") {
    return caors::meta::resolve<caors::list<int>>().id();
  }
  if (name == "std.array<int,3>") {
    return caors::meta::resolve<std::array<int, 3>>().id();
  }
  if (name == "std.span<int>") {
    return caors::meta::resolve<std::span<int>>().id();
  }
  if (name == "std.span<int,3>") {
    return caors::meta::resolve<std::span<int, 3>>().id();
  }
  if (name == "std.pair<int,caors.string>") {
    return caors::meta::resolve<std::pair<int, caors::string>>().id();
  }
  if (name == "std.tuple<int,caors.string,float>") {
    return caors::meta::resolve<std::tuple<int, caors::string, float>>().id();
  }
  if (name == "std.variant<int,caors.string,float>") {
    return caors::meta::resolve<std::variant<int, caors::string, float>>().id();
  }
  if (name == "test.custom_sequence<int>") {
    return caors::meta::resolve<test_meta::custom_sequence<int>>().id();
  }
  if (name == "caors.hash_map<int,int>") {
    return caors::meta::resolve<caors::hash_map<int, int>>().id();
  }
  if (name == "caors.hash_set<int>") {
    return caors::meta::resolve<caors::hash_set<int>>().id();
  }
  if (name == "caors.test.meta.color") {
    return caors::meta::resolve<reflected_color>().id();
  }
  if (name == "caors.object_ptr<caors.test.meta.reflected_object_ptr_target>") {
    return caors::meta::resolve<caors::object_ptr<reflected_object_ptr_target>>().id();
  }

  FAIL("unknown test type name");
  return caors::type_id::invalid();
}

[[nodiscard]] constexpr caors::error meta_error(caors::errc code) noexcept {
  return {caors::error_domain::meta, code};
}

[[nodiscard]] inline caors::meta::type require_property_traits_owner_meta() {
  const caors::error status =
      caors::registration::type_<field_traits_owner>("caors.test.meta.field_traits_owner")
          .meta()
          .property("value", &field_traits_owner::value)
          .property("color", &field_traits_owner::color)
          .property("raw", &field_traits_owner::raw)
          .property("unique", &field_traits_owner::unique)
          .property("shared", &field_traits_owner::shared)
          .property("weak", &field_traits_owner::weak)
          .property("optional", &field_traits_owner::optional)
          .property("wrapper", &field_traits_owner::wrapper)
          .property("custom_inline", &field_traits_owner::custom_inline)
          .property("object_ptr", &field_traits_owner::object_ptr)
          .property("sequence", &field_traits_owner::sequence)
          .property("span", &field_traits_owner::span)
          .property("pair", &field_traits_owner::pair)
          .property("tuple", &field_traits_owner::tuple)
          .property("variant", &field_traits_owner::variant)
          .commit();

  REQUIRE((status == caors::error{} || status == meta_error(caors::errc::duplicate_id)));
  return require_reflected("caors.test.meta.field_traits_owner");
}

template <typename T> void require_registered_type(std::string_view name) {
  const caors::error status = caors::registration::type_<T>(name).commit();
  REQUIRE((status == caors::error{} || status == meta_error(caors::errc::duplicate_id)));
}

template <typename T> void register_common_type(std::string_view name) {
  const caors::error status = caors::registration::type_<T>(name).commit();
  (void)status;
}

[[maybe_unused]] const bool common_meta_types_registered = [] {
  register_common_type<reflected_color>("caors.test.meta.color");
  register_common_type<reflected_object_ptr_target>("caors.test.meta.reflected_object_ptr_target");
  register_common_type<caors::object_ptr<reflected_object_ptr_target>>(
      "caors.object_ptr<caors.test.meta.reflected_object_ptr_target>");
  return true;
}();

} // namespace test_meta

#endif
