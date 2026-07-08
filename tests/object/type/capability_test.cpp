// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/object.h"
#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

namespace {

struct capability_target : caors::object {};

struct capability_owner {
  caors::object_ptr<capability_target> pointer{};
  caors::object_handle handle{};
  caors::typed_object_handle<capability_target> typed_handle{};
};

} // namespace

TEST_CASE("object - pointer and handle expose static meta capabilities", "[object]") {
  const auto* pointer = caors::meta::capabilities::get<caors::objects::detail::ptr_data,
                                                       caors::object_ptr<capability_target>>();
  REQUIRE(pointer != nullptr);
  CHECK(pointer->valid());

  const auto* handle =
      caors::meta::capabilities::get<caors::objects::detail::handle_data, caors::object_handle>();
  REQUIRE(handle != nullptr);
  CHECK(handle->valid());

  const auto* typed_handle =
      caors::meta::capabilities::get<caors::objects::detail::handle_data,
                                     caors::typed_object_handle<capability_target>>();
  REQUIRE(typed_handle != nullptr);
  CHECK(typed_handle->valid());
}

TEST_CASE("object - reflected fields register object capabilities by type id", "[object]") {
  REQUIRE(caors::reflection::class_<capability_owner>("caors.test.object.capability_owner")
              .field("pointer", &capability_owner::pointer)
              .field("handle", &capability_owner::handle)
              .field("typed_handle", &capability_owner::typed_handle)
              .commit()
              .succeeded());

  const auto pointer_type = caors::meta::id_of<caors::object_ptr<capability_target>>();
  CHECK(caors::meta::capabilities::contains<caors::objects::detail::ptr_data>(pointer_type));

  const auto handle_type = caors::meta::id_of<caors::object_handle>();
  CHECK(caors::meta::capabilities::contains<caors::objects::detail::handle_data>(handle_type));

  const auto typed_handle_type =
      caors::meta::id_of<caors::typed_object_handle<capability_target>>();
  CHECK(
      caors::meta::capabilities::contains<caors::objects::detail::handle_data>(typed_handle_type));
}

TEST_CASE("object - any_ref resolves object capabilities through runtime index", "[object]") {
  caors::object_ptr<capability_target> pointer{};
  const caors::any_ref pointer_ref = caors::any_ref::from(pointer);
  const caors::objects::ptr_view pointer_view = caors::objects::ptr_view_of(pointer_ref);
  CHECK(pointer_view);

  caors::typed_object_handle<capability_target> handle{};
  const caors::any_ref handle_ref = caors::any_ref::from(handle);
  const caors::objects::handle_view handle_view = caors::objects::handle_view_of(handle_ref);
  CHECK(handle_view);
}
