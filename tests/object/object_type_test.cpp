// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "object_test_common.h"

using namespace caors::tests::object;

TEST_CASE("object - typed base exposes runtime type information", "[object]") {
  const caors::error actor_status = register_object_test_type<runtime_actor>();
  REQUIRE(
      (actor_status == caors::error{} || actor_status == object_error(caors::errc::duplicate_id)));
  REQUIRE(register_object_test_type<runtime_player>() == caors::error{});

  runtime_player instance;
  caors::object& base = instance;

  CHECK(runtime_player::static_type_id());
  CHECK(runtime_player::static_type_id() == caors::meta::id_of<runtime_player>());
  CHECK(base.type_id() == runtime_player::static_type_id());
  CHECK(base.type_name() == runtime_player::object_type_name);
}

TEST_CASE("object - handles carry identity without owning instances", "[object]") {
  const caors::error register_status = register_object_test_type<runtime_actor>();
  REQUIRE((register_status == caors::error{} ||
           register_status == object_error(caors::errc::duplicate_id)));

  runtime_actor instance;

  const caors::objects::handle handle{instance.id(), instance.type_id()};
  CHECK(handle);
  CHECK(handle.id() == instance.id());
  CHECK(handle.type() == instance.type_id());
  CHECK(handle == caors::objects::handle{instance.id(), instance.type_id()});
  CHECK(handle != caors::objects::handle{caors::object_id{999}, instance.type_id()});

  caors::objects::typed_handle<runtime_actor> typed{instance};
  CHECK(typed);
  CHECK(typed.handle() == handle);
  CHECK(typed == handle);

  const caors::objects::typed_handle<runtime_actor> from_handle{handle};
  CHECK(from_handle.id() == handle.id());
  CHECK(from_handle.type() == handle.type());
  CHECK(from_handle.handle() == handle);

  typed.reset();
  CHECK_FALSE(typed);
  CHECK(typed.handle() == caors::objects::handle{});
}
