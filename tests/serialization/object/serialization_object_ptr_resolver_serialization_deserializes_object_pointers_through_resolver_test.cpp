// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ptr_resolver_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_resolver;
TEST_CASE("serialization - deserializes object pointers through resolver", "[serialization]") {
  require_object_ptr_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{88}),
  });

  object_ptr_actor actor{};
  const caors::error result =
      caors::deserialize(context, actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.target.get() == &target);

  object_ptr_actor from_json{};
  const caors::error json_result = caors::deserialize_json(context, from_json, R"({"target":88})");
  REQUIRE(json_result.succeeded());
  CHECK(from_json.target.get() == &target);
}
