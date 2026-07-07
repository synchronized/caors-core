// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_descriptor_resource_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::descriptor_resource;

TEST_CASE("serialization - context memory resource constructs empty wrapper values",
          "[serialization]") {
  require_context_resource_holder_metadata();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory.resource = &tracking;

  std::size_t allocation_count = 0;
  {
    context_resource_holder decoded{};
    const caors::error status = caors::deserialize_json(
        context, decoded,
        R"({"unique":{"value":7},"shared":{"value":9},"names":["Ada","Grace"],"maybe":"present","choice":[1,"variant"]})");
    REQUIRE(status.succeeded());

    REQUIRE(decoded.unique != nullptr);
    REQUIRE(decoded.shared != nullptr);
    CHECK(decoded.unique->value == 7);
    CHECK(decoded.shared->value == 9);
    REQUIRE(decoded.names.size() == 2);
    CHECK(decoded.names[0] == "Ada");
    CHECK(decoded.names[0].get_allocator().resource() == &tracking);
    CHECK(decoded.names[1] == "Grace");
    CHECK(decoded.names[1].get_allocator().resource() == &tracking);
    REQUIRE(decoded.maybe.has_value());
    CHECK(*decoded.maybe == "present");
    CHECK(decoded.maybe->get_allocator().resource() == &tracking);
    REQUIRE(std::holds_alternative<caors::string>(decoded.choice));
    CHECK(std::get<caors::string>(decoded.choice) == "variant");
    CHECK(std::get<caors::string>(decoded.choice).get_allocator().resource() == &tracking);

    allocation_count = tracking.statistics().allocation_count;
    CHECK(allocation_count >= 2);
    CHECK(tracking.statistics().bytes_in_use >= sizeof(context_resource_value) * 2);
  }

  CHECK(tracking.statistics().deallocation_count == allocation_count);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
