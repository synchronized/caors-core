// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_polymorphic_object_error_polymorphic_object_rejects_wrong_declared_base_test.cpp

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object rejects wrong declared base", "[serialization]") {
  require_metadata();

  player player_value{};
  player_value.level = 3;
  const actor& actor_value = player_value;

  const caors::serialization::variant_result value = caors::to_value(actor_value);
  REQUIRE(value.succeeded());

  caors::serialization::context context{};

  enemy target{};
  caors::error status = caors::read_value_from(context, target, value.value);
  CHECK(status.failed());
  CHECK(status.value() == caors::errc::invalid_type);
}

// 原测试文件：serialization_polymorphic_object_error_polymorphic_object_rolls_back_failed_reads_test.cpp

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object rolls back failed reads", "[serialization]") {
  require_metadata();

  player player_value{};
  player_value.level = 5;
  const actor& actor_value = player_value;

  caors::serialization::variant_result value = caors::to_value(actor_value);
  REQUIRE(value.succeeded());

  caors::meta::variant_member* stored_value = value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  caors::meta::variant_member* level = stored_value->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  level->value = caors::variant::from("not an integer");

  caors::serialization::context context{};
  player target{};
  actor& target_actor = target;
  const caors::error failed = caors::read_value_from(context, target_actor, value.value);
  CHECK(failed.failed());
}

// 原测试文件：serialization_polymorphic_object_read_ordinary_object_api_reads_runtime_type_envelopes_test.cpp

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - ordinary object API reads runtime type envelopes", "[serialization]") {
  require_metadata();

  player source{};
  source.level = 11;
  source.name = "Ren";
  const actor& source_actor = source;

  const caors::serialization::serialization_text_result json = caors::to_json(source_actor);
  REQUIRE(json.succeeded());

  player target{};
  actor& target_actor = target;
  const caors::error status = caors::read_json_from(target_actor, json.value);
  REQUIRE(status.succeeded());
  CHECK(target.level == 11);
  CHECK(target.name == "Ren");

  actor wrong_target{};
  const caors::error wrong_status = caors::read_json_from(wrong_target, json.value);
  CHECK(wrong_status.failed());
  CHECK(wrong_status.value() == caors::errc::invalid_type);
}

// 原测试文件：serialization_polymorphic_object_read_polymorphic_object_roundtrips_through_ordinary_api_test.cpp

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object roundtrips through ordinary API", "[serialization]") {
  require_metadata();

  caors::serialization::context context{};

  player player_value{};
  player_value.level = 9;
  player_value.name = "Mina";
  const actor& actor_value = player_value;

  const caors::serialization::serialization_text_result json = caors::to_json(context, actor_value);
  REQUIRE(json.succeeded());

  player decoded_player{};
  actor& decoded_actor = decoded_player;
  const caors::error status = caors::read_json_from(context, decoded_actor, json.value);
  REQUIRE(status.succeeded());
  CHECK(decoded_player.type_id() == player::static_type_id());
  CHECK(decoded_player.level == 9);
  CHECK(decoded_player.name == "Mina");
}

// 原测试文件：serialization_polymorphic_object_write_ordinary_object_api_writes_runtime_type_envelopes_test.cpp

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - ordinary object API writes runtime type envelopes", "[serialization]") {
  require_metadata();

  player player_value{};
  player_value.level = 8;
  player_value.name = "Ira";
  const actor& actor_value = player_value;

  const caors::serialization::variant_result value = caors::to_value(actor_value);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::meta::variant_member* type_member =
      value.value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player::static_type_guid.value.string()});

  const caors::meta::variant_member* stored_value =
      value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);

  const caors::serialization::serialization_text_result json = caors::to_json(actor_value);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("$type") != caors::string::npos);
  CHECK(json.value.find("$value") != caors::string::npos);
  CHECK(json.value.find("level") != caors::string::npos);
}

// 原测试文件：serialization_polymorphic_object_write_polymorphic_object_envelope_names_are_configurable_test.cpp

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object envelope names are configurable", "[serialization]") {
  require_metadata();

  caors::serialization::context context{};
  context.config.polymorphic.type_field = caors::string_name{"kind"};
  context.config.polymorphic.value_field = caors::string_name{"data"};
  context.config.polymorphic.type_name_field = caors::string_name{"debug_type"};
  context.config.polymorphic.write_type_name = true;

  player player_value{};
  player_value.level = 13;
  player_value.name = "Kai";
  const actor& actor_value = player_value;

  const caors::serialization::variant_result value = caors::to_value(context, actor_value);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  CHECK(value.value.find_member(caors::string_name{"$type"}) == nullptr);
  CHECK(value.value.find_member(caors::string_name{"$value"}) == nullptr);

  const caors::meta::variant_member* type_member =
      value.value.find_member(caors::string_name{"kind"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player::static_type_guid.value.string()});

  const caors::meta::variant_member* type_name_member =
      value.value.find_member(caors::string_name{"debug_type"});
  REQUIRE(type_name_member != nullptr);
  REQUIRE(type_name_member->value.is_string());
  CHECK(std::string_view{type_name_member->value.as_string().data(),
                         type_name_member->value.as_string().size()} == player::object_type_name);

  const caors::meta::variant_member* stored_value =
      value.value.find_member(caors::string_name{"data"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);

  const caors::serialization::serialization_text_result json = caors::to_json(context, actor_value);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("kind") != caors::string::npos);
  CHECK(json.value.find("data") != caors::string::npos);
  CHECK(json.value.find("debug_type") != caors::string::npos);

  player target{};
  actor& target_actor = target;
  const caors::error status = caors::read_json_from(context, target_actor, json.value);
  REQUIRE(status.succeeded());
  CHECK(target.level == 13);
  CHECK(target.name == "Kai");
}

// 原测试文件：serialization_polymorphic_object_write_polymorphic_object_writes_type_envelope_test.cpp

#include "serialization_polymorphic_object_test_support.h"

using namespace caors::tests::serialization::polymorphic_object;

TEST_CASE("serialization - polymorphic object writes type envelope", "[serialization]") {
  require_metadata();

  player player_value{};
  player_value.level = 7;
  player_value.name = "Ada";
  const actor& actor_value = player_value;

  const caors::serialization::variant_result value = caors::to_value(actor_value);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::meta::variant_member* type_member =
      value.value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player::static_type_guid.value.string()});

  const caors::meta::variant_member* stored_value =
      value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);
}
