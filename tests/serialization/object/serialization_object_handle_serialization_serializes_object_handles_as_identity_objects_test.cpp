// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_handle_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_handle;
TEST_CASE("serialization - serializes object handles as identity objects", "[serialization]") {
  require_object_handle_actor_metadata();

  const caors::objects::handle handle{caors::object_id{42}, caors::object_type_id{84}};
  const caors::serialization::variant_result direct =
      caors::serialization::to_variant(caors::any_ref::from_const(handle));
  REQUIRE(direct.succeeded());
  require_handle_value(direct.value, handle.id(), handle.type());

  const object_handle_actor actor{
      handle,
      caors::typed_object_handle<object_ptr_target>{handle},
  };

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"target":{"id":42,"type":84},"typed":{"id":42,"type":84}})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* target =
      value.value.find_member(caors::string_name{"target"});
  const caors::value::variant_member* typed = value.value.find_member(caors::string_name{"typed"});
  REQUIRE(target != nullptr);
  REQUIRE(typed != nullptr);
  require_handle_value(target->value, handle.id(), handle.type());
  require_handle_value(typed->value, handle.id(), handle.type());
}
