// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ptr_resolver_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_resolver;
TEST_CASE("serialization - serializes object pointers through resolver", "[serialization]") {
  require_object_ptr_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{&target}};

  const caors::serialization::serialization_text_result text =
      caors::serialize_text(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"target":77})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_u64());
  CHECK(target_member->value.as_u64() == 77);

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"target":77})");
}
