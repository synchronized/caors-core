// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/object.h"

using namespace caors::tests::serialization;

namespace {

class object_ref_target : public caors::object_base<object_ref_target>,
                          public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name = "caors.test.serialization.object_ref_target";

  int value = 0;
};

struct object_ref_owner_actor {
  caors::object_ref<object_ref_target> owner{};
};

struct object_weak_ref_actor {
  caors::object_weak_ref<object_ref_target> observer{};
};

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

void require_object_ref_target_metadata() {
  const caors::error object_status = caors::objects::class_<object_ref_target>().commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));

  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_target>(object_ref_target::object_type_name)
          .field("value", &object_ref_target::value)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ref_target>()));
}

void require_object_ref_owner_actor_metadata() {
  require_object_ref_target_metadata();

  REQUIRE(reflection_result_ok(caors::reflection::class_<object_ref_owner_actor>(
                                   "caors.test.serialization.object_ref_owner_actor")
                                   .field("owner", &object_ref_owner_actor::owner)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ref_owner_actor>()));
}

void require_object_weak_ref_actor_metadata() {
  require_object_ref_target_metadata();

  REQUIRE(reflection_result_ok(caors::reflection::class_<object_weak_ref_actor>(
                                   "caors.test.serialization.object_weak_ref_actor")
                                   .field("observer", &object_weak_ref_actor::observer)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_weak_ref_actor>()));
}

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

TEST_CASE("serialization - serializes object refs as inline values", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  object_ref_owner_actor actor{};
  actor.owner = caors::objects::make_ref<object_ref_target>();
  actor.owner->value = 42;

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"owner":{"value":42}})");
}

TEST_CASE("serialization - serializes empty object refs as null", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  const object_ref_owner_actor actor{};

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"owner":null})");
}

TEST_CASE("serialization - deserializes object refs with context resource", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory_resource = &tracking;

  object_ref_owner_actor actor{};
  const caors::error result = caors::deserialize_json(actor, R"({"owner":{"value":17}})", context);

  REQUIRE(result.succeeded());
  REQUIRE(actor.owner);
  CHECK(actor.owner->value == 17);
  CHECK(actor.owner.use_count() == 1);
  CHECK(tracking.statistics().bytes_in_use > 0);

  actor.owner.reset();
  const caors::memory_statistics statistics = tracking.statistics();
  CHECK(statistics.bytes_in_use == 0);
  CHECK(statistics.allocation_count == statistics.deallocation_count);
}

TEST_CASE("serialization - deserializes null object refs", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  object_ref_owner_actor actor{};
  actor.owner = caors::objects::make_ref<object_ref_target>();
  actor.owner->value = 9;

  const caors::error result = caors::deserialize_json(actor, R"({"owner":null})");

  REQUIRE(result.succeeded());
  CHECK(actor.owner == nullptr);
}

TEST_CASE("serialization - rejects object weak refs", "[serialization]") {
  require_object_weak_ref_actor_metadata();

  const caors::objects::ref<object_ref_target> owner =
      caors::objects::make_ref<object_ref_target>();
  object_weak_ref_actor actor{};
  actor.observer = owner;

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  CHECK(json.failed());
  CHECK(json.status.domain() == caors::error_domain::serialization);
  CHECK(json.status.value() == caors::errc::unsupported);
}

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
