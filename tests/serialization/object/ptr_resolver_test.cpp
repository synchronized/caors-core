// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_object_ptr_resolver_serialization_deserializes_object_pointers_through_resolver_test.cpp

#include "serialization_object_ptr_resolver_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_resolver;
TEST_CASE("serialization - deserializes object pointers through resolver", "[serialization]") {
  require_object_ptr_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::object_ref_resolver resolver = make_test_object_ref_resolver(resolver_data);
  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{88}),
  });

  object_ptr_actor actor{};
  const caors::error result =
      caors::deserialize(context, actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.target.get() == &target);

  object_ptr_actor from_json{};
  const caors::error json_result = caors::deserialize(context, from_json, R"({"target":88})");
  REQUIRE(json_result.succeeded());
  CHECK(from_json.target.get() == &target);
}

// 原测试文件：serialization_object_ptr_resolver_serialization_object_pointer_resolver_failures_are_reported_test.cpp

#include "serialization_object_ptr_resolver_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_resolver;
TEST_CASE("serialization - object pointer resolver failures are reported", "[serialization]") {
  require_object_ptr_actor_metadata();

  object_ptr_target target;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{&target}};
  const caors::serialization::serialization_text_result missing_resolver =
      caors::serialization::serialize_text(actor);
  CHECK(missing_resolver.failed());
  CHECK(missing_resolver.status.value() == caors::errc::invalid_argument);

  object_ptr_resolver_data resolver_data{caors::object_id{99}, nullptr};
  const caors::object_ref_resolver resolver = make_test_object_ref_resolver(resolver_data);
  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{99}),
  });

  object_ptr_actor decoded{};
  const caors::error missing_object =
      caors::deserialize(context, decoded, caors::variant::object(std::move(members)));
  CHECK(missing_object.failed());
  CHECK(missing_object.value() == caors::errc::not_found);

  other_object_ptr_target other;
  object_ptr_resolver_data wrong_type_data{caors::object_id{100}, &other};
  const caors::object_ref_resolver wrong_type_resolver =
      make_test_object_ref_resolver(wrong_type_data);
  caors::serialization::context wrong_type_context{};
  wrong_type_context.resolvers.external_refs.resolver = &wrong_type_resolver;

  caors::variant::object_type wrong_type_members;
  wrong_type_members.push_back(caors::meta::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{100}),
  });

  const caors::error wrong_type = caors::deserialize(
      wrong_type_context, decoded, caors::variant::object(std::move(wrong_type_members)));
  CHECK(wrong_type.failed());
  CHECK(wrong_type.value() == caors::errc::invalid_type);
}

// 原测试文件：serialization_object_ptr_resolver_serialization_serializes_object_pointers_through_resolver_test.cpp

#include "serialization_object_ptr_resolver_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_resolver;
TEST_CASE("serialization - serializes object pointers through resolver", "[serialization]") {
  require_object_ptr_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::object_ref_resolver resolver = make_test_object_ref_resolver(resolver_data);
  caors::serialization::context context{};
  context.resolvers.external_refs.resolver = &resolver;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{&target}};

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(context, actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"target":77})");

  const caors::serialization::variant_result value = caors::to_value(context, actor);
  REQUIRE(value.succeeded());
  const caors::meta::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_u64());
  CHECK(target_member->value.as_u64() == 77);

  const caors::serialization::serialization_text_result json = caors::to_string(context, actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"target":77})");
}
