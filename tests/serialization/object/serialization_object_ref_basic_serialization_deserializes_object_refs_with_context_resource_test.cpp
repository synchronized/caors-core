// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ref_basic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_basic;
TEST_CASE("serialization - deserializes object refs with context resource", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory.resource = &tracking;

  object_ref_owner_actor actor{};
  const caors::error result = caors::deserialize_json(context, actor, R"({"owner":{"value":17}})");

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
