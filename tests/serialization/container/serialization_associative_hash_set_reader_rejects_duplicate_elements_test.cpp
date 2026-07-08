// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - hash set reader rejects duplicate elements", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<set_actor>("caors.test.serialization.set_actor")
          .field("ids", &set_actor::ids)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<set_actor>()));

  caors::variant::array_type ids;
  ids.push_back(caors::variant::from(std::int64_t{1}));
  ids.push_back(caors::variant::from(std::int64_t{1}));

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"ids"},
      0,
      caors::variant::array(std::move(ids)),
  });

  set_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::already_exists);
}
