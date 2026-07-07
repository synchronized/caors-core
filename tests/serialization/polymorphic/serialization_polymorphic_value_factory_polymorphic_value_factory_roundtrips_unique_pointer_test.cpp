// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

TEST_CASE("serialization - polymorphic value factory roundtrips unique pointer",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 42;
  player->name = "Una";
  holder.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 42);
  CHECK(decoded_owner->name == "Una");
}
