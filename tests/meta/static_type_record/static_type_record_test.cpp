// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/adapters/stl_traits.h"
#include "caors/adapters/stl_wrapper.h"
#include "caors/meta/query.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace {

enum class static_type_enum : int {
  negative = -1,
  positive = 7,
};

template <typename Ops, typename T> [[nodiscard]] const Ops* ops_of() {
  const caors::meta::type type = caors::meta::resolve<T>();
  REQUIRE(type.valid());
  return type.get_raw_ops<Ops>();
}

template <typename T> [[nodiscard]] caors::type_id type_id_of() {
  return caors::meta::resolve<T>().id();
}

} // namespace

TEST_CASE("meta static type data - detects incompatible runtime models", "[meta]") {
  const caors::meta::detail::type_record* expected =
      caors::meta::detail::static_type_record_ptr_of<int>();
  REQUIRE(expected != nullptr);
  CHECK(expected->static_model_matches(*expected));

  caors::meta::value_storage_ops incompatible_value = expected->value_ops();
  ++incompatible_value.layout.size;
  const caors::meta::detail::type_record incompatible{
      expected->id(),
      expected->name(),
      expected->guid(),
      incompatible_value,
      expected->semantics(),
      expected->builtin(),
      nullptr,
      0,
      false,
  };

  CHECK_FALSE(expected->static_model_matches(incompatible));
}

TEST_CASE("meta static type data - scalar capability describes primitive aliases and strings",
          "[meta]") {
  const caors::meta::type int_type = caors::meta::resolve<int>();
  REQUIRE(int_type.valid());
  CHECK(int_type.kind() == caors::meta::type_kind::value);
  CHECK(int_type.is_scalar());
  CHECK(int_type.scalar() == caors::meta::scalar_kind::signed_integer);

  const auto* int_ops = ops_of<caors::meta::scalar_ops, int>();
  REQUIRE(int_ops != nullptr);
  CHECK(int_ops->size == sizeof(int));
  CHECK(int_ops->is_signed);

  int number = 32;
  std::int64_t signed_value = 0;
  REQUIRE(int_ops->to_i64 != nullptr);
  CHECK(int_ops->to_i64(&number, signed_value));
  CHECK(signed_value == 32);

  if constexpr (std::is_same_v<std::int32_t, int>) {
    CHECK(caors::meta::resolve<std::int32_t>().id() == int_type.id());
  } else {
    const caors::meta::type i32_type = caors::meta::resolve<std::int32_t>();
    CHECK(i32_type.is_scalar());
    CHECK(i32_type.scalar() == caors::meta::scalar_kind::signed_integer);
  }

  const caors::meta::type std_string_type = caors::meta::resolve<std::string>();
  const caors::meta::type caors_string_type = caors::meta::resolve<caors::string>();
  REQUIRE(std_string_type.valid());
  REQUIRE(caors_string_type.valid());
  CHECK(std_string_type.id() != caors_string_type.id());
  CHECK(std_string_type.scalar() == caors::meta::scalar_kind::string);
  CHECK(caors_string_type.scalar() == caors::meta::scalar_kind::string);

  caors::string text = "Ada";
  const auto* string_ops = ops_of<caors::meta::scalar_ops, caors::string>();
  REQUIRE(string_ops != nullptr);
  REQUIRE(string_ops->to_string != nullptr);
  std::string_view view;
  CHECK(string_ops->to_string(&text, view));
  CHECK(view == "Ada");
}

TEST_CASE("meta static type data - enum capability describes integer conversion", "[meta]") {
  const caors::meta::type type = caors::meta::resolve<static_type_enum>();
  REQUIRE(type.valid());
  CHECK(type.kind() == caors::meta::type_kind::enumeration);

  const auto* ops = ops_of<caors::meta::enum_ops, static_type_enum>();
  REQUIRE(ops != nullptr);
  CHECK(ops->is_signed);

  static_type_enum value = static_type_enum::positive;
  CHECK(ops->to_i64(&value) == 7);
  CHECK(ops->from_i64(&value, -1));
  CHECK(value == static_type_enum::negative);
}

TEST_CASE("meta static type data - indirect capability describes raw pointers and wrappers",
          "[meta]") {
  int value = 42;
  int* pointer = &value;

  const caors::meta::type pointer_type = caors::meta::resolve<int*>();
  REQUIRE(pointer_type.valid());
  CHECK(pointer_type.kind() == caors::meta::type_kind::raw_pointer);

  const auto* pointer_ops = ops_of<caors::meta::indirect_ops, int*>();
  REQUIRE(pointer_ops != nullptr);
  REQUIRE(pointer_ops->get_value_type != nullptr);
  CHECK(pointer_ops->get_value_type().id() == type_id_of<int>());
  CHECK(pointer_ops->has_value(&pointer));
  CHECK(pointer_ops->value(&pointer) == &value);

  std::optional<int> optional{9};
  const caors::meta::type optional_type = caors::meta::resolve<std::optional<int>>();
  REQUIRE(optional_type.valid());
  CHECK(optional_type.kind() == caors::meta::type_kind::wrapper);

  const auto* optional_ops = ops_of<caors::meta::indirect_ops, std::optional<int>>();
  REQUIRE(optional_ops != nullptr);
  REQUIRE(optional_ops->get_value_type != nullptr);
  CHECK(optional_ops->get_value_type().id() == type_id_of<int>());
  CHECK(optional_ops->has_value(&optional));
  CHECK(*static_cast<const int*>(optional_ops->value(&optional)) == 9);
}

TEST_CASE("meta static type data - sequence set and associative capabilities describe containers",
          "[meta]") {
  const auto* sequence_ops = ops_of<caors::meta::sequence_ops, std::vector<int>>();
  REQUIRE(sequence_ops != nullptr);
  REQUIRE(sequence_ops->get_value_type != nullptr);
  CHECK(sequence_ops->get_value_type().id() == type_id_of<int>());

  std::vector<int> values{1, 2, 3};
  CHECK(sequence_ops->size(&values) == 3);
  REQUIRE(sequence_ops->element(&values, 1) != nullptr);
  CHECK(*static_cast<const int*>(sequence_ops->element(&values, 1)) == 2);

  const auto* set_ops = ops_of<caors::meta::set_ops, std::set<int>>();
  REQUIRE(set_ops != nullptr);
  REQUIRE(set_ops->get_value_type != nullptr);
  CHECK(set_ops->get_value_type().id() == type_id_of<int>());

  std::set<int> set_values{4, 5};
  CHECK(set_ops->size(&set_values) == 2);

  const auto* associative_ops = ops_of<caors::meta::associative_ops, std::map<int, std::string>>();
  REQUIRE(associative_ops != nullptr);
  REQUIRE(associative_ops->get_key_type != nullptr);
  REQUIRE(associative_ops->get_value_type != nullptr);
  CHECK(associative_ops->get_key_type().id() == type_id_of<int>());
  CHECK(associative_ops->get_value_type().id() == type_id_of<std::string>());

  std::map<int, std::string> map_values{{7, "seven"}};
  CHECK(associative_ops->size(&map_values) == 1);
}

TEST_CASE("meta static type data - pair tuple and variant capabilities describe heterogeneous values",
          "[meta]") {
  using pair_type = std::pair<int, std::string>;
  const auto* pair_ops = ops_of<caors::meta::pair_ops, pair_type>();
  REQUIRE(pair_ops != nullptr);
  REQUIRE(pair_ops->get_key_type != nullptr);
  REQUIRE(pair_ops->get_value_type != nullptr);
  CHECK(pair_ops->get_key_type().id() == type_id_of<int>());
  CHECK(pair_ops->get_value_type().id() == type_id_of<std::string>());

  pair_type pair{3, "three"};
  CHECK(*static_cast<const int*>(pair_ops->first(&pair)) == 3);
  CHECK(*static_cast<const std::string*>(pair_ops->second(&pair)) == "three");

  using tuple_type = std::tuple<int, std::string, double>;
  const auto* tuple_ops = ops_of<caors::meta::tuple_ops, tuple_type>();
  REQUIRE(tuple_ops != nullptr);
  REQUIRE(tuple_ops->elements.size() == 3);
  REQUIRE(tuple_ops->elements[0].get_value_type != nullptr);
  REQUIRE(tuple_ops->elements[1].get_value_type != nullptr);
  REQUIRE(tuple_ops->elements[2].get_value_type != nullptr);
  CHECK(tuple_ops->elements[0].get_value_type().id() == type_id_of<int>());
  CHECK(tuple_ops->elements[1].get_value_type().id() == type_id_of<std::string>());
  CHECK(tuple_ops->elements[2].get_value_type().id() == type_id_of<double>());

  tuple_type tuple{1, "one", 1.5};
  CHECK(*static_cast<const int*>(tuple_ops->elements[0].element(&tuple)) == 1);
  CHECK(*static_cast<const std::string*>(tuple_ops->elements[1].element(&tuple)) == "one");

  using variant_type = std::variant<int, std::string>;
  const auto* variant_ops = ops_of<caors::meta::variant_ops, variant_type>();
  REQUIRE(variant_ops != nullptr);
  REQUIRE(variant_ops->alternatives.size() == 2);
  REQUIRE(variant_ops->alternatives[0].get_value_type != nullptr);
  REQUIRE(variant_ops->alternatives[1].get_value_type != nullptr);
  CHECK(variant_ops->alternatives[0].get_value_type().id() == type_id_of<int>());
  CHECK(variant_ops->alternatives[1].get_value_type().id() == type_id_of<std::string>());

  variant_type variant{std::string{"text"}};
  CHECK(variant_ops->index(&variant) == 1);
  REQUIRE(variant_ops->alternatives[1].alternative(&variant) != nullptr);
  CHECK(*static_cast<const std::string*>(variant_ops->alternatives[1].alternative(&variant)) ==
        "text");
}
