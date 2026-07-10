// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_tuple_variant_rejects_std_pair_length_mismatch_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std pair length mismatch", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<pair_actor>("caors.test.serialization.pair_actor")
          .field("entry", &pair_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<pair_actor>().commit()));

  pair_actor actor{{7, caors::string{"seven"}}};
  const caors::error result = caors::deserialize_json(actor, R"({"entry":[9]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
  CHECK(actor.entry.first == 7);
  CHECK(actor.entry.second == "seven");
}

// 原测试文件：serialization_tuple_variant_rejects_std_tuple_length_mismatch_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std tuple length mismatch", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<tuple_actor>("caors.test.serialization.tuple_actor")
          .field("entry", &tuple_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<tuple_actor>().commit()));

  tuple_actor actor{{7, caors::string{"seven"}, 1.5F}};
  const caors::error result = caors::deserialize_json(actor, R"({"entry":[9,"nine"]})");

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
  CHECK(std::get<0>(actor.entry) == 7);
  CHECK(std::get<1>(actor.entry) == "seven");
  CHECK(std::get<2>(actor.entry) == 1.5F);
}

// 原测试文件：serialization_tuple_variant_rejects_std_variant_invalid_data_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - rejects std variant invalid data", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<variant_actor>("caors.test.serialization.variant_actor")
          .field("entry", &variant_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<variant_actor>().commit()));

  variant_actor actor{};
  actor.entry = caors::string{"seven"};
  const caors::error invalid_index = caors::deserialize_json(actor, R"({"entry":[9,"bad"]})");

  CHECK(invalid_index.failed());
  CHECK(invalid_index.domain() == caors::error_domain::serialization);
  CHECK(invalid_index.value() == caors::errc::invalid_argument);
  REQUIRE(std::holds_alternative<caors::string>(actor.entry));
  CHECK(std::get<caors::string>(actor.entry) == "seven");

  const caors::error invalid_length = caors::deserialize_json(actor, R"({"entry":[1]})");

  CHECK(invalid_length.failed());
  CHECK(invalid_length.domain() == caors::error_domain::serialization);
  CHECK(invalid_length.value() == caors::errc::invalid_argument);
  REQUIRE(std::holds_alternative<caors::string>(actor.entry));
  CHECK(std::get<caors::string>(actor.entry) == "seven");
}

// 原测试文件：serialization_tuple_variant_roundtrips_std_pair_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std pair fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<pair_actor>("caors.test.serialization.pair_actor")
          .field("entry", &pair_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<pair_actor>().commit()));

  pair_actor actor{{7, caors::string{"seven"}}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven"]})");

  pair_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine"]})");

  REQUIRE(result.succeeded());
  CHECK(decoded.entry.first == 9);
  CHECK(decoded.entry.second == "nine");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 2);
  CHECK(entry->value.as_array()[0].as_i64() == 7);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
}

// 原测试文件：serialization_tuple_variant_roundtrips_std_tuple_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std tuple fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<tuple_actor>("caors.test.serialization.tuple_actor")
          .field("entry", &tuple_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<tuple_actor>().commit()));

  tuple_actor actor{{7, caors::string{"seven"}, 1.5F}};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[7,"seven",1.5]})");

  tuple_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[9,"nine",2.5]})");

  REQUIRE(result.succeeded());
  CHECK(std::get<0>(decoded.entry) == 9);
  CHECK(std::get<1>(decoded.entry) == "nine");
  CHECK(std::get<2>(decoded.entry) == 2.5F);

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 3);
  CHECK(entry->value.as_array()[0].as_i64() == 7);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
  CHECK(entry->value.as_array()[2].as_f64() == 1.5);
}

// 原测试文件：serialization_tuple_variant_roundtrips_std_variant_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std variant fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<variant_actor>("caors.test.serialization.variant_actor")
          .field("entry", &variant_actor::entry)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<variant_actor>().commit()));

  variant_actor actor{};
  actor.entry = caors::string{"seven"};

  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"entry":[1,"seven"]})");

  variant_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"entry":[2,2.5]})");

  REQUIRE(result.succeeded());
  REQUIRE(std::holds_alternative<float>(decoded.entry));
  CHECK(std::get<float>(decoded.entry) == 2.5F);

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* entry = value.value.find_member(caors::string_name{"entry"});
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.is_array());
  REQUIRE(entry->value.as_array().size() == 2);
  CHECK(entry->value.as_array()[0].as_u64() == 1U);
  CHECK(entry->value.as_array()[1].as_string() == "seven");
}
