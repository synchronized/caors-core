// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value envelope names are configurable", "[serialization]") {
  require_polymorphic_value_metadata();

  caors::serialization::context context{};
  context.polymorphic.type_field = caors::string_name{"kind"};
  context.polymorphic.value_field = caors::string_name{"data"};
  context.polymorphic.type_name_field = caors::string_name{"debug_type"};
  context.polymorphic.write_type_name = true;

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 29;
  owner->name = "Uma";
  holder.owner = std::move(owner);

  const caors::serialization::variant_result value = caors::to_serialization_value(holder, context);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* owner_member =
      value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner_member != nullptr);
  REQUIRE(owner_member->value.is_object());

  CHECK(owner_member->value.find_member(caors::string_name{"$type"}) == nullptr);
  CHECK(owner_member->value.find_member(caors::string_name{"$value"}) == nullptr);
  CHECK(owner_member->value.find_member(caors::string_name{"kind"}) != nullptr);
  CHECK(owner_member->value.find_member(caors::string_name{"debug_type"}) != nullptr);

  const caors::value::variant_member* stored_value =
      owner_member->value.find_member(caors::string_name{"data"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(holder, context);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("kind") != caors::string::npos);
  CHECK(json.value.find("data") != caors::string::npos);
  CHECK(json.value.find("debug_type") != caors::string::npos);

  polymorphic_value_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_value_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 29);
  CHECK(decoded_owner->name == "Uma");
}
