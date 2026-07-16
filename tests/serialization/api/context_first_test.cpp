// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include "caors/assets.h"
#include "caors/io.h"
#include "caors/resources.h"

#include <limits>

using namespace caors::tests::serialization;

namespace caors::tests::serialization::api {

struct context_codec_value {
  int value = 0;
};

struct context_codec_actor {
  context_codec_value value{};
};

class context_api_resource : public caors::resource {
public:
  using caors::resource::resource;

  int value = 0;
};

constexpr caors::resource_guid resource_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x5c, 0x7a, 0x41, 0x10, 0x9b, 0x19, 0x4c, 0x27, 0x9a, 0x2a, 0x61, 0x8c,
                            0x6d, 0x9f, 0x5a, 0x20})};

[[nodiscard]] std::int64_t context_offset(const caors::serialization::context& context) noexcept {
  return static_cast<std::int64_t>(context.config.fields.schema_version) * 100;
}

void require_context_codec_metadata() {
  require_registered_type<context_codec_value>("caors.test.serialization.api.context_codec_value");
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<context_codec_actor>(
                                    "caors.test.serialization.api.context_codec_actor"),
                                caors::reflection::class_<context_codec_actor>())
                                   .field("value", &context_codec_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<context_codec_actor>().commit()));
}

void require_context_resource_metadata() {
  REQUIRE(reflection_result_ok(((void)caors::meta::class_<context_api_resource>(
                                    "caors.test.serialization.api.context_api_resource"),
                                caors::reflection::class_<context_api_resource>())
                                   .field("value", &context_api_resource::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<context_api_resource>().commit()));
}

} // namespace caors::tests::serialization::api

namespace caors::serialization {

template <> struct codec_traits<caors::tests::serialization::api::context_codec_value> {
  using value_type = caors::tests::serialization::api::context_codec_value;

  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(const serialization::context& context, writer& writer,
                                   const value_type& value) {
    return writer.write_i64(static_cast<std::int64_t>(value.value) +
                            caors::tests::serialization::api::context_offset(context));
  }

  [[nodiscard]] static error read(const serialization::context& context, reader& reader,
                                  value_type& value) {
    std::int64_t decoded = 0;
    error status = reader.read_i64(decoded);
    if (status) {
      std::uint64_t unsigned_decoded = 0;
      status = reader.read_u64(unsigned_decoded);
      if (status) {
        return status;
      }

      if (unsigned_decoded > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return {error_domain::serialization, errc::invalid_argument};
      }

      decoded = static_cast<std::int64_t>(unsigned_decoded);
    }

    value.value =
        static_cast<int>(decoded - caors::tests::serialization::api::context_offset(context));
    return {};
  }

  [[nodiscard]] static variant_result make_value(const serialization::context& context,
                                                 const value_type& value) {
    return {{},
            variant::from(static_cast<std::int64_t>(value.value) +
                          caors::tests::serialization::api::context_offset(context))};
  }
};

} // namespace caors::serialization

namespace {

TEST_CASE("serialization - public context overloads are context-first", "[serialization]") {
  using namespace caors::tests::serialization::api;

  require_context_codec_metadata();

  caors::serialization::context context{};
  context.config.fields.schema_version = 5;

  const context_codec_actor actor{{7}};

  caors::string json_output;
  REQUIRE(caors::serialization::serialize_json_to(context, actor, json_output).succeeded());
  CHECK(json_output == R"({"value":507})");

  caors::serialization::serialization_text_result json = caors::to_string(context, actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"value":507})");

  caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(context, actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"value":507})");

  caors::serialization::variant_result value = caors::to_value(context, actor);
  REQUIRE(value.succeeded());
  const caors::meta::variant_member* member = value.value.find_member(caors::string_name{"value"});
  REQUIRE(member != nullptr);
  REQUIRE(member->value.try_as_i64() != nullptr);
  CHECK(*member->value.try_as_i64() == 507);

  caors::serialization::json_writer writer;
  REQUIRE(caors::serialize(context, actor, writer).succeeded());
  CHECK(writer.text() == R"({"value":507})");

  context_codec_actor decoded{};
  REQUIRE(caors::deserialize(context, decoded, R"({"value":512})").succeeded());
  CHECK(decoded.value.value == 12);

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"value"},
      0,
      caors::variant::from(std::int64_t{515}),
  });

  context_codec_actor decoded_from_dom{};
  REQUIRE(caors::deserialize(context, decoded_from_dom, caors::variant::object(std::move(members)))
              .succeeded());
  CHECK(decoded_from_dom.value.value == 15);
}

TEST_CASE("resources - json loader and saver accept context first", "[resources]") {
  using namespace caors::tests::serialization::api;

  require_context_resource_metadata();

  caors::io::memory_file_system files;
  caors::resources::file_source source{files};
  caors::resources::provider provider;
  REQUIRE(provider.bind(resource_guid, source, "assets/context-resource.json").succeeded());

  caors::serialization::context context{};
  caors::assets::json_saver<context_api_resource> saver{context, provider};

  context_api_resource value{resource_guid};
  value.value = 42;
  REQUIRE(saver.save(value).succeeded());

  caors::assets::json_loader<context_api_resource> loader{context, provider};
  caors::resource_cache<context_api_resource> cache;
  caors::resources::handle_result<context_api_resource> loaded =
      caors::resources::load_handle_into(cache, loader, resource_guid);

  REQUIRE(loaded.succeeded());
  REQUIRE(loaded.value);
  CHECK(loaded.value->guid() == resource_guid);
  CHECK(loaded.value->value == 42);
}

} // namespace
