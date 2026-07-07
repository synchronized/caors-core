// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

namespace {

struct json_options_actor {
  int id = 0;
};

} // namespace

TEST_CASE("serialization - json options can pretty print json", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<json_options_actor>("caors.test.serialization.json_options_actor")
          .field("id", &json_options_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<json_options_actor>()));

  caors::serialization::json_options options{};
  options.pretty = true;
  options.indent = 2;

  const json_options_actor actor{7};
  const caors::serialization::serialization_text_result result =
      caors::serialize_json(actor, options);

  REQUIRE(result.succeeded());
  CHECK(result.value == "{\n  \"id\": 7\n}");
}
