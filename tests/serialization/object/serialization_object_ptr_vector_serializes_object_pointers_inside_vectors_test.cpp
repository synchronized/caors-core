// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes object pointers inside vectors", "[serialization]") {
  require_object_ptr_vector_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;

  object_ptr_vector_actor actor{};
  actor.targets.push_back(caors::object_ptr<object_ptr_target>{&target});
  actor.targets.push_back(caors::object_ptr<object_ptr_target>{});

  const caors::serialization::serialization_text_result text =
      caors::serialize_json(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"targets":[77,0]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* targets =
      value.value.find_member(caors::string_name{"targets"});
  REQUIRE(targets != nullptr);
  REQUIRE(targets->value.is_array());
  REQUIRE(targets->value.as_array().size() == 2);
  CHECK(targets->value.as_array()[0].as_u64() == 77);
  CHECK(targets->value.as_array()[1].as_u64() == 0);
}
