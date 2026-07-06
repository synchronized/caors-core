// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/memory.h"

#include <memory>

namespace {

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

struct polymorphic_factory_unique_holder {
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{};
};

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
  REQUIRE(type_result_ok(caors::meta::type_<caors::memory::unique_ptr<polymorphic_factory_actor>>(
                             "caors.memory.unique_ptr<caors.test.serialization.poly_factory.actor>")
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
  REQUIRE(caors::tests::serialization::reflection_result_ok(
      caors::reflection::class_<polymorphic_factory_unique_holder>(
          "caors.test.serialization.poly_factory.unique_holder")
          .field("owner", &polymorphic_factory_unique_holder::owner,
                 "caors.memory.unique_ptr<caors.test.serialization.poly_factory.actor>")
          .commit()));

  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_factory_actor>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_factory_player>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_factory_holder>()));
  REQUIRE(caors::tests::serialization::serialization_result_ok(
      caors::serialization::register_type<polymorphic_factory_unique_holder>()));
}

} // namespace

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
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.shared.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("serialization - polymorphic value factory roundtrips unique pointer",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 42;
  player->name = "Una";
  holder.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 42);
  CHECK(decoded_owner->name == "Una");
}

TEST_CASE("serialization - polymorphic value factory uses memory resource for unique pointer",
          "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_unique_holder holder{};
  caors::memory::unique_ptr<polymorphic_factory_actor> owner{new polymorphic_factory_player(1)};
  auto* player = static_cast<polymorphic_factory_player*>(owner.get());
  player->level = 43;
  player->name = "Uma";
  holder.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory_resource = &tracking;

  polymorphic_factory_unique_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_factory_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 43);
  CHECK(decoded_owner->name == "Uma");
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.owner.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
