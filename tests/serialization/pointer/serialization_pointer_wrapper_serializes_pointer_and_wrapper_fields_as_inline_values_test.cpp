// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes pointer and wrapper fields as inline values",
          "[serialization]") {
  require_pointer_actor_metadata();

  int raw = 7;
  nested_stats stats{2, 3.5F};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.shared = std::make_shared<int>(8);
  actor.unique = std::make_unique<int>(9);
  actor.optional = 10;
  actor.stats = &stats;

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value ==
        R"({"raw":7,"shared":8,"unique":9,"optional":10,"stats":{"level":2,"score":3.5}})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* shared =
      value.value.find_member(caors::string_name{"shared"});
  REQUIRE(shared != nullptr);
  CHECK(shared->value.as_i64() == 8);

  const caors::value::variant_member* nested = value.value.find_member(caors::string_name{"stats"});
  REQUIRE(nested != nullptr);
  REQUIRE(nested->value.is_object());
  const caors::value::variant_member* level =
      nested->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  CHECK(level->value.as_i64() == 2);
}
