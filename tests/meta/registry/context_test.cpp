// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

namespace {

struct context_local_type {};
struct context_find_only_type {};
struct context_capability_type {};

struct context_capability {
  int value = 0;
};

} // namespace

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
