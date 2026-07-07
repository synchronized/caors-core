// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes object pointers inside vectors", "[serialization]") {
  require_object_ptr_vector_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;

  object_ptr_vector_actor actor{};
  const caors::error result = caors::deserialize_json(context, actor, R"({"targets":[88,0]})");

  REQUIRE(result.succeeded());
  REQUIRE(actor.targets.size() == 2);
  CHECK(actor.targets[0].get() == &target);
  CHECK(actor.targets[1].get() == nullptr);
}
