// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

namespace caors::tests::meta_context {

struct context_local_type {};
struct context_find_only_type {};
struct context_capability_type {};
struct context_identity_type {};

inline constexpr type_guid context_identity_guid{
    guid::from_bytes({0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xA9, 0xBA, 0xCB, 0xDC, 0xED,
                      0xFE, 0x0F, 0x10})};

struct context_capability {
  int value = 0;
};

} // namespace caors::tests::meta_context

namespace caors::traits {

template <> struct type_identity<tests::meta_context::context_identity_type> {
  [[nodiscard]] static constexpr std::string_view name() noexcept {
    return "caors.test.meta.context.identity";
  }

  [[nodiscard]] static constexpr core::type_guid guid() noexcept {
    return tests::meta_context::context_identity_guid;
  }
};

} // namespace caors::traits

using namespace caors::tests::meta_context;

TEST_CASE("meta context - local registry isolates registered type data", "[meta]") {
  caors::meta::registry registry;
  caors::meta::context ctx{registry};

  const caors::meta::type descriptor = caors::meta::resolve<context_local_type>(ctx);
  REQUIRE(descriptor);
  CHECK_FALSE(descriptor.registered());

  REQUIRE(caors::registration::type_<context_local_type>(ctx, "caors.test.meta.context.local")
              .commit() == caors::error{});

  const caors::meta::type local = caors::meta::find<context_local_type>(ctx);
  REQUIRE(local);
  CHECK(local.registered());
  CHECK(local.name() == "caors.test.meta.context.local");
  CHECK(caors::meta::find(ctx, "caors.test.meta.context.local").id() == local.id());
  CHECK(caors::meta::id(ctx, "caors.test.meta.context.local") == local.id());

  CHECK_FALSE(caors::meta::find<context_local_type>());
  CHECK_FALSE(caors::meta::find("caors.test.meta.context.local"));
}

TEST_CASE("meta context - typed find does not mutate local registry", "[meta]") {
  caors::meta::registry registry;
  caors::meta::context ctx{registry};

  CHECK(registry.size() == 0);
  CHECK_FALSE(caors::meta::find<context_find_only_type>(ctx));
  CHECK(registry.size() == 0);

  const caors::meta::type descriptor = caors::meta::resolve<context_find_only_type>(ctx);
  REQUIRE(descriptor);
  CHECK_FALSE(descriptor.registered());
  CHECK(registry.size() == 0);
}

TEST_CASE("meta context - dynamic capability lookup uses owning registry", "[meta]") {
  caors::meta::registry registry;
  caors::meta::context ctx{registry};

  const context_capability capability{42};
  REQUIRE(
      caors::registration::type_<context_capability_type>(ctx, "caors.test.meta.context.capability")
          .meta()
          .capability(&capability)
          .commit() == caors::error{});

  const caors::meta::type local = caors::meta::find<context_capability_type>(ctx);
  REQUIRE(local);
  const context_capability* found = local.get_capability<context_capability>();
  REQUIRE(found != nullptr);
  CHECK(found->value == 42);

  CHECK_FALSE(caors::meta::find<context_capability_type>());
  CHECK(caors::meta::capabilities::find<context_capability>(local.id()) == nullptr);
}

TEST_CASE("meta context - static identity is shared by independent registries", "[meta]") {
  caors::meta::registry first_registry;
  caors::meta::registry second_registry;
  caors::meta::context first{first_registry};
  caors::meta::context second{second_registry};

  REQUIRE(
      caors::registration::type_<context_identity_type>(first, "caors.test.meta.context.identity")
          .commit() == caors::error{});
  REQUIRE(
      caors::registration::type_<context_identity_type>(second, "caors.test.meta.context.identity")
          .commit() == caors::error{});

  const caors::meta::type first_type = caors::meta::find<context_identity_type>(first);
  const caors::meta::type second_type = caors::meta::find<context_identity_type>(second);
  REQUIRE(first_type);
  REQUIRE(second_type);
  CHECK(first_type.id() == second_type.id());
  CHECK(first_type.name() == second_type.name());
  CHECK(first_type.guid() == context_identity_guid);
  CHECK(second_type.guid() == context_identity_guid);
  CHECK(first_registry.size() == 1);
  CHECK(second_registry.size() == 1);
}
