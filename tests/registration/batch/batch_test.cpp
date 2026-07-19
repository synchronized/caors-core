// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"
#include "caors/registration.h"
#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

namespace {

struct batch_actor {
  int id = 0;
};

enum class batch_state {
  idle,
  running,
};

struct batch_duplicate_type {
  int value = 0;
};

struct batch_after_failure_type {
  int value = 0;
};

struct batch_context_type {
  int value = 0;
};

enum class batch_context_enum {
  first,
};

enum class batch_duplicate_enum {
  first,
  second,
};

struct batch_after_enum_failure_type {
  int value = 0;
};

} // namespace

TEST_CASE("registration batch - commits type and enum in order", "[registration]") {
  caors::registration::batch_builder batch = caors::registration::batch();

  batch.type_<batch_actor>("caors.test.registration.batch.actor")
      .meta()
      .property("id", &batch_actor::id)
      .serialization()
      .field("id");

  batch.enum_<batch_state>("caors.test.registration.batch.state")(
      caors::registration::value("idle", batch_state::idle),
      caors::registration::value("running", batch_state::running));

  REQUIRE(batch.size() == 2);
  REQUIRE(batch.commit() == caors::error{});
  CHECK(batch.commit() == caors::error{});
  CHECK(batch.succeeded());
  CHECK_FALSE(batch.has_diagnostic());
  CHECK(batch.diagnostic() == nullptr);
  CHECK(batch.diagnostic_message().empty());

  const caors::meta::type actor_type = caors::meta::find<batch_actor>();
  REQUIRE(actor_type.registered());
  CHECK(actor_type.name() == caors::core::string_name{"caors.test.registration.batch.actor"});
  CHECK(static_cast<bool>(actor_type.find_property("id")));
  CHECK(caors::serialization::contains(actor_type));

  const caors::meta::type state_type = caors::meta::find<batch_state>();
  REQUIRE(state_type.registered());
  const caors::meta::enum_capability values =
      state_type.get_capability<caors::meta::enum_capability>();
  REQUIRE(values);
  CHECK(values.value_count() == 2);
  CHECK(values.find_value("running").get<batch_state>() == batch_state::running);
}

TEST_CASE("registration batch - stops at first failed item and records diagnostic",
          "[registration]") {
  REQUIRE(
      caors::registration::type_<batch_duplicate_type>("caors.test.registration.batch.duplicate")
          .commit() == caors::error{});

  caors::registration::batch_builder batch = caors::registration::batch();
  batch.type_<batch_duplicate_type>("caors.test.registration.batch.duplicate");
  batch.type_<batch_after_failure_type>("caors.test.registration.batch.after_failure");

  const caors::error status = batch.commit();
  REQUIRE(status);
  CHECK(status.value() == caors::errc::duplicate_id);

  const caors::registration::batch_diagnostic* diagnostic = batch.diagnostic();
  REQUIRE(diagnostic != nullptr);
  CHECK(diagnostic->kind == caors::registration::batch_item_kind::type);
  CHECK(diagnostic->index == 0);
  CHECK(diagnostic->id == caors::meta::id_of<batch_duplicate_type>());
  CHECK(diagnostic->name == caors::core::string_name{"caors.test.registration.batch.duplicate"});
  CHECK(diagnostic->status == status);
  CHECK(batch.has_diagnostic());
  CHECK(batch.failed_kind() == caors::registration::batch_item_kind::type);
  CHECK(batch.failed_index() == 0);
  CHECK(batch.failed_type_id() == caors::meta::id_of<batch_duplicate_type>());
  CHECK(batch.failed_name() == caors::core::string_name{"caors.test.registration.batch.duplicate"});
  CHECK(batch.diagnostic_message() ==
        "registration batch failed at item 0: type caors.test.registration.batch.duplicate "
        "duplicate id");

  CHECK_FALSE(caors::meta::find<batch_after_failure_type>());
}

TEST_CASE("registration batch - records enum item failure and stops following items",
          "[registration]") {
  caors::registration::batch_builder batch = caors::registration::batch();
  batch.enum_<batch_duplicate_enum>("caors.test.registration.batch.duplicate_enum")
      .value("same", batch_duplicate_enum::first)
      .value("same", batch_duplicate_enum::second);
  batch.type_<batch_after_enum_failure_type>("caors.test.registration.batch.after_enum_failure");

  const caors::error status = batch.commit();
  REQUIRE(status);
  CHECK(status.value() == caors::errc::duplicate_name);

  const caors::registration::batch_diagnostic* diagnostic = batch.diagnostic();
  REQUIRE(diagnostic != nullptr);
  CHECK(diagnostic->kind == caors::registration::batch_item_kind::enumeration);
  CHECK(diagnostic->index == 0);
  CHECK(diagnostic->id == caors::meta::id_of<batch_duplicate_enum>());
  CHECK(diagnostic->name ==
        caors::core::string_name{"caors.test.registration.batch.duplicate_enum"});
  CHECK(batch.diagnostic_message() ==
        "registration batch failed at item 0: enum caors.test.registration.batch.duplicate_enum "
        "duplicate name");

  CHECK_FALSE(caors::meta::find<batch_duplicate_enum>());
  CHECK_FALSE(caors::meta::find<batch_after_enum_failure_type>());
}

TEST_CASE("registration batch - explicit context registers into local registry", "[registration]") {
  caors::meta::registry registry;
  caors::meta::context ctx{registry};
  caors::registration::batch_builder batch = caors::registration::batch(ctx);

  batch.type_<batch_context_type>("caors.test.registration.batch.context_type")
      .meta()
      .property("value", &batch_context_type::value);
  batch.enum_<batch_context_enum>("caors.test.registration.batch.context_enum")
      .value("first", batch_context_enum::first);

  REQUIRE(batch.commit() == caors::error{});

  CHECK(caors::meta::find<batch_context_type>(ctx).registered());
  CHECK(caors::meta::find<batch_context_enum>(ctx).registered());
  CHECK_FALSE(caors::meta::find<batch_context_type>());
  CHECK_FALSE(caors::meta::find<batch_context_enum>());
}
