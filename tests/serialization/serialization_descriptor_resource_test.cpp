// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/memory.h"

#include <vector>

using namespace caors::tests::serialization;

namespace {

enum class descriptor_enum_color : unsigned char {
  red = 1,
  blue = 2,
};

struct descriptor_enum_actor {
  std::vector<descriptor_enum_color> colors{};
};

struct context_resource_value {
  int value = 0;
};

struct context_resource_holder {
  caors::memory::unique_ptr<context_resource_value> unique{};
  std::shared_ptr<context_resource_value> shared{};
  std::vector<caors::string> names{};
  std::optional<caors::string> maybe{};
  std::variant<int, caors::string> choice{0};
};

void require_context_resource_holder_metadata() {
  REQUIRE(type_result_ok(
      caors::meta::type_<context_resource_value>("caors.test.serialization.context_resource_value")
          .result()));
  REQUIRE(
      type_result_ok(caors::meta::type_<caors::memory::unique_ptr<context_resource_value>>(
                         "caors.memory.unique_ptr<caors.test.serialization.context_resource_value>")
                         .result()));
  REQUIRE(type_result_ok(caors::meta::type_<std::shared_ptr<context_resource_value>>(
                             "std.shared_ptr<caors.test.serialization.context_resource_value>")
                             .result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<std::vector<caors::string>>("std.vector<caors.string>").result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<std::optional<caors::string>>("std.optional<caors.string>").result()));
  REQUIRE(type_result_ok(
      caors::meta::type_<std::variant<int, caors::string>>("std.variant<int,caors.string>")
          .result()));

  REQUIRE(reflection_result_ok(caors::reflection::class_<context_resource_value>(
                                   "caors.test.serialization.context_resource_value")
                                   .field("value", &context_resource_value::value)
                                   .commit()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<context_resource_holder>(
          "caors.test.serialization.context_resource_holder")
          .field("unique", &context_resource_holder::unique,
                 "caors.memory.unique_ptr<caors.test.serialization.context_resource_value>")
          .field("shared", &context_resource_holder::shared,
                 "std.shared_ptr<caors.test.serialization.context_resource_value>")
          .field("names", &context_resource_holder::names, "std.vector<caors.string>")
          .field("maybe", &context_resource_holder::maybe, "std.optional<caors.string>")
          .field("choice", &context_resource_holder::choice, "std.variant<int,caors.string>")
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<context_resource_value>()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<context_resource_holder>()));
}

} // namespace

TEST_CASE("serialization - descriptor enum data handles unregistered enum values",
          "[serialization]") {
  descriptor_enum_color color = descriptor_enum_color::blue;

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

  REQUIRE(reflection_result_ok(caors::reflection::class_<descriptor_enum_actor>(
                                   "caors.test.serialization.descriptor_enum_actor")
                                   .field("colors", &descriptor_enum_actor::colors)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<descriptor_enum_actor>()));

  const descriptor_enum_actor actor{{descriptor_enum_color::blue, descriptor_enum_color::red}};
  const caors::serialization::serialization_text_result text = caors::serialize_json(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"colors":[2,1]})");

  descriptor_enum_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"colors":[1,2]})");
  REQUIRE(result.succeeded());
  REQUIRE(decoded.colors.size() == 2);
  CHECK(decoded.colors[0] == descriptor_enum_color::red);
  CHECK(decoded.colors[1] == descriptor_enum_color::blue);
}

TEST_CASE("serialization - context memory resource constructs empty wrapper values",
          "[serialization]") {
  require_context_resource_holder_metadata();

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory_resource = &tracking;

  std::size_t allocation_count = 0;
  {
    context_resource_holder decoded{};
    const caors::error status = caors::deserialize_json(
        decoded,
        R"({"unique":{"value":7},"shared":{"value":9},"names":["Ada","Grace"],"maybe":"present","choice":[1,"variant"]})",
        context);
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
