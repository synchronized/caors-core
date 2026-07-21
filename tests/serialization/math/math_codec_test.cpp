// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/math/registration.h"
#include "caors/math/serialization.h"
#include "caors/serialization/json_reader.h"

#include <catch2/catch_all.hpp>
#include <string_view>

namespace {

struct math_codec_actor {
  caors::math::vec3 position{};
  caors::math::quat rotation{};
  caors::math::rect viewport{};
  caors::math::aabb bounds{};
  caors::math::mat4 transform{};
};

[[nodiscard]] bool contains(std::string_view text, std::string_view fragment) {
  return text.find(fragment) != std::string_view::npos;
}

void require_math_codec_actor_registered() {
  REQUIRE(caors::math::register_meta_types().succeeded());
  REQUIRE(caors::tests::serialization::registration_result_ok(
      caors::registration::type_<math_codec_actor>("caors.test.serialization.math_codec_actor")
          .meta()
          .property("position", &math_codec_actor::position)
          .property("rotation", &math_codec_actor::rotation)
          .property("viewport", &math_codec_actor::viewport)
          .property("bounds", &math_codec_actor::bounds)
          .property("transform", &math_codec_actor::transform)
          .serialization()
          .commit()));
}

} // namespace

TEST_CASE("serialization - math codecs are available without math type registration",
          "[serialization][math]") {
  const caors::serialization::codec vec3_codec =
      caors::serialization::codec_of<caors::math::vec3>();
  REQUIRE(vec3_codec);
  CHECK(vec3_codec.can_write());
  CHECK(vec3_codec.can_read());
  CHECK(vec3_codec.can_make_value());

  const caors::serialization::variant_result encoded =
      caors::serialization::to_variant(caors::any::from(caors::math::vec3{1.0F, 2.0F, 3.0F}));
  REQUIRE(encoded.succeeded());
  REQUIRE(encoded.value.is_array());
  REQUIRE(encoded.value.as_array().size() == 3);
  CHECK(encoded.value.as_array()[0].as_f64() == 1.0);
  CHECK(encoded.value.as_array()[1].as_f64() == 2.0);
  CHECK(encoded.value.as_array()[2].as_f64() == 3.0);
}

TEST_CASE("serialization - math codecs support template scalar aliases", "[serialization][math]") {
  caors::string integer_text;
  caors::serialization::text_writer writer{integer_text};
  REQUIRE(caors::serialization::codec_traits<caors::math::ivec3>::write(writer,
                                                                        caors::math::ivec3{1, 2, 3})
              .succeeded());
  CHECK(integer_text == "[1,2,3]");

  caors::math::ivec3 integer_decoded{};
  caors::serialization::json_reader reader{integer_text};
  REQUIRE(caors::serialization::codec_traits<caors::math::ivec3>::read(reader, integer_decoded)
              .succeeded());
  CHECK(integer_decoded == caors::math::ivec3{1, 2, 3});

  const caors::serialization::variant_result precise =
      caors::serialization::to_variant(caors::any::from(caors::math::dvec2{1.5, 2.25}));
  REQUIRE(precise.succeeded());
  REQUIRE(precise.value.is_array());
  REQUIRE(precise.value.as_array().size() == 2);
  CHECK(precise.value.as_array()[0].as_f64() == 1.5);
  CHECK(precise.value.as_array()[1].as_f64() == 2.25);
}

TEST_CASE("serialization - math codecs roundtrip registered object fields",
          "[serialization][math]") {
  require_math_codec_actor_registered();

  math_codec_actor actor{};
  actor.position = {1.0F, 2.0F, 3.0F};
  actor.rotation = caors::math::quat::identity();
  actor.viewport = {{10.0F, 20.0F}, {800.0F, 600.0F}};
  actor.bounds = {{-1.0F, -2.0F, -3.0F}, {1.0F, 2.0F, 3.0F}};
  actor.transform = caors::math::mat4::identity();

  const caors::serialization::serialization_text_result text = caors::serialization::to_text(actor);
  REQUIRE(text.succeeded());

  const std::string_view output{text.value.data(), text.value.size()};
  CHECK(contains(output, R"("position":[1,2,3])"));
  CHECK(contains(output, R"("rotation":[0,0,0,1])"));
  CHECK(contains(output, R"("viewport":{"position":[10,20],"size":[800,600]})"));
  CHECK(contains(output, R"("bounds":{"min":[-1,-2,-3],"max":[1,2,3]})"));
  CHECK(contains(output, R"("transform":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1])"));

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  const caors::variant* position = value.value.find_member_variant(caors::string_name{"position"});
  REQUIRE(position != nullptr);
  REQUIRE(position->is_array());
  CHECK(position->as_array().size() == 3);

  const caors::variant* viewport = value.value.find_member_variant(caors::string_name{"viewport"});
  REQUIRE(viewport != nullptr);
  REQUIRE(viewport->is_object());
  REQUIRE(viewport->find_member_variant(caors::string_name{"position"}) != nullptr);
  CHECK(viewport->find_member_variant(caors::string_name{"position"})->is_array());

  math_codec_actor decoded{};
  const caors::error result = caors::read_json_from(decoded, output);
  REQUIRE(result.succeeded());
  CHECK(caors::math::nearly_equal(decoded.position, actor.position));
  CHECK(caors::math::nearly_equal(decoded.rotation, actor.rotation));
  CHECK(caors::math::nearly_equal(decoded.viewport, actor.viewport));
  CHECK(caors::math::nearly_equal(decoded.bounds, actor.bounds));
  CHECK(caors::math::nearly_equal(decoded.transform, actor.transform));
}
