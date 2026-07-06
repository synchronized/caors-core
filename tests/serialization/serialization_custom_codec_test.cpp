// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <limits>

using namespace caors::tests::serialization;

namespace caors::tests::serialization {

struct policy_custom_codec_value {
  int value = 0;
};

struct policy_custom_codec_actor {
  policy_custom_codec_value value{};
};

} // namespace caors::tests::serialization

namespace caors::serialization {

template <> struct codec<caors::tests::serialization::policy_custom_codec_value> {
  using value_type = caors::tests::serialization::policy_custom_codec_value;

  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static caors::error write(writer& writer, const value_type& value) {
    return writer.write_i64(static_cast<std::int64_t>(value.value + 100));
  }

  [[nodiscard]] static caors::error read(reader& reader, value_type& value) {
    std::int64_t decoded = 0;
    caors::error status = reader.read_i64(decoded);
    if (status) {
      std::uint64_t unsigned_decoded = 0;
      status = reader.read_u64(unsigned_decoded);
      if (status) {
        return status;
      }

      if (unsigned_decoded > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return {caors::error_domain::serialization, caors::errc::invalid_argument};
      }

      decoded = static_cast<std::int64_t>(unsigned_decoded);
    }

    value.value = static_cast<int>(decoded - 100);
    return {};
  }

  [[nodiscard]] static caors::serialization::variant_result make_value(const value_type& value) {
    return {{}, caors::variant::from(static_cast<std::int64_t>(value.value + 100))};
  }
};

} // namespace caors::serialization

TEST_CASE("serialization - custom codec specialization registers one runtime table",
          "[serialization]") {
  require_registered_type<policy_custom_codec_value>(
      "caors.test.serialization.policy_custom_codec_value");
  REQUIRE(reflection_result_ok(caors::reflection::class_<policy_custom_codec_actor>(
                                   "caors.test.serialization.policy_custom_codec_actor")
                                   .field("value", &policy_custom_codec_actor::value)
                                   .commit()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_codec<policy_custom_codec_value>()));
  REQUIRE(
      serialization_result_ok(caors::serialization::register_type<policy_custom_codec_actor>()));

  const policy_custom_codec_actor actor{{42}};
  const caors::serialization::serialization_text_result text = caors::serialize_text(actor);

  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":142})");

  const caors::serialization::variant_result value =
      caors::serialization::to_variant(caors::any::from(actor.value));

  REQUIRE(value.succeeded());
  REQUIRE(value.value.try_as_i64() != nullptr);
  CHECK(*value.value.try_as_i64() == 142);

  policy_custom_codec_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, R"({"value":107})");

  REQUIRE(result.succeeded());
  CHECK(decoded.value.value == 7);
}

TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(caors::serialization::register_type<numeric_writer_actor>(
              {
                  caors::serialization::make_field("id"),
              })
              .succeeded());

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}
