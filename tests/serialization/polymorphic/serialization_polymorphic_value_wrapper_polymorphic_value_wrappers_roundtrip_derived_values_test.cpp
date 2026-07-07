// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value wrappers roundtrip derived values",
          "[serialization]") {
  require_polymorphic_value_metadata();

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 31;
  owner->name = "Lin";
  holder.owner = std::move(owner);

  auto shared = std::make_shared<polymorphic_value_player>();
  shared->level = 37;
  shared->name = "Nia";
  holder.shared = std::move(shared);

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  polymorphic_value_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_value_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 31);
  CHECK(decoded_owner->name == "Lin");

  auto* decoded_shared = dynamic_cast<polymorphic_value_player*>(decoded.shared.get());
  REQUIRE(decoded_shared != nullptr);
  CHECK(decoded_shared->level == 37);
  CHECK(decoded_shared->name == "Nia");
}
