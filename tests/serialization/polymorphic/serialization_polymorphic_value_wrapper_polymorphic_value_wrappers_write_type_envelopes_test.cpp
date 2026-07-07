// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value wrappers write type envelopes", "[serialization]") {
  require_polymorphic_value_metadata();

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 17;
  owner->name = "Ada";
  holder.owner = std::move(owner);

  auto shared = std::make_shared<polymorphic_value_player>();
  shared->level = 23;
  shared->name = "Mina";
  holder.shared = std::move(shared);

  const caors::serialization::variant_result value = caors::to_serialization_value(holder);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* owner_member =
      value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner_member != nullptr);
  REQUIRE(owner_member->value.is_object());

  const caors::value::variant_member* type_member =
      owner_member->value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player_guid.value.string()});

  const caors::value::variant_member* stored_value =
      owner_member->value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);
}
