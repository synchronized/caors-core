// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_non_default_value_rejects_empty_non_default_constructible_unique_pointers_test.cpp

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects empty non default constructible unique pointers",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_unique_ptr_actor actor{};

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_type);
  CHECK(actor.value == nullptr);
}

// 原测试文件：serialization_non_default_value_rejects_non_default_constructible_sequence_elements_test.cpp

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects non default constructible sequence elements",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_vector_actor actor{};
  actor.values.emplace_back(1);

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"values":[{"value":1}]})");

  const caors::error result = caors::deserialize_json(actor, R"({"values":[{"value":7}]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_type);
  REQUIRE(actor.values.size() == 1);
  CHECK(actor.values[0].value == 1);
}

// 原测试文件：serialization_non_default_value_rejects_switching_to_non_default_constructible_variant_alternatives_test.cpp

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects switching to non default constructible variant alternatives",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_variant_actor actor{};
  actor.value = 3;

  const caors::error result = caors::deserialize_json(actor, R"({"value":[1,{"value":7}]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_type);
  REQUIRE(std::holds_alternative<int>(actor.value));
  CHECK(std::get<int>(actor.value) == 3);
}

// 原测试文件：serialization_non_default_value_updates_existing_non_default_constructible_unique_pointers_test.cpp

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - updates existing non default constructible unique pointers",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_unique_ptr_actor actor{};
  actor.value = std::make_unique<non_default_value>(1);

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  REQUIRE(result.succeeded());
  REQUIRE(actor.value != nullptr);
  CHECK(actor.value->value == 7);
}

// 原测试文件：serialization_non_default_value_updates_existing_non_default_constructible_variant_alternatives_test.cpp

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - updates existing non default constructible variant alternatives",
          "[serialization]") {
  require_non_default_value_metadata();

  non_default_variant_actor actor{};
  actor.value.emplace<1>(1);

  const caors::error result = caors::deserialize_json(actor, R"({"value":[1,{"value":7}]})");

  REQUIRE(result.succeeded());
  REQUIRE(std::holds_alternative<non_default_value>(actor.value));
  CHECK(std::get<non_default_value>(actor.value).value == 7);
}

// 原测试文件：serialization_non_default_value_updates_non_default_constructible_direct_fields_test.cpp

#include "serialization_test_support.h"

#include <memory>
#include <variant>

using namespace caors::tests::serialization;

TEST_CASE("serialization - updates non default constructible direct fields", "[serialization]") {
  require_non_default_value_metadata();

  non_default_field_actor actor{non_default_value{1}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":{"value":1}})");

  const caors::error result = caors::deserialize_json(actor, R"({"value":{"value":7}})");

  REQUIRE(result.succeeded());
  CHECK(actor.value.value == 7);
}
