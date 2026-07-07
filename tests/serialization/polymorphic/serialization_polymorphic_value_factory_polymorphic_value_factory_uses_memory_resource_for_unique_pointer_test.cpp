// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

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
  context.memory.resource = &tracking;

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
