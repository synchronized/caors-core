// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader matches fields by stable field id", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_id_actor>(
                                   "caors.test.serialization.reader_field_id_actor")
                                   .field("id", &reader_field_id_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_field_id_actor>({
      caors::serialization::make_field("id", "actor_id", false, 7),
  })));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"legacy_id"},
      7,
      caors::variant::from(std::int64_t{42}),
  });

  reader_field_id_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 42);
}
