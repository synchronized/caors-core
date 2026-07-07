// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant reader ignores unknown object members", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_unknown_field_actor>(
                                   "caors.test.serialization.reader_unknown_field_actor")
                                   .field("id", &reader_unknown_field_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<reader_unknown_field_actor>({
      caors::serialization::make_field("id"),
  })));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::int64_t{7}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"extra"},
      0,
      caors::variant::from(std::int64_t{99}),
  });

  reader_unknown_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
}
