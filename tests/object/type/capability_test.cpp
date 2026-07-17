// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/object.h"
#include "caors/meta.h"

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
  const caors::meta::type pointer = caors::meta::resolve<caors::object_ptr<capability_target>>();
  CHECK(pointer.has_capability<caors::objects::ptr_capability>());
  CHECK(pointer.get_capability<caors::objects::ptr_capability>());

  const caors::meta::type handle = caors::meta::resolve<caors::object_handle>();
  CHECK(handle.has_capability<caors::objects::handle_capability>());
  CHECK(handle.get_capability<caors::objects::handle_capability>());

  const caors::meta::type typed_handle =
      caors::meta::resolve<caors::typed_object_handle<capability_target>>();
  CHECK(typed_handle.has_capability<caors::objects::handle_capability>());
  CHECK(typed_handle.get_capability<caors::objects::handle_capability>());
}

TEST_CASE("object - pointer and handle expose public capability facades", "[object]") {
  caors::object_ptr<capability_target> pointer{};
  capability_target target{};
  const caors::meta::type pointer_type =
      caors::meta::resolve<caors::object_ptr<capability_target>>();
  const caors::objects::ptr_capability pointer_capability =
      pointer_type.get_capability<caors::objects::ptr_capability>();
  REQUIRE(pointer_capability);

  const caors::objects::ptr_view pointer_view =
      pointer_capability.bind(caors::any_ref::from(pointer));
  REQUIRE(pointer_view);
  CHECK(pointer_view.set(&target));
  CHECK(pointer.get() == &target);

  caors::object_handle handle{};
  const caors::meta::type handle_type = caors::meta::resolve<caors::object_handle>();
  const caors::objects::handle_capability handle_capability =
      handle_type.get_capability<caors::objects::handle_capability>();
  REQUIRE(handle_capability);
  CHECK(handle_capability.bind(caors::any_ref::from(handle)));

  caors::typed_object_handle<capability_target> typed_handle{};
  const caors::meta::type typed_handle_type =
      caors::meta::resolve<caors::typed_object_handle<capability_target>>();
  const caors::objects::handle_capability typed_handle_capability =
      typed_handle_type.get_capability<caors::objects::handle_capability>();
  REQUIRE(typed_handle_capability);
  CHECK(typed_handle_capability.bind(caors::any_ref::from(typed_handle)));
}

TEST_CASE("object - factory facade is exposed as object capability", "[object]") {
  const caors::meta::type meta = caors::meta::resolve<capability_registered_object>();
  const caors::objects::object_factory_capability factory =
      meta.get_capability<caors::objects::object_factory_capability>();
  REQUIRE(factory);
  CHECK(factory.constructible());

  caors::object* instance = factory.try_create();
  REQUIRE(instance != nullptr);
  CHECK(instance->type_id() == capability_registered_object::static_type_id());
  CHECK(factory.destroy(instance));
}

TEST_CASE("object - static factory capability does not imply object registry membership",
          "[object]") {
  const caors::meta::type meta = caors::meta::resolve<capability_meta_only_object>();
  REQUIRE(meta.get_capability<caors::objects::object_factory_capability>());

  CHECK_FALSE(caors::objects::contains(meta.id()));
  CHECK_FALSE(caors::objects::find<capability_meta_only_object>());
}

TEST_CASE("object - descriptor exposes object capabilities", "[object]") {
  caors::object_ptr<capability_target> pointer{};
  const caors::any_ref pointer_ref = caors::any_ref::from(pointer);
  CHECK(pointer_ref.get_type().has_capability<caors::objects::ptr_capability>());
  const caors::objects::ptr_capability pointer_capability =
      pointer_ref.get_type().get_capability<caors::objects::ptr_capability>();
  REQUIRE(pointer_capability);
  CHECK(pointer_capability.bind(pointer_ref));
  CHECK(pointer_ref.get_capability<caors::objects::ptr_capability>());

  caors::typed_object_handle<capability_target> handle{};
  const caors::any_ref handle_ref = caors::any_ref::from(handle);
  CHECK(handle_ref.get_type().has_capability<caors::objects::handle_capability>());
  const caors::objects::handle_capability handle_capability =
      handle_ref.get_type().get_capability<caors::objects::handle_capability>();
  REQUIRE(handle_capability);
  CHECK(handle_capability.bind(handle_ref));
  CHECK(handle_ref.get_capability<caors::objects::handle_capability>());
}

TEST_CASE("object - reflected fields use descriptor object capabilities", "[object]") {
  REQUIRE(((void)caors::meta::class_<capability_owner>("caors.test.object.capability_owner"),
           caors::meta::class_<capability_owner>())
              .field("pointer", &capability_owner::pointer)
              .field("handle", &capability_owner::handle)
              .field("typed_handle", &capability_owner::typed_handle)
              .commit()
              .succeeded());

  auto reflected = caors::meta::find("caors.test.object.capability_owner");
  REQUIRE(reflected);

  capability_owner owner{};
  capability_target target{};
  const caors::meta::field pointer = reflected.find_field("pointer");
  REQUIRE(pointer);
  const caors::objects::ptr_view pointer_view =
      pointer.get_ref(&owner).get_capability<caors::objects::ptr_capability>();
  REQUIRE(pointer_view);
  CHECK(pointer_view.set(&target));
  CHECK(owner.pointer.get() == &target);
}

TEST_CASE("object - any_ref resolves object capabilities through descriptor", "[object]") {
  caors::object_ptr<capability_target> pointer{};
  const caors::any_ref pointer_ref = caors::any_ref::from(pointer);
  const caors::objects::ptr_view pointer_view =
      pointer_ref.get_capability<caors::objects::ptr_capability>();
  CHECK(pointer_view);

  caors::typed_object_handle<capability_target> handle{};
  const caors::any_ref handle_ref = caors::any_ref::from(handle);
  const caors::objects::handle_view handle_view =
      handle_ref.get_capability<caors::objects::handle_capability>();
  CHECK(handle_view);
}
