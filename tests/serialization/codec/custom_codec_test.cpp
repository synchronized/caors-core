// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_custom_codec_specialization_uses_static_capability_test.cpp

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - custom codec specialization uses static capability", "[serialization]") {
  const caors::serialization::codec_capability codec =
      caors::serialization::codec_of<policy_custom_codec_value>();
  REQUIRE(codec);
  CHECK(codec.type() == caors::meta::id_of<policy_custom_codec_value>());
  CHECK(codec.can_write());
  CHECK(codec.can_read());
  CHECK(codec.can_make_value());

  const caors::serialization::codec_capability capability =
      caors::meta::resolve<policy_custom_codec_value>()
          .get_capability<caors::serialization::codec_capability>();
  REQUIRE(capability);
  CHECK(capability.type() == caors::meta::id_of<policy_custom_codec_value>());
  CHECK(capability.can_write());
  CHECK(capability.can_read());
  CHECK(capability.can_make_value());

  policy_custom_codec_value ref_source{6};
  const caors::serialization::codec_view ref_codec =
      capability.bind(caors::any_ref::from(ref_source));
  REQUIRE(ref_codec);
  CHECK(ref_codec.writable());

  const caors::serialization::variant_result ref_value =
      ref_codec.make_value(caors::serialization::context{});
  REQUIRE(ref_value.succeeded());
  REQUIRE(ref_value.value.try_as_i64() != nullptr);
  CHECK(*ref_value.value.try_as_i64() == 106);

  caors::string ref_text;
  caors::serialization::text_writer ref_writer{ref_text};
  CHECK(ref_codec.write(caors::serialization::context{}, ref_writer).succeeded());
  CHECK(ref_text == "106");

  caors::any owned_source = caors::any::from(policy_custom_codec_value{5});
  const caors::serialization::codec_view owned_codec = capability.bind(owned_source.ref());
  REQUIRE(owned_codec);
  const caors::serialization::variant_result capability_value =
      owned_codec.make_value(caors::serialization::context{});
  REQUIRE(capability_value.succeeded());
  REQUIRE(capability_value.value.try_as_i64() != nullptr);
  CHECK(*capability_value.value.try_as_i64() == 105);

  const policy_custom_codec_value readonly_source{8};
  const caors::serialization::codec_view readonly_codec =
      capability.bind(caors::any_ref::from(readonly_source));
  REQUIRE(readonly_codec);
  CHECK_FALSE(readonly_codec.writable());

  policy_custom_codec_value decoded_source{};
  const caors::serialization::codec_view decoded_codec =
      capability.bind(caors::any_ref::from(decoded_source));
  caors::serialization::json_reader decoded_reader{"109"};
  REQUIRE(decoded_codec.read(caors::serialization::context{}, decoded_reader).succeeded());
  CHECK(decoded_source.value == 9);

  caors::serialization::json_reader readonly_reader{"110"};
  CHECK_FALSE(readonly_codec.read(caors::serialization::context{}, readonly_reader).succeeded());

  int wrong_type = 0;
  CHECK_FALSE(capability.bind(caors::any_ref::from(wrong_type)));

  const caors::serialization::codec_view direct =
      caors::any_ref::from(ref_source).get_capability<caors::serialization::codec_capability>();
  REQUIRE(direct);
  CHECK(direct.make_value(caors::serialization::context{}).succeeded());

  CHECK_FALSE(caors::meta::find<policy_custom_codec_value>());
  CHECK(caors::serialization::find_codec(caors::meta::resolve<policy_custom_codec_value>()));

  REQUIRE(registration_result_ok(caors::registration::type_<policy_custom_codec_value>(
                                     "caors.test.serialization.policy_custom_codec_value")
                                     .commit()));
  CHECK(caors::serialization::find_codec(caors::meta::resolve<policy_custom_codec_value>()));
  require_serializable_property_type<policy_custom_codec_actor>(
      "caors.test.serialization.policy_custom_codec_actor", "value",
      &policy_custom_codec_actor::value);

  const policy_custom_codec_actor actor{{42}};
  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":142})");

  const caors::serialization::variant_result value =
      caors::serialization::to_variant(caors::any::from(actor.value));

  REQUIRE(value.succeeded());
  REQUIRE(value.value.try_as_i64() != nullptr);
  CHECK(*value.value.try_as_i64() == 142);

  policy_custom_codec_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, R"({"value":107})");

  REQUIRE(result.succeeded());
  CHECK(decoded.value.value == 7);
}

TEST_CASE("serialization - static codec handles erased values without registration",
          "[serialization]") {
  CHECK(caors::serialization::find_codec(caors::meta::resolve<static_codec_value>()));

  const static_codec_value value{9};
  const caors::serialization::variant_result erased =
      caors::serialization::to_variant(caors::any::from(value));
  REQUIRE(erased.succeeded());
  REQUIRE(erased.value.try_as_i64() != nullptr);
  CHECK(*erased.value.try_as_i64() == 209);

  REQUIRE(registration_result_ok(
      caors::registration::type_<static_codec_value>("caors.test.serialization.static_codec_value")
          .commit()));
  require_serializable_property_type<static_codec_actor>(
      "caors.test.serialization.static_codec_actor", "value", &static_codec_actor::value);

  const static_codec_actor actor{{11}};
  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":211})");

  static_codec_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, R"({"value":207})");

  REQUIRE(result.succeeded());
  CHECK(decoded.value.value == 7);
  CHECK(caors::serialization::find_codec(caors::meta::resolve<static_codec_value>()));
}

TEST_CASE("serialization - codec validates decode requirements before dispatch",
          "[serialization]") {
  const caors::serialization::codec_capability capability =
      caors::serialization::codec_of<borrowed_codec_value>();
  REQUIRE(capability);
  CHECK(capability.can_read());
  CHECK(capability.requirement() == caors::serialization::decode_requirement::borrowed_storage);

  borrowed_codec_value value{};
  const caors::serialization::codec_view view = capability.bind(caors::any_ref::from(value));
  REQUIRE(view);

  caors::serialization::context missing_storage_context;
  CHECK_FALSE(missing_storage_context.satisfies(
      caors::serialization::decode_requirement::borrowed_storage));
  caors::serialization::json_reader rejected_reader{"17"};
  const caors::error rejected = view.read(missing_storage_context, rejected_reader);
  CHECK(rejected == caors::error{caors::error_domain::serialization, caors::errc::unsupported});
  CHECK(value.value == 0);

  caors::serialization::string_storage storage;
  caors::serialization::context context;
  context.state.storage.string_view_storage = &storage;
  CHECK(context.satisfies(caors::serialization::decode_requirement::borrowed_storage));
  CHECK_FALSE(context.satisfies(caors::serialization::decode_requirement::caller_buffer));
  CHECK_FALSE(context.satisfies(caors::serialization::decode_requirement::object_graph));

  caors::serialization::json_reader accepted_reader{"17"};
  REQUIRE(view.read(context, accepted_reader).succeeded());
  CHECK(value.value == 17);

  require_serializable_property_type<borrowed_codec_actor>(
      "caors.test.serialization.borrowed_codec_actor", "value", &borrowed_codec_actor::value);

  borrowed_codec_actor actor{};
  CHECK(caors::read_json_from(actor, R"({"value":23})").value() == caors::errc::unsupported);
  REQUIRE(caors::read_json_from(context, actor, R"({"value":23})").succeeded());
  CHECK(actor.value.value == 23);
}

// 原测试文件：serialization_custom_codec_serialization_writer_features_validate_required_field_identifiers_test.cpp

#include "serialization_custom_codec_test_support.h"

using namespace caors::tests::serialization;
TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(registration_result_ok(caors::registration::type_<numeric_writer_actor>(
                                     "caors.test.serialization.numeric_writer_actor")
                                     .meta()
                                     .property("id", &numeric_writer_actor::id)
                                     .serialization()
                                     .field(caors::serialization::make_field("id"))
                                     .commit()));

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}
