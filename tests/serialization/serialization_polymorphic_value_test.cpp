// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/memory.h"

namespace {

class polymorphic_value_actor {
public:
  virtual ~polymorphic_value_actor() = default;
};

class polymorphic_value_player : public polymorphic_value_actor {
public:
  int level = 0;
  caors::string name{};
};

class polymorphic_value_enemy {
public:
  int danger = 0;
};

struct polymorphic_value_holder {
  std::unique_ptr<polymorphic_value_actor> owner{};
  std::shared_ptr<polymorphic_value_actor> shared{};
};

class polymorphic_factory_actor {
public:
  virtual ~polymorphic_factory_actor() = default;
};

class polymorphic_factory_player : public polymorphic_factory_actor {
public:
  explicit polymorphic_factory_player(int initial_level) : level(initial_level) {}

  int level = 0;
  caors::string name{};
};

struct polymorphic_factory_holder {
  std::shared_ptr<polymorphic_factory_actor> shared{};
};

constexpr caors::type_guid actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x20, 0x01})};

constexpr caors::type_guid player_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x20, 0x02})};

constexpr caors::type_guid enemy_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x20, 0x03})};

constexpr caors::type_guid factory_actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x21, 0x01})};

constexpr caors::type_guid factory_player_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x0f, 0xc2, 0x7d, 0x83, 0x34, 0x4a, 0x42, 0x58, 0x90, 0x9d, 0xf9, 0x0e,
                            0x5c, 0xb7, 0x21, 0x02})};

[[nodiscard]] bool type_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_id;
}

void* construct_factory_player(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<polymorphic_factory_player>(resource, 0);
  }

  return new polymorphic_factory_player(0);
}

void destroy_factory_player(caors::memory_resource* resource, void* value) noexcept {
  auto* player = static_cast<polymorphic_factory_player*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(player, resource);
    return;
  }

  delete player;
}

void require_polymorphic_value_metadata() {
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_value_actor>("caors.test.serialization.poly_value.actor")
          .guid(actor_guid)
          .result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_value_player>("caors.test.serialization.poly_value.player")
          .guid(player_guid)
          .parent<polymorphic_value_actor>()
          .result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_value_enemy>("caors.test.serialization.poly_value.enemy")
          .guid(enemy_guid)
          .result()));
  REQUIRE(type_result_ok(caors::meta::type_<std::unique_ptr<polymorphic_value_actor>>(
                             "std.unique_ptr<caors.test.serialization.poly_value.actor>")
                             .result()));
  REQUIRE(type_result_ok(caors::meta::type_<std::shared_ptr<polymorphic_value_actor>>(
                             "std.shared_ptr<caors.test.serialization.poly_value.actor>")
                             .result()));

  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_actor>(
          caors::meta::find<polymorphic_value_actor>())
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_player>(
          caors::meta::find<polymorphic_value_player>())
          .field("level", &polymorphic_value_player::level)
          .field("name", &polymorphic_value_player::name)
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_enemy>(
          caors::meta::find<polymorphic_value_enemy>())
          .field("danger", &polymorphic_value_enemy::danger)
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_value_holder>(
          "caors.test.serialization.poly_value.holder")
          .field("owner", &polymorphic_value_holder::owner,
                 "std.unique_ptr<caors.test.serialization.poly_value.actor>")
          .field("shared", &polymorphic_value_holder::shared,
                 "std.shared_ptr<caors.test.serialization.poly_value.actor>")
          .commit()));

  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_actor>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_player>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_enemy>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_value_holder>()));
}

void require_polymorphic_factory_metadata() {
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_factory_actor>("caors.test.serialization.poly_factory.actor")
          .guid(factory_actor_guid)
          .result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<polymorphic_factory_player>("caors.test.serialization.poly_factory.player")
          .guid(factory_player_guid)
          .parent<polymorphic_factory_actor>()
          .factory(&construct_factory_player, &destroy_factory_player)
          .result()));
  REQUIRE(type_result_ok(caors::meta::type_<std::shared_ptr<polymorphic_factory_actor>>(
                             "std.shared_ptr<caors.test.serialization.poly_factory.actor>")
                             .result()));

  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_factory_actor>(
          caors::meta::find<polymorphic_factory_actor>())
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_factory_player>(
          caors::meta::find<polymorphic_factory_player>())
          .field("level", &polymorphic_factory_player::level)
          .field("name", &polymorphic_factory_player::name)
          .commit()));
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_factory_holder>(
          "caors.test.serialization.poly_factory.holder")
          .field("shared", &polymorphic_factory_holder::shared,
                 "std.shared_ptr<caors.test.serialization.poly_factory.actor>")
          .commit()));

  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_factory_actor>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_factory_player>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_factory_holder>()));
}

} // namespace

TEST_CASE("serialization - polymorphic value wrappers write type envelopes", "[serialization]") {
  require_polymorphic_value_metadata();

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 17;
  owner->name = "Ada";
  holder.owner = std::move(owner);

  auto shared = std::make_shared<polymorphic_value_player>();
  shared->level = 23;
  shared->name = "Mina";
  holder.shared = std::move(shared);

  const caors::serialization::variant_result value = caors::to_serialization_value(holder);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* owner_member =
      value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner_member != nullptr);
  REQUIRE(owner_member->value.is_object());

  const caors::value::variant_member* type_member =
      owner_member->value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player_guid.value.string()});

  const caors::value::variant_member* stored_value =
      owner_member->value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);
}

TEST_CASE("serialization - polymorphic value wrappers roundtrip derived values",
          "[serialization]") {
  require_polymorphic_value_metadata();

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 31;
  owner->name = "Lin";
  holder.owner = std::move(owner);

  auto shared = std::make_shared<polymorphic_value_player>();
  shared->level = 37;
  shared->name = "Nia";
  holder.shared = std::move(shared);

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  polymorphic_value_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_value_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 31);
  CHECK(decoded_owner->name == "Lin");

  auto* decoded_shared = dynamic_cast<polymorphic_value_player*>(decoded.shared.get());
  REQUIRE(decoded_shared != nullptr);
  CHECK(decoded_shared->level == 37);
  CHECK(decoded_shared->name == "Nia");
}

TEST_CASE("serialization - polymorphic value wrappers reject unrelated dynamic types",
          "[serialization]") {
  require_polymorphic_value_metadata();

  caors::variant::object_type enemy_fields;
  enemy_fields.push_back(caors::value::variant_member{
      caors::string_name{"danger"},
      0,
      caors::variant::from(std::int64_t{9}),
  });

  caors::variant::object_type owner_envelope;
  owner_envelope.push_back(caors::value::variant_member{
      caors::string_name{"$type"},
      0,
      caors::variant::from(enemy_guid.value.string()),
  });
  owner_envelope.push_back(caors::value::variant_member{
      caors::string_name{"$value"},
      0,
      caors::variant::object(std::move(enemy_fields)),
  });

  caors::variant::object_type holder_fields;
  holder_fields.push_back(caors::value::variant_member{
      caors::string_name{"owner"},
      0,
      caors::variant::object(std::move(owner_envelope)),
  });

  polymorphic_value_holder decoded{};
  const caors::error status =
      caors::deserialize(decoded, caors::variant::object(std::move(holder_fields)));

  CHECK(status.failed());
  CHECK(status.value() == caors::errc::invalid_type);
  CHECK(decoded.owner == nullptr);
}

TEST_CASE("serialization - polymorphic value factory uses memory resource", "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_holder holder{};
  auto shared = std::make_shared<polymorphic_factory_player>(1);
  shared->level = 41;
  shared->name = "Rin";
  holder.shared = std::move(shared);

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory_resource = &tracking;

  polymorphic_factory_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(status.succeeded());

  auto* decoded_shared = dynamic_cast<polymorphic_factory_player*>(decoded.shared.get());
  REQUIRE(decoded_shared != nullptr);
  CHECK(decoded_shared->level == 41);
  CHECK(decoded_shared->name == "Rin");
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.shared.reset();
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
