// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_polymorphic_value_wrapper_polymorphic_value_envelope_names_are_configurable_test.cpp

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value envelope names are configurable", "[serialization]") {
  require_polymorphic_value_metadata();

  caors::serialization::context context{};
  context.config.polymorphic.type_field = caors::string_name{"kind"};
  context.config.polymorphic.value_field = caors::string_name{"data"};
  context.config.polymorphic.type_name_field = caors::string_name{"debug_type"};
  context.config.polymorphic.write_type_name = true;

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 29;
  owner->name = "Uma";
  holder.owner = std::move(owner);

  const caors::serialization::variant_result value = caors::to_value(context, holder);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::meta::variant_member* owner_member =
      value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner_member != nullptr);
  REQUIRE(owner_member->value.is_object());

  CHECK(owner_member->value.find_member(caors::string_name{"$type"}) == nullptr);
  CHECK(owner_member->value.find_member(caors::string_name{"$value"}) == nullptr);
  CHECK(owner_member->value.find_member(caors::string_name{"kind"}) != nullptr);
  CHECK(owner_member->value.find_member(caors::string_name{"debug_type"}) != nullptr);

  const caors::meta::variant_member* stored_value =
      owner_member->value.find_member(caors::string_name{"data"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());

  const caors::serialization::serialization_text_result json = caors::to_json(context, holder);
  REQUIRE(json.succeeded());
  CHECK(json.value.find("kind") != caors::string::npos);
  CHECK(json.value.find("data") != caors::string::npos);
  CHECK(json.value.find("debug_type") != caors::string::npos);

  polymorphic_value_holder decoded{};
  const caors::error status = caors::read_json_from(context, decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_value_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 29);
  CHECK(decoded_owner->name == "Uma");
}

// 原测试文件：serialization_polymorphic_value_wrapper_polymorphic_value_wrappers_reject_unrelated_dynamic_types_test.cpp

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value wrappers reject unrelated dynamic types",
          "[serialization]") {
  require_polymorphic_value_metadata();

  caors::variant::object_type enemy_fields;
  enemy_fields.push_back(caors::meta::variant_member{
      caors::string_name{"danger"},
      0,
      caors::variant::from(std::int64_t{9}),
  });

  caors::variant::object_type owner_envelope;
  owner_envelope.push_back(caors::meta::variant_member{
      caors::string_name{"$type"},
      0,
      caors::variant::from(enemy_guid.value.string()),
  });
  owner_envelope.push_back(caors::meta::variant_member{
      caors::string_name{"$value"},
      0,
      caors::variant::object(std::move(enemy_fields)),
  });

  caors::variant::object_type holder_fields;
  holder_fields.push_back(caors::meta::variant_member{
      caors::string_name{"owner"},
      0,
      caors::variant::object(std::move(owner_envelope)),
  });

  polymorphic_value_holder decoded{};
  const caors::error status =
      caors::read_value_from(decoded, caors::variant::object(std::move(holder_fields)));

  CHECK(status.failed());
  CHECK(status.value() == caors::errc::invalid_type);
  CHECK(decoded.owner == nullptr);
}

// 原测试文件：serialization_polymorphic_value_wrapper_polymorphic_value_wrappers_roundtrip_derived_values_test.cpp

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value wrappers roundtrip derived values",
          "[serialization]") {
  require_polymorphic_value_metadata();

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 31;
  owner->name = "Lin";
  holder.owner = std::move(owner);

  auto shared = std::make_shared<polymorphic_value_player>();
  shared->level = 37;
  shared->name = "Nia";
  holder.shared = std::move(shared);

  const caors::serialization::serialization_text_result json = caors::to_json(holder);
  REQUIRE(json.succeeded());

  polymorphic_value_holder decoded{};
  const caors::error status = caors::read_json_from(decoded, json.value);
  REQUIRE(status.succeeded());

  auto* decoded_owner = dynamic_cast<polymorphic_value_player*>(decoded.owner.get());
  REQUIRE(decoded_owner != nullptr);
  CHECK(decoded_owner->level == 31);
  CHECK(decoded_owner->name == "Lin");

  auto* decoded_shared = dynamic_cast<polymorphic_value_player*>(decoded.shared.get());
  REQUIRE(decoded_shared != nullptr);
  CHECK(decoded_shared->level == 37);
  CHECK(decoded_shared->name == "Nia");
}

// 原测试文件：serialization_polymorphic_value_wrapper_polymorphic_value_wrappers_write_type_envelopes_test.cpp

#include "serialization_polymorphic_value_wrapper_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_wrapper;

TEST_CASE("serialization - polymorphic value wrappers write type envelopes", "[serialization]") {
  require_polymorphic_value_metadata();

  polymorphic_value_holder holder{};
  auto owner = std::make_unique<polymorphic_value_player>();
  owner->level = 17;
  owner->name = "Ada";
  holder.owner = std::move(owner);

  auto shared = std::make_shared<polymorphic_value_player>();
  shared->level = 23;
  shared->name = "Mina";
  holder.shared = std::move(shared);

  const caors::serialization::variant_result value = caors::to_value(holder);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::meta::variant_member* owner_member =
      value.value.find_member(caors::string_name{"owner"});
  REQUIRE(owner_member != nullptr);
  REQUIRE(owner_member->value.is_object());

  const caors::meta::variant_member* type_member =
      owner_member->value.find_member(caors::string_name{"$type"});
  REQUIRE(type_member != nullptr);
  REQUIRE(type_member->value.is_string());
  CHECK(std::string_view{type_member->value.as_string().data(),
                         type_member->value.as_string().size()} ==
        std::string_view{player_guid.value.string()});

  const caors::meta::variant_member* stored_value =
      owner_member->value.find_member(caors::string_name{"$value"});
  REQUIRE(stored_value != nullptr);
  REQUIRE(stored_value->value.is_object());
  CHECK(stored_value->value.find_member(caors::string_name{"level"}) != nullptr);
  CHECK(stored_value->value.find_member(caors::string_name{"name"}) != nullptr);
}
