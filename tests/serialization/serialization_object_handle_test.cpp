// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string>

using namespace caors::tests::serialization;

namespace {

struct object_handle_actor {
  caors::object_handle target{};
  caors::typed_object_handle<object_ptr_target> typed{};
};

void require_object_handle_actor_metadata() {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_handle_actor>("caors.test.serialization.object_handle_actor")
          .field("target", &object_handle_actor::target)
          .field("typed", &object_handle_actor::typed)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_handle_actor>()));
}

void require_object_handle_target_metadata() {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
}

void require_handle_value(const caors::variant& value, caors::object_id id,
                          caors::object_type_id type) {
  REQUIRE(value.is_object());

  const caors::value::variant_member* id_member = value.find_member(caors::string_name{"id"});
  const caors::value::variant_member* type_member = value.find_member(caors::string_name{"type"});

  REQUIRE(id_member != nullptr);
  REQUIRE(type_member != nullptr);
  REQUIRE(id_member->value.is_u64());
  REQUIRE(type_member->value.is_u64());
  CHECK(id_member->value.as_u64() == id.value());
  CHECK(type_member->value.as_u64() == type.value());
}

} // namespace

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

TEST_CASE("serialization - deserializes object handles without resolving objects",
          "[serialization]") {
  require_object_handle_actor_metadata();

  object_handle_actor actor{};
  const caors::error result = caors::deserialize_json(
      actor, R"({"target":{"id":77,"type":99},"typed":{"id":88,"type":111}})");

  REQUIRE(result.succeeded());
  CHECK(actor.target == caors::object_handle{caors::object_id{77}, caors::object_type_id{99}});
  CHECK(actor.typed.handle() ==
        caors::object_handle{caors::object_id{88}, caors::object_type_id{111}});
}

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

  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);
  REQUIRE(json.succeeded());

  object_handle_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value);
  REQUIRE(result.succeeded());

  CHECK(decoded.target == actor.target);
  CHECK(decoded.typed == actor.typed);
  CHECK(manager.find(decoded.target) == target);
  CHECK(manager.find(decoded.typed) == target);

  REQUIRE(manager.destroy(target));
  CHECK(manager.find(decoded.target) == nullptr);
  CHECK(manager.find(decoded.typed) == nullptr);
}
