// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/object.h"

#include <cstdint>
#include <string_view>

using namespace caors::tests::serialization;

namespace {

class polymorphic_object_ref_actor : public caors::object_base<polymorphic_object_ref_actor>,
                                     public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.polymorphic_object_ref.actor";
};

class polymorphic_object_ref_player
    : public caors::object_base<polymorphic_object_ref_player, polymorphic_object_ref_actor> {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.polymorphic_object_ref.player";

  ~polymorphic_object_ref_player() override { ++destroyed_count; }

  int level = 0;
  caors::string name{};

  inline static int destroyed_count = 0;
};

class polymorphic_object_ref_enemy : public caors::object_base<polymorphic_object_ref_enemy>,
                                     public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.polymorphic_object_ref.enemy";

  int danger = 0;
};

struct polymorphic_object_ref_holder {
  caors::object_ref<polymorphic_object_ref_actor> owner{};
};

constexpr caors::type_guid polymorphic_object_ref_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3e, 0x1a, 0xa8, 0x02, 0x3d, 0x91, 0x4c, 0xa6, 0x8f, 0x31, 0x2b, 0xc5,
                            0xd5, 0x84, 0x10, 0x01})};

constexpr caors::type_guid polymorphic_object_ref_player_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3e, 0x1a, 0xa8, 0x02, 0x3d, 0x91, 0x4c, 0xa6, 0x8f, 0x31, 0x2b, 0xc5,
                            0xd5, 0x84, 0x10, 0x02})};

constexpr caors::type_guid polymorphic_object_ref_enemy_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3e, 0x1a, 0xa8, 0x02, 0x3d, 0x91, 0x4c, 0xa6, 0x8f, 0x31, 0x2b, 0xc5,
                            0xd5, 0x84, 0x10, 0x03})};

void require_polymorphic_object_ref_metadata() {
  REQUIRE(type_result_ok(caors::meta::type_<polymorphic_object_ref_actor>(
                             polymorphic_object_ref_actor::object_type_name)
                             .guid(polymorphic_object_ref_actor_guid)
                             .result()));
  REQUIRE(type_result_ok(caors::meta::type_<polymorphic_object_ref_player>(
                             polymorphic_object_ref_player::object_type_name)
                             .guid(polymorphic_object_ref_player_guid)
                             .parent<polymorphic_object_ref_actor>()
                             .result()));
  REQUIRE(type_result_ok(caors::meta::type_<polymorphic_object_ref_enemy>(
                             polymorphic_object_ref_enemy::object_type_name)
                             .guid(polymorphic_object_ref_enemy_guid)
                             .result()));

  REQUIRE(reflection_result_ok(caors::reflection::class_<polymorphic_object_ref_actor>(
                                   caors::meta::find<polymorphic_object_ref_actor>())
                                   .commit()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<polymorphic_object_ref_player>(
                                   caors::meta::find<polymorphic_object_ref_player>())
                                   .field("level", &polymorphic_object_ref_player::level)
                                   .field("name", &polymorphic_object_ref_player::name)
                                   .commit()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<polymorphic_object_ref_enemy>(
                                   caors::meta::find<polymorphic_object_ref_enemy>())
                                   .field("danger", &polymorphic_object_ref_enemy::danger)
                                   .commit()));
  REQUIRE(reflection_result_ok(caors::reflection::class_<polymorphic_object_ref_holder>(
                                   "caors.test.serialization.polymorphic_object_ref.holder")
                                   .field("owner", &polymorphic_object_ref_holder::owner)
                                   .commit()));

  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<polymorphic_object_ref_actor>()));
  REQUIRE(serialization_result_ok(
      caors::serialization::register_type<polymorphic_object_ref_player>()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<polymorphic_object_ref_enemy>()));
  REQUIRE(serialization_result_ok(
      caors::serialization::register_type<polymorphic_object_ref_holder>()));
}

} // namespace

TEST_CASE("serialization - polymorphic object refs write type envelopes", "[serialization]") {
  require_polymorphic_object_ref_metadata();

  caors::objects::ref<polymorphic_object_ref_player> player =
      caors::objects::make_ref<polymorphic_object_ref_player>();
  player->level = 12;
  player->name = "Ada";

  polymorphic_object_ref_holder holder{};
  holder.owner = player;

  const caors::serialization::variant_result value = caors::to_serialization_value(holder);

  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* owner = value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner != nullptr);
  REQUIRE(owner->value.is_object());

  const caors::value::variant_member* type = owner->value.find_member(caors::string_name{"$type"});
  REQUIRE(type != nullptr);
  REQUIRE(type->value.is_string());
  CHECK(std::string_view{type->value.as_string().data(), type->value.as_string().size()} ==
        std::string_view{polymorphic_object_ref_player_guid.value.string()});

  const caors::value::variant_member* stored =
      owner->value.find_member(caors::string_name{"$value"});
  REQUIRE(stored != nullptr);
  REQUIRE(stored->value.is_object());
  CHECK(stored->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored->value.find_member(caors::string_name{"name"}) != nullptr);
}

TEST_CASE("serialization - polymorphic object refs roundtrip derived values", "[serialization]") {
  require_polymorphic_object_ref_metadata();
  polymorphic_object_ref_player::destroyed_count = 0;

  polymorphic_object_ref_holder holder{};
  caors::objects::ref<polymorphic_object_ref_player> player =
      caors::objects::make_ref<polymorphic_object_ref_player>();
  player->level = 27;
  player->name = "Mina";
  holder.owner = player;

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory_resource = &tracking;

  polymorphic_object_ref_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value, context);

  REQUIRE(status.succeeded());
  REQUIRE(decoded.owner);
  auto* decoded_player = dynamic_cast<polymorphic_object_ref_player*>(decoded.owner.get());
  REQUIRE(decoded_player != nullptr);
  CHECK(decoded_player->level == 27);
  CHECK(decoded_player->name == "Mina");
  CHECK(decoded.owner.use_count() == 1);
  CHECK(tracking.statistics().bytes_in_use > 0);

  decoded.owner.reset();
  CHECK(polymorphic_object_ref_player::destroyed_count == 1);

  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

TEST_CASE("serialization - polymorphic object refs reject unrelated dynamic types",
          "[serialization]") {
  require_polymorphic_object_ref_metadata();

  caors::variant::object_type enemy_fields;
  enemy_fields.push_back(caors::value::variant_member{
      caors::string_name{"danger"},
      0,
      caors::variant::from(std::int64_t{9}),
  });

  caors::variant::object_type envelope;
  envelope.push_back(caors::value::variant_member{
      caors::string_name{"$type"},
      0,
      caors::variant::from(polymorphic_object_ref_enemy_guid.value.string()),
  });
  envelope.push_back(caors::value::variant_member{
      caors::string_name{"$value"},
      0,
      caors::variant::object(std::move(enemy_fields)),
  });

  caors::variant::object_type holder_fields;
  holder_fields.push_back(caors::value::variant_member{
      caors::string_name{"owner"},
      0,
      caors::variant::object(std::move(envelope)),
  });

  polymorphic_object_ref_holder decoded{};
  const caors::error status =
      caors::deserialize(decoded, caors::variant::object(std::move(holder_fields)));

  CHECK(status.failed());
  CHECK(status.value() == caors::errc::invalid_type);
  CHECK(decoded.owner == nullptr);
}
