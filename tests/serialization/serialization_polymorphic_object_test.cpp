// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

namespace {

class polymorphic_actor : public caors::object_base<polymorphic_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.poly.actor";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x65, 0xa4, 0x1f, 0x25, 0xf8, 0xe1, 0x40, 0xd7, 0x8a, 0x89, 0xf2,
                              0x37, 0x8d, 0xe5, 0x10, 0x01})};
};

class polymorphic_player : public caors::object_base<polymorphic_player, polymorphic_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.poly.player";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x65, 0xa4, 0x1f, 0x25, 0xf8, 0xe1, 0x40, 0xd7, 0x8a, 0x89, 0xf2,
                              0x37, 0x8d, 0xe5, 0x10, 0x02})};

  int level = 0;
  caors::string name{};
};

class polymorphic_enemy : public caors::object_base<polymorphic_enemy> {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.poly.enemy";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x65, 0xa4, 0x1f, 0x25, 0xf8, 0xe1, 0x40, 0xd7, 0x8a, 0x89, 0xf2,
                              0x37, 0x8d, 0xe5, 0x10, 0x03})};

  int danger = 0;
};

[[nodiscard]] bool object_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

void require_polymorphic_object_metadata() {
  REQUIRE(object_result_ok(
      caors::objects::class_<polymorphic_actor>(polymorphic_actor::object_type_name).commit()));
  REQUIRE(object_result_ok(
      caors::objects::class_<polymorphic_player>(polymorphic_player::object_type_name).commit()));
  REQUIRE(object_result_ok(
      caors::objects::class_<polymorphic_enemy>(polymorphic_enemy::object_type_name).commit()));

  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_actor>(caors::meta::find<polymorphic_actor>())
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_player>(caors::meta::find<polymorphic_player>())
          .field("level", &polymorphic_player::level)
          .field("name", &polymorphic_player::name)
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_enemy>(caors::meta::find<polymorphic_enemy>())
          .field("danger", &polymorphic_enemy::danger)
          .commit()));

  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_actor>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_player>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_enemy>()));
}

} // namespace

TEST_CASE("serialization - polymorphic object writes type envelope", "[serialization]") {
  require_polymorphic_object_metadata();

  polymorphic_player player{};
  player.level = 7;
  player.name = "Ada";

  const caors::serialization::variant_result value =
      caors::serialization::to_polymorphic_serialization_value(player);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* type_member =
      value.value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{polymorphic_player::object_type_guid.value.string()});

  const caors::value::variant_member* stored_value =
      value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);
}

TEST_CASE("serialization - ordinary object API writes runtime type envelopes", "[serialization]") {
  require_polymorphic_object_metadata();

  polymorphic_player player{};
  player.level = 8;
  player.name = "Ira";
  const polymorphic_actor& actor = player;

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* type_member =
      value.value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{polymorphic_player::object_type_guid.value.string()});

  const caors::value::variant_member* stored_value =
      value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("$type") != caors::string::npos);
  CHECK(json.value.find("$value") != caors::string::npos);
  CHECK(json.value.find("level") != caors::string::npos);
}

TEST_CASE("serialization - polymorphic object envelope names are configurable", "[serialization]") {
  require_polymorphic_object_metadata();

  caors::serialization::context context{};
  context.polymorphic.type_field = caors::string_name{"kind"};
  context.polymorphic.value_field = caors::string_name{"data"};
  context.polymorphic.type_name_field = caors::string_name{"debug_type"};
  context.polymorphic.write_type_name = true;

  polymorphic_player player{};
  player.level = 13;
  player.name = "Kai";
  const polymorphic_actor& actor = player;

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  CHECK(value.value.find_member(caors::string_name{"$type"}) == nullptr);
  CHECK(value.value.find_member(caors::string_name{"$value"}) == nullptr);

  const caors::value::variant_member* type_member =
      value.value.find_member(caors::string_name{"kind"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{polymorphic_player::object_type_guid.value.string()});

  const caors::value::variant_member* type_name_member =
      value.value.find_member(caors::string_name{"debug_type"});
  REQUIRE(type_name_member != nullptr);
  REQUIRE(type_name_member->value.is_string());
  CHECK(std::string_view{type_name_member->value.as_string().data(),
                         type_name_member->value.as_string().size()} ==
        polymorphic_player::object_type_name);

  const caors::value::variant_member* stored_value =
      value.value.find_member(caors::string_name{"data"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("kind") != caors::string::npos);
  CHECK(json.value.find("data") != caors::string::npos);
  CHECK(json.value.find("debug_type") != caors::string::npos);

  polymorphic_player target{};
  polymorphic_actor& target_actor = target;
  const caors::error status = caors::deserialize_json(target_actor, json.value, context);
  REQUIRE(status.succeeded());
  CHECK(target.level == 13);
  CHECK(target.name == "Kai");
}

TEST_CASE("serialization - ordinary object API reads runtime type envelopes", "[serialization]") {
  require_polymorphic_object_metadata();

  polymorphic_player source{};
  source.level = 11;
  source.name = "Ren";
  const polymorphic_actor& source_actor = source;

  const caors::serialization::serialization_text_result json = caors::serialize_json(source_actor);
  REQUIRE(json.succeeded());

  polymorphic_player target{};
  polymorphic_actor& target_actor = target;
  const caors::error status = caors::deserialize_json(target_actor, json.value);
  REQUIRE(status.succeeded());
  CHECK(target.level == 11);
  CHECK(target.name == "Ren");

  polymorphic_actor wrong_target{};
  const caors::error wrong_status = caors::deserialize_json(wrong_target, json.value);
  CHECK(wrong_status.failed());
  CHECK(wrong_status.value() == caors::errc::invalid_type);
}

TEST_CASE("serialization - polymorphic object roundtrips through manager resource",
          "[serialization]") {
  require_polymorphic_object_metadata();

  caors::objects::manager manager;
  caors::serialization::context context{};
  context.object_manager = &manager;

  polymorphic_player player{};
  player.level = 9;
  player.name = "Mina";

  const caors::serialization::serialization_text_result json =
      caors::serialization::serialize_polymorphic_json(player, context);
  REQUIRE(json.succeeded());

  const caors::result<polymorphic_actor*> decoded =
      caors::serialization::deserialize_polymorphic_json<polymorphic_actor>(json.value, context);
  REQUIRE(decoded.succeeded());
  REQUIRE(decoded.value != nullptr);
  CHECK(manager.find(decoded.value->id()) == decoded.value);
  CHECK(decoded.value->type_id() == polymorphic_player::static_type_id());

  auto* decoded_player = dynamic_cast<polymorphic_player*>(decoded.value);
  REQUIRE(decoded_player != nullptr);
  CHECK(decoded_player->level == 9);
  CHECK(decoded_player->name == "Mina");

  manager.clear();
  CHECK(manager.empty());
}

TEST_CASE("serialization - polymorphic object rejects wrong declared base", "[serialization]") {
  require_polymorphic_object_metadata();

  polymorphic_player player{};
  player.level = 3;

  const caors::serialization::variant_result value =
      caors::serialization::to_polymorphic_serialization_value(player);
  REQUIRE(value.succeeded());

  caors::objects::manager manager;
  caors::serialization::context context{};
  context.object_manager = &manager;

  const caors::result<caors::objects::object*> decoded =
      caors::serialization::deserialize_polymorphic_object(polymorphic_enemy::static_type_id(),
                                                           value.value, context);
  CHECK(decoded.failed());
  CHECK(decoded.status.value() == caors::errc::invalid_type);
  CHECK(manager.empty());
}

TEST_CASE("serialization - polymorphic object requires manager and rolls back failed reads",
          "[serialization]") {
  require_polymorphic_object_metadata();

  polymorphic_player player{};
  player.level = 5;

  caors::serialization::variant_result value =
      caors::serialization::to_polymorphic_serialization_value(player);
  REQUIRE(value.succeeded());

  caors::serialization::context missing_manager{};
  const caors::result<polymorphic_actor*> missing =
      caors::serialization::deserialize_polymorphic_object<polymorphic_actor>(value.value,
                                                                              missing_manager);
  CHECK(missing.failed());
  CHECK(missing.status.value() == caors::errc::invalid_argument);

  caors::value::variant_member* stored_value =
      value.value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  caors::value::variant_member* level =
      stored_value->value.find_member(caors::string_name{"level"});
  REQUIRE(level != nullptr);
  level->value = caors::variant::from("not an integer");

  caors::objects::manager manager;
  caors::serialization::context context{};
  context.object_manager = &manager;

  const caors::result<polymorphic_actor*> failed =
      caors::serialization::deserialize_polymorphic_object<polymorphic_actor>(value.value, context);
  CHECK(failed.failed());
  CHECK(manager.empty());
}
