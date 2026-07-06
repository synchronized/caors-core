// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/resources.h"

using namespace caors::tests::serialization;

namespace {

constexpr caors::resource_guid texture_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x6c, 0xd2, 0xda, 0x3d, 0xb5, 0x49, 0x47, 0x56, 0x8b, 0x8b, 0x0c, 0x84,
                            0x29, 0x3a, 0xf4, 0x10})};

class serialized_texture_resource : public caors::resource {
public:
  using caors::resource::resource;
};

struct resource_ref_actor {
  caors::resource_ref<serialized_texture_resource> texture{};
};

void require_resource_ref_actor_metadata() {
  REQUIRE(type_result_ok(caors::meta::type_<serialized_texture_resource>(
                             "caors.test.serialization.serialized_texture_resource")
                             .result()));
  REQUIRE(
      type_result_ok(caors::meta::type_<caors::resource_ref<serialized_texture_resource>>(
                         "caors.resource_ref<caors.test.serialization.serialized_texture_resource>")
                         .result()));

  REQUIRE(serialization_result_ok(
      caors::serialization::register_codec<caors::resource_ref<serialized_texture_resource>>()));

  REQUIRE(reflection_result_ok(
      caors::reflection::class_<resource_ref_actor>("caors.test.serialization.resource_ref_actor")
          .field("texture", &resource_ref_actor::texture)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<resource_ref_actor>()));
}

} // namespace

TEST_CASE("serialization - serializes resource refs as stable guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  const resource_ref_actor actor{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor);
  REQUIRE(value.succeeded());
  REQUIRE(value.value.is_object());

  const caors::value::variant_member* texture =
      value.value.find_member(caors::string_name{"texture"});
  REQUIRE(texture != nullptr);
  REQUIRE(texture->value.is_string());
  CHECK(texture->value.as_string() == "6cd2da3d-b549-4756-8b8b-0c84293af410");
}

TEST_CASE("serialization - deserializes resource refs as unresolved guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  resource_ref_actor decoded{};
  const caors::error status =
      caors::deserialize_json(decoded, R"({"texture":"6cd2da3d-b549-4756-8b8b-0c84293af410"})");

  REQUIRE(status.succeeded());
  CHECK(decoded.texture.guid() == texture_guid);
  CHECK_FALSE(decoded.texture.resolved());
  CHECK(decoded.texture.get() == nullptr);

  caors::resource_cache<serialized_texture_resource> cache;
  serialized_texture_resource* texture = cache.create(texture_guid);
  REQUIRE(texture != nullptr);

  REQUIRE(cache.resolve(decoded.texture));
  CHECK(decoded.texture.resolved());
  CHECK(decoded.texture.get() == texture);
}

TEST_CASE("serialization - serializes empty resource refs as null", "[serialization]") {
  require_resource_ref_actor_metadata();

  const resource_ref_actor actor{};
  const caors::serialization::serialization_text_result json = caors::serialize_json(actor);

  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"texture":null})");

  resource_ref_actor decoded{caors::resource_ref<serialized_texture_resource>{texture_guid}};
  const caors::error status = caors::deserialize_json(decoded, R"({"texture":null})");

  REQUIRE(status.succeeded());
  CHECK_FALSE(decoded.texture);
  CHECK(decoded.texture.guid() == caors::resource_guid::nil());
  CHECK_FALSE(decoded.texture.resolved());
}

TEST_CASE("serialization - rejects invalid resource ref guids", "[serialization]") {
  require_resource_ref_actor_metadata();

  resource_ref_actor decoded{};
  const caors::error status = caors::deserialize_json(decoded, R"({"texture":"not-a-guid"})");

  REQUIRE(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(status.value() == caors::errc::invalid_argument);
}
