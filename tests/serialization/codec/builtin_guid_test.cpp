// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_builtin_guid_codec_built_in_codecs_deserialize_guid_and_type_guid_from_strings_test.cpp

#include "serialization_test_support.h"

#include <optional>
#include <string_view>

using namespace caors::tests::serialization;

namespace {

void require_codec_guid_actor_metadata() {
  REQUIRE(registration_result_ok(
      caors::registration::type_<codec_guid_actor>("caors.test.serialization.codec_guid_actor")
          .meta()
          .property("id", &codec_guid_actor::id)
          .property("type", &codec_guid_actor::type)
          .serialization()
          .commit()));
}

} // namespace

TEST_CASE("serialization - built in guid codecs use static capabilities without registration",
          "[serialization]") {
  CHECK_FALSE(caors::meta::find<caors::guid>());
  CHECK_FALSE(caors::meta::find<caors::type_guid>());

  const caors::serialization::codec_capability guid_codec =
      caors::serialization::codec_of<caors::guid>();
  REQUIRE(guid_codec);
  CHECK(guid_codec.type() == caors::meta::id_of<caors::guid>());
  CHECK(guid_codec.can_write());
  CHECK(guid_codec.can_read());
  CHECK(guid_codec.can_make_value());

  const caors::serialization::codec_capability type_guid_codec =
      caors::meta::resolve<caors::type_guid>()
          .get_capability<caors::serialization::codec_capability>();
  REQUIRE(type_guid_codec);
  CHECK(type_guid_codec.type() == caors::meta::id_of<caors::type_guid>());
  CHECK(type_guid_codec.can_write());
  CHECK(type_guid_codec.can_read());
  CHECK(type_guid_codec.can_make_value());

  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());

  const caors::serialization::variant_result value =
      caors::serialization::to_variant(caors::any::from(*id));
  REQUIRE(value.succeeded());
  REQUIRE(value.value.try_as_string() != nullptr);
  CHECK(*value.value.try_as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
}

TEST_CASE("serialization - built in codecs deserialize guid and type guid from strings",
          "[serialization]") {
  require_codec_guid_actor_metadata();

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::string_view{"2f6d1f64-7a3a-4db2-9a2d-123456789abc"}),
  });
  members.push_back(caors::meta::variant_member{
      caors::string_name{"type"},
      0,
      caors::variant::from(std::string_view{"5ed39a97-7b20-4d87-86ac-e62a4a615b10"}),
  });

  codec_guid_actor actor{};
  const caors::error result =
      caors::read_value_from(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id.string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(actor.type.value.string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}

// 原测试文件：serialization_builtin_guid_codec_built_in_codecs_reject_invalid_guid_text_test.cpp

#include "serialization_test_support.h"

#include <optional>
#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - built in codecs reject invalid guid text", "[serialization]") {
  require_codec_guid_actor_metadata();

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"id"},
      0,
      caors::variant::from(std::string_view{"invalid-guid"}),
  });

  codec_guid_actor actor{};
  const caors::error result =
      caors::read_value_from(actor, caors::variant::object(std::move(members)));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_argument);
}

// 原测试文件：serialization_builtin_guid_codec_built_in_codecs_serialize_guid_and_type_guid_as_scalar_strings_test.cpp

#include "serialization_test_support.h"

#include <optional>
#include <string_view>

using namespace caors::tests::serialization;

TEST_CASE("serialization - built in codecs serialize guid and type guid as scalar strings",
          "[serialization]") {
  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  const std::optional<caors::guid> type =
      caors::guid::parse("5ed39a97-7b20-4d87-86ac-e62a4a615b10");
  REQUIRE(id.has_value());
  REQUIRE(type.has_value());

  require_codec_guid_actor_metadata();

  const codec_guid_actor actor{*id, caors::type_guid{*type}};
  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);

  REQUIRE(text.succeeded());
  CHECK(
      text.value ==
      R"({"id":"2f6d1f64-7a3a-4db2-9a2d-123456789abc","type":"5ed39a97-7b20-4d87-86ac-e62a4a615b10"})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.as_object().size() == 2);
  CHECK(value.value.as_object()[0].value.kind() == caors::meta::variant_kind::string);
  CHECK(value.value.as_object()[0].value.as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  CHECK(value.value.as_object()[1].value.kind() == caors::meta::variant_kind::string);
  CHECK(value.value.as_object()[1].value.as_string() == "5ed39a97-7b20-4d87-86ac-e62a4a615b10");
}
