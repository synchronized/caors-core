// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/object.h"

#include <string_view>

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
