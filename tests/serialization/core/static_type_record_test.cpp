// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_static_type_record_context_memory_resource_constructs_empty_wrapper_values_test.cpp

#include "serialization_static_type_record_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::static_type_record;

TEST_CASE("serialization - context memory resource constructs empty wrapper values",
          "[serialization]") {
  require_context_resource_holder_metadata();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.resource = &tracking;

  std::size_t allocation_count = 0;
  {
    context_resource_holder decoded{};
    const caors::error status = caors::read_json_from(
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

// 原测试文件：serialization_static_type_record_enum_ops_handles_unregistered_enum_values_test.cpp

#include "serialization_static_type_record_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::static_type_record;

TEST_CASE("serialization - static enum data handles unregistered enum values",
          "[serialization]") {
  static_enum_color color = static_enum_color::blue;

  const caors::serialization::variant_result ref_value =
      caors::serialization::to_variant(caors::any_ref::from(color));
  REQUIRE(ref_value.succeeded());
  REQUIRE(ref_value.value.try_as_u64() != nullptr);
  CHECK(ref_value.value.as_u64() == 2U);

  const caors::serialization::variant_result owned_value =
      caors::serialization::to_variant(caors::any::from(color));
  REQUIRE(owned_value.succeeded());
  REQUIRE(owned_value.value.try_as_u64() != nullptr);
  CHECK(owned_value.value.as_u64() == 2U);

  require_serializable_property_type<static_enum_actor>(
      "caors.test.serialization.static_enum_actor", "colors", &static_enum_actor::colors);

  const static_enum_actor actor{{static_enum_color::blue, static_enum_color::red}};
  const caors::serialization::serialization_text_result text = caors::to_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"colors":[2,1]})");

  static_enum_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, R"({"colors":[1,2]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.colors.size() == 2);
  CHECK(decoded.colors[0] == static_enum_color::red);
  CHECK(decoded.colors[1] == static_enum_color::blue);
}
