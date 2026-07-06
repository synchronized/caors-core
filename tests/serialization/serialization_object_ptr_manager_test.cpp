// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string>
#include <string_view>

using namespace caors::tests::serialization;

namespace {

void require_object_ptr_actor_metadata() {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));
}

} // namespace

TEST_CASE("serialization - object manager can resolve object pointers", "[serialization]") {
  require_object_ptr_actor_metadata();

  caors::objects::manager manager;
  object_ptr_target* target = manager.create<object_ptr_target>();
  REQUIRE(target != nullptr);

  const caors::object_id target_id = target->id();
  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{target}};

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"target\":" + std::to_string(target_id.value()) + "}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.target.get() == target);

  REQUIRE(manager.destroy(target_id));

  object_ptr_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - object manager can resolve persistent object guid references",
          "[serialization]") {
  require_object_ptr_actor_metadata();

  const caors::object_guid target_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x60, 0x13, 0x54, 0x9d, 0x91, 0xf7, 0x40, 0x9b, 0xb9, 0x2e, 0x10,
                              0x3d, 0xf4, 0x2c, 0x0c, 0x6a})};
  const std::string target_guid_text = target_guid.string();

  caors::objects::object_index index;
  caors::objects::manager manager(index);
  object_ptr_target* target = manager.create_with_guid<object_ptr_target>(target_guid);
  REQUIRE(target != nullptr);

  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_persistent_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{target}};

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"target\":\"" + target_guid_text + "\"}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_string());
  CHECK(std::string_view{target_member->value.as_string().data(),
                         target_member->value.as_string().size()} ==
        std::string_view{target_guid_text});

  object_ptr_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.target.get() == target);

  object_ptr_actor cleared{caors::object_ptr<object_ptr_target>{target}};
  const caors::error null_result = caors::deserialize_json(cleared, R"({"target":null})", context);
  REQUIRE(null_result.succeeded());
  CHECK(cleared.target.get() == nullptr);

  REQUIRE(manager.destroy(target->id()));

  object_ptr_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}
