// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes object references through resolver", "[serialization]") {
  REQUIRE(type_result_ok(caors::meta::detail::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::detail::register_serialization<object_ref_actor>()));

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  const caors::serialization::context context{0, &resolver};
  const object_ref_actor actor{caors::object_ref<object_ref_target>{&target}};

  const caors::serialization::serialization_text_result text =
      caors::serialize_text(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"target":77})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_u64());
  CHECK(target_member->value.as_u64() == 77);

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"target":77})");
}


TEST_CASE("serialization - deserializes object references through resolver", "[serialization]") {
  REQUIRE(type_result_ok(caors::meta::detail::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::detail::register_serialization<object_ref_actor>()));

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{88}),
  });

  object_ref_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)),
                                                 caors::serialization::context{0, &resolver});

  REQUIRE(result.succeeded());
  CHECK(actor.target.get() == &target);

  object_ref_actor from_json{};
  const caors::error json_result = caors::deserialize_json(
      from_json, R"({"target":88})", caors::serialization::context{0, &resolver});
  REQUIRE(json_result.succeeded());
  CHECK(from_json.target.get() == &target);
}


TEST_CASE("serialization - serializes object references inside vectors", "[serialization]") {
  require_object_ref_vector_actor_metadata();

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  const caors::serialization::context context{0, &resolver};

  object_ref_vector_actor actor{};
  actor.targets.push_back(caors::object_ref<object_ref_target>{&target});
  actor.targets.push_back(caors::object_ref<object_ref_target>{});

  const caors::serialization::serialization_text_result text =
      caors::serialize_json(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"targets":[77,0]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* targets = value.value.find_member(caors::string_name{"targets"});
  REQUIRE(targets != nullptr);
  REQUIRE(targets->value.is_array());
  REQUIRE(targets->value.as_array().size() == 2);
  CHECK(targets->value.as_array()[0].as_u64() == 77);
  CHECK(targets->value.as_array()[1].as_u64() == 0);
}


TEST_CASE("serialization - deserializes object references inside vectors", "[serialization]") {
  require_object_ref_vector_actor_metadata();

  object_ref_target target;
  object_ref_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  object_ref_vector_actor actor{};
  const caors::error result = caors::deserialize_json(actor, R"({"targets":[88,0]})",
                                                      caors::serialization::context{0, &resolver});

  REQUIRE(result.succeeded());
  REQUIRE(actor.targets.size() == 2);
  CHECK(actor.targets[0].get() == &target);
  CHECK(actor.targets[1].get() == nullptr);
}


TEST_CASE("serialization - object reference resolver failures are reported", "[serialization]") {
  REQUIRE(type_result_ok(caors::meta::detail::register_type<caors::object_ref<object_ref_target>>(
      "caors.object_ref<caors.test.serialization.object_ref_target>")));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ref_actor>("caors.test.serialization.object_ref_actor")
          .field("target", &object_ref_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(
      caors::serialization::detail::register_serialization<object_ref_actor>()));

  object_ref_target target;
  const object_ref_actor actor{caors::object_ref<object_ref_target>{&target}};
  const caors::serialization::serialization_text_result missing_resolver =
      caors::serialize_text(actor);
  CHECK(missing_resolver.failed());
  CHECK(missing_resolver.status.value() == caors::errc::invalid_argument);

  object_ref_resolver_data resolver_data{caors::object_id{99}, nullptr};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{99}),
  });

  object_ref_actor decoded{};
  const caors::error missing_object =
      caors::deserialize(decoded, caors::variant::object(std::move(members)),
                         caors::serialization::context{0, &resolver});
  CHECK(missing_object.failed());
  CHECK(missing_object.value() == caors::errc::not_found);

  other_object_ref_target other;
  object_ref_resolver_data wrong_type_data{caors::object_id{100}, &other};
  const caors::serialization::reference_resolver wrong_type_resolver{
      &resolve_object_id,
      &resolve_object,
      &wrong_type_data,
  };

  caors::variant::object_type wrong_type_members;
  wrong_type_members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{100}),
  });

  const caors::error wrong_type =
      caors::deserialize(decoded, caors::variant::object(std::move(wrong_type_members)),
                         caors::serialization::context{0, &wrong_type_resolver});
  CHECK(wrong_type.failed());
  CHECK(wrong_type.value() == caors::errc::invalid_type);
}



