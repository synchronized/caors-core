// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_object_ptr_manager_serialization_object_manager_can_resolve_object_pointers_test.cpp

#include "serialization_object_ptr_manager_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_manager;
TEST_CASE("serialization - object manager can resolve object pointers", "[serialization]") {
  require_object_ptr_actor_metadata();

  caors::objects::manager manager;
  object_ptr_target* target = manager.create<object_ptr_target>();
  REQUIRE(target != nullptr);

  const caors::object_id target_id = target->id();
  const caors::object_ref_resolver resolver = caors::make_object_ref_resolver(manager);
  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{target}};

  const caors::serialization::serialization_text_result json = caors::to_string(context, actor);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"target\":" + std::to_string(target_id.value()) + "}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_actor decoded{};
  const caors::error result = caors::deserialize(context, decoded, json.value);
  REQUIRE(result.succeeded());
  CHECK(decoded.target.get() == target);

  REQUIRE(manager.destroy(target_id));

  object_ptr_actor missing{};
  const caors::error missing_result = caors::deserialize(context, missing, json.value);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}

// 原测试文件：serialization_object_ptr_manager_serialization_object_manager_can_resolve_persistent_object_guid_references_test.cpp

#include "serialization_object_ptr_manager_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_manager;
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

  const caors::object_ref_resolver resolver = caors::make_persistent_object_ref_resolver(manager);
  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{target}};

  const caors::serialization::serialization_text_result json = caors::to_string(context, actor);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"target\":\"" + target_guid_text + "\"}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  const caors::serialization::variant_result value = caors::to_value(context, actor);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_string());
  CHECK(std::string_view{target_member->value.as_string().data(),
                         target_member->value.as_string().size()} ==
        std::string_view{target_guid_text});

  object_ptr_actor decoded{};
  const caors::error result = caors::deserialize(context, decoded, json.value);
  REQUIRE(result.succeeded());
  CHECK(decoded.target.get() == target);

  object_ptr_actor cleared{caors::object_ptr<object_ptr_target>{target}};
  const caors::error null_result = caors::deserialize(context, cleared, R"({"target":null})");
  REQUIRE(null_result.succeeded());
  CHECK(cleared.target.get() == nullptr);

  REQUIRE(manager.destroy(target->id()));

  object_ptr_actor missing{};
  const caors::error missing_result = caors::deserialize(context, missing, json.value);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}
