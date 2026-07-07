// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object rejects wrong declared base", "[serialization]") {
  require_metadata();

  player player_value{};
  player_value.level = 3;

  const caors::serialization::variant_result value =
      caors::serialization::to_polymorphic_serialization_value(player_value);
  REQUIRE(value.succeeded());

  caors::objects::manager manager;
  caors::serialization::context context{};
  context.objects.manager = &manager;

  const caors::result<caors::objects::object*> decoded =
      caors::serialization::deserialize_polymorphic_object(context, enemy::static_type_id(),
                                                           value.value);
  CHECK(decoded.failed());
  CHECK(decoded.status.value() == caors::errc::invalid_type);
  CHECK(manager.empty());
}
