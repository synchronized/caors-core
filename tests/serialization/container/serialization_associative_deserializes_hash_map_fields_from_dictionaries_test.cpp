// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes hash map fields from dictionaries", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<map_actor>("caors.test.serialization.map_actor")
          .field("labels", &map_actor::labels)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<map_actor>()));

  caors::variant::dictionary_type entries;
  entries.push_back(caors::value::variant_entry{
      caors::variant::from(std::int64_t{1}),
      caors::variant::from(std::string_view{"one"}),
  });
  entries.push_back(caors::value::variant_entry{
      caors::variant::from(std::int64_t{2}),
      caors::variant::from(std::string_view{"two"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"labels"},
      0,
      caors::variant::dictionary(std::move(entries)),
  });

  map_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.labels.size() == 2);
  CHECK(actor.labels.at(1) == "one");
  CHECK(actor.labels.at(2) == "two");
}
