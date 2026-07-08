// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_object_ref_basic_serialization_deserializes_null_object_refs_test.cpp

#include "serialization_object_ref_basic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_basic;
TEST_CASE("serialization - deserializes null object refs", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  object_ref_owner_actor actor{};
  actor.owner = caors::objects::make_ref<object_ref_target>();
  actor.owner->value = 9;

  const caors::error result = caors::deserialize_json(actor, R"({"owner":null})");

  REQUIRE(result.succeeded());
  CHECK(actor.owner == nullptr);
}

// 原测试文件：serialization_object_ref_basic_serialization_deserializes_object_refs_with_context_resource_test.cpp

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

// 原测试文件：serialization_object_ref_basic_serialization_serializes_empty_object_refs_as_null_test.cpp

#include "serialization_object_ref_basic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_basic;
TEST_CASE("serialization - serializes empty object refs as null", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  const object_ref_owner_actor actor{};

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"owner":null})");
}

// 原测试文件：serialization_object_ref_basic_serialization_serializes_object_refs_as_inline_values_test.cpp

#include "serialization_object_ref_basic_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ref_basic;
TEST_CASE("serialization - serializes object refs as inline values", "[serialization]") {
  require_object_ref_owner_actor_metadata();

  object_ref_owner_actor actor{};
  actor.owner = caors::objects::make_ref<object_ref_target>();
  actor.owner->value = 42;

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"owner":{"value":42}})");
}
