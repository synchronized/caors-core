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

TEST_CASE("object - descriptor exposes object capabilities", "[object]") {
  caors::object_ptr<capability_target> pointer{};
  const caors::any_ref pointer_ref = caors::any_ref::from(pointer);
  const auto* pointer_descriptor = caors::value::detail::descriptor_of(pointer_ref);
  CHECK(caors::meta::detail::descriptor_static_capability<caors::objects::detail::ptr_data>(
            pointer_descriptor) != nullptr);

  caors::typed_object_handle<capability_target> handle{};
  const caors::any_ref handle_ref = caors::any_ref::from(handle);
  const auto* handle_descriptor = caors::value::detail::descriptor_of(handle_ref);
  CHECK(caors::meta::detail::descriptor_static_capability<caors::objects::detail::handle_data>(
            handle_descriptor) != nullptr);
}

TEST_CASE("object - reflected fields use descriptor object capabilities", "[object]") {
  REQUIRE(caors::reflection::class_<capability_owner>("caors.test.object.capability_owner")
              .field("pointer", &capability_owner::pointer)
              .field("handle", &capability_owner::handle)
              .field("typed_handle", &capability_owner::typed_handle)
              .commit()
              .succeeded());

  auto reflected = caors::reflection::find("caors.test.object.capability_owner");
  REQUIRE(reflected);

  capability_owner owner{};
  capability_target target{};
  caors::reflection::field pointer = reflected.find_field("pointer");
  REQUIRE(pointer);
  CHECK(pointer.try_set_object_ptr(&owner, &target));
  CHECK(owner.pointer.get() == &target);
}

TEST_CASE("object - any_ref resolves object capabilities through descriptor", "[object]") {
  caors::object_ptr<capability_target> pointer{};
  const caors::any_ref pointer_ref = caors::any_ref::from(pointer);
  const caors::objects::ptr_view pointer_view = caors::objects::ptr_view_of(pointer_ref);
  CHECK(pointer_view);

  caors::typed_object_handle<capability_target> handle{};
  const caors::any_ref handle_ref = caors::any_ref::from(handle);
  const caors::objects::handle_view handle_view = caors::objects::handle_view_of(handle_ref);
  CHECK(handle_view);
}
