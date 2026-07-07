// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object requires manager and rolls back failed reads",
          "[serialization]") {
  require_metadata();

  player player_value{};
  player_value.level = 5;

  caors::serialization::variant_result value =
      caors::serialization::to_polymorphic_serialization_value(player_value);
  REQUIRE(value.succeeded());

  caors::serialization::context missing_manager{};
  const caors::result<actor*> missing =
      caors::serialization::deserialize_polymorphic_object<actor>(value.value, missing_manager);
  CHECK(missing.failed());
  CHECK(missing.status.value() == caors::errc::invalid_argument);

  caors::value::variant_member* stored_value =
      value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  caors::value::variant_member* level =
      stored_value->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  level->value = caors::variant::from("not an integer");

  caors::objects::manager manager;
  caors::serialization::context context{};
  context.objects.manager = &manager;

  const caors::result<actor*> failed =
      caors::serialization::deserialize_polymorphic_object<actor>(value.value, context);
  CHECK(failed.failed());
  CHECK(manager.empty());
}
