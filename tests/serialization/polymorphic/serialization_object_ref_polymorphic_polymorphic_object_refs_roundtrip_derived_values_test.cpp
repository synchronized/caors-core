// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

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

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory.resource = &tracking;

  polymorphic_object_ref_holder decoded{};
  const caors::error status = caors::deserialize_json(context, decoded, json.value);

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
