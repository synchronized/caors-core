// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_object_handle_serialization_deserializes_object_handles_without_resolving_objects_test.cpp

#include "serialization_object_handle_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_handle;
TEST_CASE("serialization - deserializes object handles without resolving objects",
          "[serialization]") {
  require_object_handle_actor_metadata();

  object_handle_actor actor{};
  const caors::error result = caors::read_json_from(
      actor, R"({"target":{"id":77,"type":99},"typed":{"id":88,"type":111}})");

  REQUIRE(result.succeeded());
  CHECK(actor.target == caors::object_handle{caors::object_id{77}, caors::type_id{99}});
  CHECK(actor.typed.handle() ==
        caors::object_handle{caors::object_id{88}, caors::type_id{111}});
}

// 原测试文件：serialization_object_handle_serialization_object_manager_resolves_deserialized_typed_handles_test.cpp

#include "serialization_object_handle_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_handle;
TEST_CASE("serialization - object manager resolves deserialized typed handles", "[serialization]") {
  require_object_handle_target_metadata();
  require_object_handle_actor_metadata();

  caors::objects::manager manager;
  object_ptr_target* target = manager.create<object_ptr_target>();
  REQUIRE(target != nullptr);

  object_handle_actor actor{};
  actor.target = manager.handle_of(target->id());
  actor.typed = manager.handle_of<object_ptr_target>(target->id());
  REQUIRE(actor.target);
  REQUIRE(actor.typed);

  const caors::serialization::serialization_text_result json = caors::to_json(actor);
  REQUIRE(json.succeeded());

  object_handle_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, json.value);
  REQUIRE(result.succeeded());

  CHECK(decoded.target == actor.target);
  CHECK(decoded.typed == actor.typed);
  CHECK(manager.find(decoded.target) == target);
  CHECK(manager.find(decoded.typed) == target);

  REQUIRE(manager.destroy(target));
  CHECK(manager.find(decoded.target) == nullptr);
  CHECK(manager.find(decoded.typed) == nullptr);
}

// 原测试文件：serialization_object_handle_serialization_serializes_object_handles_as_identity_objects_test.cpp

#include "serialization_object_handle_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_handle;
TEST_CASE("serialization - serializes object handles as identity objects", "[serialization]") {
  require_object_handle_actor_metadata();

  const caors::objects::handle handle{caors::object_id{42}, caors::type_id{84}};
  const caors::serialization::variant_result direct =
      caors::serialization::to_variant(caors::any_ref::from_const(handle));
  REQUIRE(direct.succeeded());
  require_handle_value(direct.value, handle.id(), handle.type());

  const object_handle_actor actor{
      handle,
      caors::typed_object_handle<object_ptr_target>{handle},
  };

  const caors::serialization::serialization_text_result json = caors::to_json(actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"target":{"id":42,"type":84},"typed":{"id":42,"type":84}})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::meta::variant_member* target = value.value.find_member(caors::string_name{"target"});
  const caors::meta::variant_member* typed = value.value.find_member(caors::string_name{"typed"});
  REQUIRE(target != nullptr);
  REQUIRE(typed != nullptr);
  require_handle_value(target->value, handle.id(), handle.type());
  require_handle_value(typed->value, handle.id(), handle.type());
}
