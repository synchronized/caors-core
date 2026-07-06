// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - converts runtime values to variant", "[serialization]") {
  require_registered_type<int>("int");
  require_registered_type<caors::string>("caors.string");
  require_registered_type<caors::guid>("caors.guid");

  caors::any number = caors::any::from(42);
  caors::serialization::variant_result number_result = caors::serialization::to_variant(number);
  REQUIRE(number_result.succeeded());
  CHECK(number_result.value.is_i64());
  REQUIRE(number_result.value.try_as_i64() != nullptr);
  CHECK(*number_result.value.try_as_i64() == 42);

  caors::string name = "Ada";
  caors::any_ref name_ref = caors::any_ref::from(name);
  caors::serialization::variant_result name_result = caors::serialization::to_variant(name_ref);
  REQUIRE(name_result.succeeded());
  CHECK(name_result.value.is_string());
  REQUIRE(name_result.value.try_as_string() != nullptr);
  CHECK(*name_result.value.try_as_string() == "Ada");

  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());
  caors::serialization::variant_result guid_result =
      caors::serialization::to_variant(caors::any::from(*id));
  REQUIRE(guid_result.succeeded());
  CHECK(guid_result.value.is_string());
  REQUIRE(guid_result.value.try_as_string() != nullptr);
  CHECK(*guid_result.value.try_as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  caors::serialization::variant_result invalid_result =
      caors::serialization::to_variant(caors::any_ref{});
  CHECK(invalid_result.failed());
  CHECK(invalid_result.status.value() == caors::errc::invalid_argument);
}
