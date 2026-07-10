// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/object.h"
#include "caors/reflection.h"

#include <catch2/catch_all.hpp>

namespace {

struct capability_target : caors::object {};

class capability_registered_object : public caors::object_base<capability_registered_object> {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.object.capability_registered_object";
};

class capability_meta_only_object : public caors::object_base<capability_meta_only_object> {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.object.capability_meta_only_object";
};

struct capability_owner {
  caors::object_ptr<capability_target> pointer{};
  caors::object_handle handle{};
  caors::typed_object_handle<capability_target> typed_handle{};
};

} // namespace

TEST_CASE("object - pointer and handle expose static meta capabilities", "[object]") {
  const auto* pointer = caors::meta::capabilities::get<caors::objects::ptr_ops,
                                                       caors::object_ptr<capability_target>>();
  REQUIRE(pointer != nullptr);
  CHECK(pointer->valid());

  const auto* handle =
      caors::meta::capabilities::get<caors::objects::handle_ops, caors::object_handle>();
  REQUIRE(handle != nullptr);
  CHECK(handle->valid());

  const auto* typed_handle =
      caors::meta::capabilities::get<caors::objects::handle_ops,
                                     caors::typed_object_handle<capability_target>>();
  REQUIRE(typed_handle != nullptr);
  CHECK(typed_handle->valid());
}

TEST_CASE("object - factory ops are exposed as object capabilities", "[object]") {
  const auto* factory = caors::meta::capabilities::get<caors::objects::object_factory_ops,
                                                       capability_registered_object>();
  REQUIRE(factory != nullptr);
  CHECK(factory->constructible());

  const caors::meta::type meta = caors::meta::resolve<capability_registered_object>();
  CHECK(meta.get_capability<caors::objects::object_factory_ops>() != nullptr);
}

TEST_CASE("object - static factory capability does not imply object registry membership",
          "[object]") {
  const caors::meta::type meta = caors::meta::resolve<capability_meta_only_object>();
  REQUIRE(meta.get_capability<caors::objects::object_factory_ops>() != nullptr);

  CHECK_FALSE(caors::objects::contains(meta.id()));
  CHECK_FALSE(caors::objects::find<capability_meta_only_object>());
}

TEST_CASE("object - descriptor exposes object capabilities", "[object]") {
  caors::object_ptr<capability_target> pointer{};
  const caors::any_ref pointer_ref = caors::any_ref::from(pointer);
  CHECK(pointer_ref.get_type().get_capability<caors::objects::ptr_ops>() != nullptr);

  caors::typed_object_handle<capability_target> handle{};
  const caors::any_ref handle_ref = caors::any_ref::from(handle);
  CHECK(handle_ref.get_type().get_capability<caors::objects::handle_ops>() != nullptr);
}

TEST_CASE("object - reflected fields use descriptor object capabilities", "[object]") {
  REQUIRE(((void)caors::meta::type_<capability_owner>("caors.test.object.capability_owner"),
           caors::reflection::class_<capability_owner>())
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
