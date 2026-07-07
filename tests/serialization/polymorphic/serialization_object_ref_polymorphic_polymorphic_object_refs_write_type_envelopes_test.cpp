// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ref_polymorphic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_polymorphic;

TEST_CASE("serialization - polymorphic object refs write type envelopes", "[serialization]") {
  require_polymorphic_object_ref_metadata();

  caors::objects::ref<polymorphic_object_ref_player> player =
      caors::objects::make_ref<polymorphic_object_ref_player>();
  player->level = 12;
  player->name = "Ada";

  polymorphic_object_ref_holder holder{};
  holder.owner = player;

  const caors::serialization::variant_result value = caors::to_serialization_value(holder);

  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* owner = value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner != nullptr);
  REQUIRE(owner->value.is_object());

  const caors::value::variant_member* type = owner->value.find_member(caors::string_name{"$type"});
  REQUIRE(type != nullptr);
  REQUIRE(type->value.is_string());
  CHECK(std::string_view{type->value.as_string().data(), type->value.as_string().size()} ==
        std::string_view{polymorphic_object_ref_player_guid.value.string()});

  const caors::value::variant_member* stored =
      owner->value.find_member(caors::string_name{"$value"});
  REQUIRE(stored != nullptr);
  REQUIRE(stored->value.is_object());
  CHECK(stored->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored->value.find_member(caors::string_name{"name"}) != nullptr);
}
