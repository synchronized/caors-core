// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：meta_type_builder_meta_type_builder_updates_registered_data_in_place_test.cpp

#include "meta_type_builder_test_support.h"

#include <array>
#include <span>

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - builder updates registered data in place", "[meta]") {
  const auto guid = caors::guid::parse("2f7cc996-e0bc-4b61-85ea-b0263b876081");
  REQUIRE(guid.has_value());

  auto builder = caors::meta::class_<stable_builder_type>().type("caors.test.type.stable_builder");
  const caors::meta::type before = builder.get();
  REQUIRE(before);
  CHECK_FALSE(before.guid());
  const caors::any before_value = caors::any::default_construct(before);
  REQUIRE(before_value.valid());
  CHECK(before_value.size() == sizeof(stable_builder_type));
  CHECK(before_value.alignment() == alignof(stable_builder_type));

  REQUIRE(builder.guid(caors::type_guid{*guid}).result() == caors::error{});
  const caors::meta::type after = builder.get();
  REQUIRE(after);

  CHECK(before.id() == after.id());
  const caors::any after_value = caors::any::default_construct(after);
  REQUIRE(after_value.valid());
  CHECK(after_value.size() == sizeof(stable_builder_type));
  CHECK(after_value.alignment() == alignof(stable_builder_type));
  CHECK(before.guid() == caors::type_guid{*guid});
  CHECK(after.guid() == caors::type_guid{*guid});
}

// 原测试文件：meta_type_builder_meta_type_builder_without_name_does_not_register_type_test.cpp

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - builder without name does not register type", "[meta]") {
  const auto builder = caors::meta::class_<lazy_meta_builder_type>();

  CHECK_FALSE(builder.get());
  CHECK(caors::meta::resolve<lazy_meta_builder_type>());
  CHECK_FALSE(caors::meta::resolve<lazy_meta_builder_type>().registered());
  CHECK_FALSE(caors::meta::find<lazy_meta_builder_type>());
}

// 原测试文件：meta_type_builder_meta_type_explicit_type_access_does_not_create_missing_types_test.cpp

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - resolve creates descriptor-only views", "[meta]") {
  const caors::meta::type missing = caors::meta::resolve<missing_find_type>();
  REQUIRE(missing);
  CHECK_FALSE(missing.registered());
  CHECK_FALSE(caors::meta::find<missing_find_type>());

  const caors::meta::type type =
      caors::meta::class_<explicit_find_type>("caors.test.type.explicit_find").get();
  REQUIRE(type.valid());
  CHECK(type.registered());

  const caors::meta::type found = caors::meta::find<explicit_find_type>();
  REQUIRE(found.valid());
  CHECK(found.registered());
  CHECK(found.id() == type.id());
}

// 原测试文件：meta_type_builder_meta_type_fluent_builder_registers_name_and_guid_test.cpp

#include "meta_type_builder_test_support.h"

using namespace caors::tests::meta_type_builder;
TEST_CASE("meta type - fluent builder registers name and guid", "[meta]") {
  const auto guid = caors::guid::parse("d2953377-4ffc-4c81-aeef-7e020272a536");
  REQUIRE(guid.has_value());

  auto builder = caors::meta::class_<fluent_meta_type>()
                     .type("caors.test.type.fluent_meta")
                     .guid(caors::type_guid{*guid});

  CHECK(builder.succeeded());
  const caors::meta::type type = builder.get();
  REQUIRE(type);
  CHECK(type.name() == "caors.test.type.fluent_meta");
  CHECK(type.guid() == caors::type_guid{*guid});
  CHECK(caors::meta::find<fluent_meta_type>().id() == type.id());
  CHECK(caors::meta::find(caors::type_guid{*guid}).id() == type.id());
}

TEST_CASE("meta type - class builder registers fields and methods", "[meta]") {
  auto builder = caors::meta::class_<reflected_meta_actor>("caors.test.meta.reflected_actor");
  builder(caors::meta::metadata("category", "gameplay"))
      .field("health", &reflected_meta_actor::health)(caors::meta::metadata("display", "Health"))
      .method("add_health",
              &reflected_meta_actor::add_health)(caors::meta::metadata("scriptable", true));

  REQUIRE(builder.succeeded());

  const caors::meta::type type = caors::meta::find<reflected_meta_actor>();
  REQUIRE(type);
  CHECK(type.field_count() == 1);
  CHECK(type.method_count() == 1);
  const caors::string_name category_key{"category"};
  REQUIRE(type.find_metadata(category_key) != nullptr);
  CHECK(type.find_metadata(category_key)->as_string() == "gameplay");

  const caors::meta::field health = type.find_field("health");
  REQUIRE(health);
  CHECK(health.name() == "health");
  CHECK(health.get_owner_type().id() == type.id());
  CHECK(health.get_type().id() == caors::meta::id_of<int>());
  const caors::string_name display_key{"display"};
  REQUIRE(health.find_metadata(display_key) != nullptr);
  CHECK(health.find_metadata(display_key)->as_string() == "Health");

  reflected_meta_actor actor{};
  CHECK(health.try_set(&actor, 20));
  CHECK(actor.health == 20);

  const caors::meta::method add_health = type.find_method("add_health");
  REQUIRE(add_health);
  CHECK(add_health.parameter_count() == 1);
  const caors::string_name scriptable_key{"scriptable"};
  REQUIRE(add_health.find_metadata(scriptable_key) != nullptr);
  CHECK(add_health.find_metadata(scriptable_key)->as_bool());

  int delta = 5;
  std::array<caors::any_ref, 1> arguments{caors::any_ref::from(delta)};
  const caors::any result = add_health.invoke(
      &actor, std::span<const caors::any_ref>{arguments.data(), arguments.size()});
  REQUIRE(result);
  REQUIRE(result.try_get<int>() != nullptr);
  CHECK(*result.try_get<int>() == 25);
  CHECK(actor.health == 25);
}

TEST_CASE("meta enum - builder registers values and metadata", "[meta]") {
  auto builder = caors::meta::enum_<reflected_meta_state>("caors.test.meta.reflected_state")(
                     caors::meta::enum_value("idle", reflected_meta_state::idle),
                     caors::meta::enum_value("running", reflected_meta_state::running))
                     .value("hidden", reflected_meta_state::hidden)(
                         caors::meta::metadata("editor_hidden", true));

  REQUIRE(builder.succeeded());

  const caors::meta::type type = caors::meta::find<reflected_meta_state>();
  REQUIRE(type);
  CHECK(type.kind() == caors::meta::type_kind::enumeration);
  const caors::meta::enum_capability values = type.get_capability<caors::meta::enum_capability>();
  REQUIRE(values);
  CHECK(values.value_count() == 3);

  const caors::meta::enum_value_view idle = values.find_value("idle");
  REQUIRE(idle);
  CHECK(idle.name() == "idle");
  REQUIRE(idle.get<reflected_meta_state>().has_value());
  CHECK(*idle.get<reflected_meta_state>() == reflected_meta_state::idle);

  const caors::meta::enum_value_view running = values.find_value(reflected_meta_state::running);
  REQUIRE(running);
  CHECK(running.name() == "running");
  CHECK(running.to_i64() == 2);
  CHECK(running.to_u64() == 2U);

  const caors::meta::enum_value_view hidden = values.value_at(2);
  REQUIRE(hidden);
  CHECK(hidden.name() == "hidden");
  const caors::string_name editor_hidden_key{"editor_hidden"};
  REQUIRE(hidden.find_metadata(editor_hidden_key) != nullptr);
  CHECK(hidden.find_metadata(editor_hidden_key)->as_bool());
}

TEST_CASE("meta enum - builder rejects duplicate value names", "[meta]") {
  auto builder = caors::meta::enum_<duplicate_meta_state>("caors.test.meta.duplicate_state")
                     .value("same", duplicate_meta_state::first);
  REQUIRE(builder.succeeded());

  const auto duplicate = builder.value("same", duplicate_meta_state::second);
  CHECK(duplicate.result() == caors::error{caors::error_domain::meta, caors::errc::duplicate_name});

  const caors::meta::type type = caors::meta::find<duplicate_meta_state>();
  REQUIRE(type);
  const caors::meta::enum_capability values = type.get_capability<caors::meta::enum_capability>();
  REQUIRE(values);
  CHECK(values.value_count() == 1);
}
