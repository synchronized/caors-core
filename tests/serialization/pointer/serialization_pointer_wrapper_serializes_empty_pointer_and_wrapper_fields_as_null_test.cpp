// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes empty pointer and wrapper fields as null",
          "[serialization]") {
  require_pointer_actor_metadata();

  pointer_actor actor{};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"raw":null,"shared":null,"unique":null,"optional":null,"stats":null})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());

  const caors::value::variant_member* raw = value.value.find_member(caors::string_name{"raw"});
  const caors::value::variant_member* shared =
      value.value.find_member(caors::string_name{"shared"});
  const caors::value::variant_member* unique =
      value.value.find_member(caors::string_name{"unique"});
  const caors::value::variant_member* optional =
      value.value.find_member(caors::string_name{"optional"});
  const caors::value::variant_member* stats = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(raw != nullptr);
  REQUIRE(shared != nullptr);
  REQUIRE(unique != nullptr);
  REQUIRE(optional != nullptr);
  REQUIRE(stats != nullptr);
  CHECK(raw->value.is_null());
  CHECK(shared->value.is_null());
  CHECK(unique->value.is_null());
  CHECK(optional->value.is_null());
  CHECK(stats->value.is_null());
}
