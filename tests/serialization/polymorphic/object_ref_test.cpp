// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_object_ref_polymorphic_polymorphic_object_refs_reject_unrelated_dynamic_types_test.cpp

#include "serialization_object_ref_polymorphic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_polymorphic;

TEST_CASE("serialization - polymorphic object refs reject unrelated dynamic types",
          "[serialization]") {
  require_polymorphic_object_ref_metadata();

  caors::variant::object_type enemy_fields;
  enemy_fields.push_back(caors::meta::variant_member{
      caors::string_name{"danger"},
      0,
      caors::variant::from(std::int64_t{9}),
  });

  caors::variant::object_type envelope;
  envelope.push_back(caors::meta::variant_member{
      caors::string_name{"$type"},
      0,
      caors::variant::from(polymorphic_object_ref_enemy_guid.value.string()),
  });
  envelope.push_back(caors::meta::variant_member{
      caors::string_name{"$value"},
      0,
      caors::variant::object(std::move(enemy_fields)),
  });

  caors::variant::object_type holder_fields;
  holder_fields.push_back(caors::meta::variant_member{
      caors::string_name{"owner"},
      0,
      caors::variant::object(std::move(envelope)),
  });

  polymorphic_object_ref_holder decoded{};
  const caors::error status =
      caors::read_value_from(decoded, caors::variant::object(std::move(holder_fields)));

  CHECK(status.failed());
  CHECK(status.value() == caors::errc::invalid_type);
  CHECK(decoded.owner == nullptr);
}

// 原测试文件：serialization_object_ref_polymorphic_polymorphic_object_refs_roundtrip_derived_values_test.cpp

#include "serialization_object_ref_polymorphic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_polymorphic;

TEST_CASE("serialization - polymorphic object refs roundtrip derived values", "[serialization]") {
  require_polymorphic_object_ref_metadata();
  polymorphic_object_ref_player::destroyed_count = 0;

  polymorphic_object_ref_holder holder{};
  caors::objects::ref<polymorphic_object_ref_player> player =
      caors::objects::make_ref<polymorphic_object_ref_player>();
  player->level = 27;
  player->name = "Mina";
  holder.owner = player;

  const caors::serialization::serialization_text_result json = caors::to_json(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.result_resource = &tracking;

  polymorphic_object_ref_holder decoded{};
  const caors::error status = caors::read_json_from(context, decoded, json.value);

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

// 原测试文件：serialization_object_ref_polymorphic_polymorphic_object_refs_write_type_envelopes_test.cpp

#include "serialization_object_ref_polymorphic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_polymorphic;

TEST_CASE("serialization - polymorphic object refs write type envelopes", "[serialization]") {
  require_polymorphic_object_ref_metadata();

  caors::objects::ref<polymorphic_object_ref_player> player =
      caors::objects::make_ref<polymorphic_object_ref_player>();
  player->level = 12;
  player->name = "Ada";

  polymorphic_object_ref_holder holder{};
  holder.owner = player;

  const caors::serialization::variant_result value = caors::to_value(holder);

  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::meta::variant_member* owner = value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner != nullptr);
  REQUIRE(owner->value.is_object());

  const caors::meta::variant_member* type = owner->value.find_member(caors::string_name{"$type"});
  REQUIRE(type != nullptr);
  REQUIRE(type->value.is_string());
  CHECK(std::string_view{type->value.as_string().data(), type->value.as_string().size()} ==
        std::string_view{polymorphic_object_ref_player_guid.value.string()});

  const caors::meta::variant_member* stored =
      owner->value.find_member(caors::string_name{"$value"});
  REQUIRE(stored != nullptr);
  REQUIRE(stored->value.is_object());
  CHECK(stored->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored->value.find_member(caors::string_name{"name"}) != nullptr);
}
