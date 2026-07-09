// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/adapters/stl_traits.h"
#include "caors/adapters/stl_wrapper.h"
#include "caors/meta/detail/type_descriptor_model.h"

#include <catch2/catch_all.hpp>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace {

enum class descriptor_enum : int {
  negative = -1,
  positive = 7,
};

template <typename Capability, typename T> [[nodiscard]] const Capability* capability_of() {
  const caors::meta::type_descriptor* descriptor = caors::meta::detail::descriptor_ptr_of<T>();
  REQUIRE(descriptor != nullptr);
  REQUIRE(descriptor->valid());
  return caors::meta::detail::descriptor_capability<Capability>(*descriptor);
}

} // namespace

TEST_CASE("meta descriptor - enum capability describes integer conversion", "[meta]") {
  const auto* descriptor = caors::meta::detail::descriptor_ptr_of<descriptor_enum>();
  REQUIRE(descriptor != nullptr);
  CHECK(caors::meta::detail::descriptor_kind(*descriptor) == caors::meta::type_kind::enumeration);

  const auto* ops = capability_of<caors::meta::enum_ops, descriptor_enum>();
  REQUIRE(ops != nullptr);
  CHECK(ops->is_signed);

  descriptor_enum value = descriptor_enum::positive;
  CHECK(ops->to_i64(&value) == 7);
  CHECK(ops->from_i64(&value, -1));
  CHECK(value == descriptor_enum::negative);
}

TEST_CASE("meta descriptor - indirect capability describes raw pointers and wrappers", "[meta]") {
  int value = 42;
  int* pointer = &value;

  const auto* pointer_descriptor = caors::meta::detail::descriptor_ptr_of<int*>();
  REQUIRE(pointer_descriptor != nullptr);
  CHECK(caors::meta::detail::descriptor_kind(*pointer_descriptor) ==
        caors::meta::type_kind::raw_pointer);

  const auto* pointer_ops = capability_of<caors::meta::indirect_ops, int*>();
  REQUIRE(pointer_ops != nullptr);
  REQUIRE(pointer_ops->value_descriptor != nullptr);
  CHECK(pointer_ops->value_descriptor->type == caors::meta::detail::descriptor_type_id<int>());
  CHECK(pointer_ops->has_value(&pointer));
  CHECK(pointer_ops->value(&pointer) == &value);

  std::optional<int> optional{9};
  const auto* optional_descriptor = caors::meta::detail::descriptor_ptr_of<std::optional<int>>();
  REQUIRE(optional_descriptor != nullptr);
  CHECK(caors::meta::detail::descriptor_kind(*optional_descriptor) ==
        caors::meta::type_kind::wrapper);

  const auto* optional_ops = capability_of<caors::meta::indirect_ops, std::optional<int>>();
  REQUIRE(optional_ops != nullptr);
  REQUIRE(optional_ops->value_descriptor != nullptr);
  CHECK(optional_ops->value_descriptor->type == caors::meta::detail::descriptor_type_id<int>());
  CHECK(optional_ops->has_value(&optional));
  CHECK(*static_cast<const int*>(optional_ops->value(&optional)) == 9);
}

TEST_CASE("meta descriptor - sequence set and associative capabilities describe containers",
          "[meta]") {
  const auto* sequence_ops = capability_of<caors::meta::sequence_ops, std::vector<int>>();
  REQUIRE(sequence_ops != nullptr);
  REQUIRE(sequence_ops->value_descriptor != nullptr);
  CHECK(sequence_ops->value_descriptor->type == caors::meta::detail::descriptor_type_id<int>());

  std::vector<int> values{1, 2, 3};
  CHECK(sequence_ops->size(&values) == 3);
  REQUIRE(sequence_ops->element(&values, 1) != nullptr);
  CHECK(*static_cast<const int*>(sequence_ops->element(&values, 1)) == 2);

  const auto* set_ops = capability_of<caors::meta::set_ops, std::set<int>>();
  REQUIRE(set_ops != nullptr);
  REQUIRE(set_ops->value_descriptor != nullptr);
  CHECK(set_ops->value_descriptor->type == caors::meta::detail::descriptor_type_id<int>());

  std::set<int> set_values{4, 5};
  CHECK(set_ops->size(&set_values) == 2);

  const auto* associative_ops =
      capability_of<caors::meta::associative_ops, std::map<int, std::string>>();
  REQUIRE(associative_ops != nullptr);
  REQUIRE(associative_ops->key_descriptor != nullptr);
  REQUIRE(associative_ops->value_descriptor != nullptr);
  CHECK(associative_ops->key_descriptor->type == caors::meta::detail::descriptor_type_id<int>());
  CHECK(associative_ops->value_descriptor->type ==
        caors::meta::detail::descriptor_type_id<std::string>());

  std::map<int, std::string> map_values{{7, "seven"}};
  CHECK(associative_ops->size(&map_values) == 1);
}

TEST_CASE("meta descriptor - pair tuple and variant capabilities describe heterogeneous values",
          "[meta]") {
  using pair_type = std::pair<int, std::string>;
  const auto* pair_ops = capability_of<caors::meta::pair_ops, pair_type>();
  REQUIRE(pair_ops != nullptr);
  REQUIRE(pair_ops->key_descriptor != nullptr);
  REQUIRE(pair_ops->value_descriptor != nullptr);
  CHECK(pair_ops->key_descriptor->type == caors::meta::detail::descriptor_type_id<int>());
  CHECK(pair_ops->value_descriptor->type == caors::meta::detail::descriptor_type_id<std::string>());

  pair_type pair{3, "three"};
  CHECK(*static_cast<const int*>(pair_ops->first(&pair)) == 3);
  CHECK(*static_cast<const std::string*>(pair_ops->second(&pair)) == "three");

  using tuple_type = std::tuple<int, std::string, double>;
  const auto* tuple_ops = capability_of<caors::meta::tuple_ops, tuple_type>();
  REQUIRE(tuple_ops != nullptr);
  REQUIRE(tuple_ops->elements.size() == 3);
  REQUIRE(tuple_ops->elements[0].value_descriptor != nullptr);
  REQUIRE(tuple_ops->elements[1].value_descriptor != nullptr);
  REQUIRE(tuple_ops->elements[2].value_descriptor != nullptr);
  CHECK(tuple_ops->elements[0].value_descriptor->type ==
        caors::meta::detail::descriptor_type_id<int>());
  CHECK(tuple_ops->elements[1].value_descriptor->type ==
        caors::meta::detail::descriptor_type_id<std::string>());
  CHECK(tuple_ops->elements[2].value_descriptor->type ==
        caors::meta::detail::descriptor_type_id<double>());

  tuple_type tuple{1, "one", 1.5};
  CHECK(*static_cast<const int*>(tuple_ops->elements[0].element(&tuple)) == 1);
  CHECK(*static_cast<const std::string*>(tuple_ops->elements[1].element(&tuple)) == "one");

  using variant_type = std::variant<int, std::string>;
  const auto* variant_ops = capability_of<caors::meta::variant_ops, variant_type>();
  REQUIRE(variant_ops != nullptr);
  REQUIRE(variant_ops->alternatives.size() == 2);
  REQUIRE(variant_ops->alternatives[0].value_descriptor != nullptr);
  REQUIRE(variant_ops->alternatives[1].value_descriptor != nullptr);
  CHECK(variant_ops->alternatives[0].value_descriptor->type ==
        caors::meta::detail::descriptor_type_id<int>());
  CHECK(variant_ops->alternatives[1].value_descriptor->type ==
        caors::meta::detail::descriptor_type_id<std::string>());

  variant_type variant{std::string{"text"}};
  CHECK(variant_ops->index(&variant) == 1);
  REQUIRE(variant_ops->alternatives[1].alternative(&variant) != nullptr);
  CHECK(*static_cast<const std::string*>(variant_ops->alternatives[1].alternative(&variant)) ==
        "text");
}
