// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "caors/serialization/messagepack.h"
#include "caors/serialization/operations.h"
#include "serialization_test_support.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace {

using caors::meta::variant;
using namespace caors::tests::serialization;

[[nodiscard]] std::uint8_t byte_at(const caors::io::byte_buffer& bytes, std::size_t index) {
  return std::to_integer<std::uint8_t>(bytes[index]);
}

} // namespace

TEST_CASE("serialization - MessagePack encodes scalar values with standard markers",
          "[serialization][messagepack]") {
  const caors::io::byte_buffer_result positive =
      caors::serialization::encode_messagepack(variant::from(std::uint64_t{128}));
  REQUIRE(positive.succeeded());
  REQUIRE(positive.value.size() == 2);
  CHECK(byte_at(positive.value, 0) == 0xCC);
  CHECK(byte_at(positive.value, 1) == 0x80);

  const caors::io::byte_buffer_result negative =
      caors::serialization::encode_messagepack(variant::from(std::int64_t{-33}));
  REQUIRE(negative.succeeded());
  REQUIRE(negative.value.size() == 2);
  CHECK(byte_at(negative.value, 0) == 0xD0);
  CHECK(byte_at(negative.value, 1) == 0xDF);

  const caors::io::byte_buffer_result floating =
      caors::serialization::encode_messagepack(variant::from(1.0));
  REQUIRE(floating.succeeded());
  REQUIRE(floating.value.size() == 9);
  CHECK(byte_at(floating.value, 0) == 0xCB);
  CHECK(byte_at(floating.value, 1) == 0x3F);
  CHECK(byte_at(floating.value, 2) == 0xF0);
}

TEST_CASE("serialization - MessagePack DOM round trips nested values",
          "[serialization][messagepack]") {
  variant root = variant::object({});
  REQUIRE(root.add_member(caors::string_name{"enabled"}, 0, variant::from(true)));

  variant values = variant::array({});
  REQUIRE(values.push_back(variant::from(std::int64_t{-7})));
  REQUIRE(values.push_back(variant::from(std::uint64_t{500})));
  REQUIRE(values.push_back(variant::from(std::string_view{"text"})));
  REQUIRE(root.add_member(caors::string_name{"values"}, 0, std::move(values)));

  const caors::io::byte_buffer_result encoded = caors::serialization::encode_messagepack(root);
  REQUIRE(encoded.succeeded());

  const caors::serialization::variant_result decoded =
      caors::serialization::parse_messagepack(encoded.value);
  REQUIRE(decoded.succeeded());
  REQUIRE(decoded.value.is_object());
  const variant* enabled = decoded.value.find_member_variant(caors::string_name{"enabled"});
  REQUIRE(enabled != nullptr);
  CHECK(enabled->is_bool());
  CHECK(enabled->as_bool());
  const variant* decoded_values = decoded.value.find_member_variant(caors::string_name{"values"});
  REQUIRE(decoded_values != nullptr);
  REQUIRE(decoded_values->is_array());
  REQUIRE(decoded_values->as_array().size() == 3);
  CHECK(decoded_values->as_array()[0].as_i64() == -7);
  CHECK(decoded_values->as_array()[1].as_u64() == 500);
  REQUIRE(decoded_values->as_array()[2].is_string());
  CHECK(decoded_values->as_array()[2].as_string() == "text");
}

TEST_CASE("serialization - MessagePack DOM recursively uses the result resource",
          "[serialization][messagepack][memory]") {
  const std::array encoded{std::byte{0x81}, std::byte{0xA4}, std::byte{'n'},  std::byte{'a'},
                           std::byte{'m'},  std::byte{'e'},  std::byte{0xA5}, std::byte{'A'},
                           std::byte{'l'},  std::byte{'i'},  std::byte{'c'},  std::byte{'e'}};
  caors::tracking_resource result_resource{&caors::get_system_memory_resource()};
  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary_resource{64, &temporary_tracking};

  {
    const caors::serialization::variant_result decoded = caors::serialization::parse_messagepack(
        encoded, caors::serialization::messagepack_options{}, &result_resource,
        &temporary_resource);
    REQUIRE(decoded.succeeded());
    CHECK(temporary_resource.active_scope_count() == 0);
    REQUIRE(decoded.value.is_object());
    CHECK(decoded.value.resource() == &result_resource);

    const variant* name = decoded.value.find_member_variant(caors::string_name{"name"});
    REQUIRE(name != nullptr);
    REQUIRE(name->is_string());
    CHECK(name->resource() == &result_resource);
    CHECK(name->as_string() == "Alice");
    CHECK(result_resource.statistics().bytes_in_use > 0);
  }

  CHECK(result_resource.statistics().bytes_in_use == 0);
  temporary_resource.reset();
  CHECK(temporary_tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("serialization - MessagePack rejects truncated unsupported and trailing input",
          "[serialization][messagepack]") {
  const std::array truncated{std::byte{0xDA}, std::byte{0x00}};
  const caors::serialization::variant_result truncated_result =
      caors::serialization::parse_messagepack(truncated);
  CHECK(truncated_result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});

  const std::array binary{std::byte{0xC4}, std::byte{0x00}};
  const caors::serialization::variant_result binary_result =
      caors::serialization::parse_messagepack(binary);
  CHECK(binary_result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::unsupported});

  const std::array trailing{std::byte{0xC0}, std::byte{0xC0}};
  const caors::serialization::variant_result trailing_result =
      caors::serialization::parse_messagepack(trailing);
  CHECK(trailing_result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});
}

TEST_CASE("serialization - MessagePack enforces parser limits", "[serialization][messagepack]") {
  const std::array nested{std::byte{0x91}, std::byte{0x91}, std::byte{0xC0}};
  caors::serialization::messagepack_options depth_options;
  depth_options.max_parse_depth = 1;
  const caors::serialization::variant_result depth_result =
      caors::serialization::parse_messagepack(nested, depth_options);
  CHECK(depth_result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::recursion_limit});

  const std::array text{std::byte{0xA2}, std::byte{'o'}, std::byte{'k'}};
  caors::serialization::messagepack_options string_options;
  string_options.max_string_size = 1;
  const caors::serialization::variant_result string_result =
      caors::serialization::parse_messagepack(text, string_options);
  CHECK(string_result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});
}

TEST_CASE("serialization - MessagePack writer accepts generic writer events",
          "[serialization][messagepack]") {
  variant root = variant::object({});
  REQUIRE(root.add_member(caors::string_name{"id"}, 0, variant::from(std::uint64_t{7})));

  variant values = variant::array({});
  REQUIRE(values.push_back(variant::from(true)));
  REQUIRE(values.push_back(variant::from(std::int64_t{-2})));
  REQUIRE(root.add_member(caors::string_name{"values"}, 0, std::move(values)));

  variant labels = variant::dictionary({});
  REQUIRE(
      labels.add_entry(variant::from(std::uint64_t{3}), variant::from(std::string_view{"three"})));
  REQUIRE(root.add_member(caors::string_name{"labels"}, 0, std::move(labels)));

  caors::serialization::messagepack_writer writer;
  REQUIRE(caors::serialization::write_value_to(writer, root).succeeded());
  REQUIRE_FALSE(writer.bytes().empty());

  const caors::serialization::variant_result decoded =
      caors::serialization::parse_messagepack(writer.bytes());
  REQUIRE(decoded.succeeded());
  const variant* id = decoded.value.find_member_variant(caors::string_name{"id"});
  REQUIRE(id != nullptr);
  CHECK(id->as_u64() == 7);
  const variant* decoded_values = decoded.value.find_member_variant(caors::string_name{"values"});
  REQUIRE(decoded_values != nullptr);
  REQUIRE(decoded_values->is_array());
  CHECK(decoded_values->as_array()[0].as_bool());
  CHECK(decoded_values->as_array()[1].as_i64() == -2);
  const variant* decoded_labels = decoded.value.find_member_variant(caors::string_name{"labels"});
  REQUIRE(decoded_labels != nullptr);
  REQUIRE(decoded_labels->is_dictionary());
  REQUIRE(decoded_labels->as_dictionary().size() == 1);
  CHECK(decoded_labels->as_dictionary()[0].key.as_u64() == 3);
  CHECK(decoded_labels->as_dictionary()[0].value.as_string() == "three");

  writer.reset();
  CHECK(writer.bytes().empty());
  REQUIRE(writer.write_null().succeeded());
  REQUIRE(writer.bytes().size() == 1);
  CHECK(byte_at(writer.bytes(), 0) == 0xC0);
}

TEST_CASE("serialization - MessagePack writer uses and rewinds a temporary stack resource",
          "[serialization][messagepack][memory]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary{64, &tracking};
  caors::io::byte_buffer output;

  {
    caors::serialization::messagepack_writer writer{output, &temporary};
    REQUIRE(writer.begin_array(2).succeeded());
    REQUIRE(writer.begin_element(0).succeeded());
    REQUIRE(writer.write_string("first").succeeded());
    REQUIRE(writer.end_element().succeeded());
    REQUIRE(writer.begin_element(1).succeeded());
    REQUIRE(writer.write_string("second").succeeded());
    REQUIRE(writer.end_element().succeeded());
    REQUIRE(writer.end_array().succeeded());
  }

  CHECK(temporary.active_scope_count() == 0);
  REQUIRE_FALSE(output.empty());
  const caors::serialization::variant_result decoded =
      caors::serialization::parse_messagepack(output);
  REQUIRE(decoded.succeeded());
  REQUIRE(decoded.value.is_array());
  CHECK(decoded.value.as_array().size() == 2);

  temporary.reset();
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("serialization - MessagePack selects string length markers",
          "[serialization][messagepack]") {
  const auto require_marker = [](std::size_t size, std::uint8_t marker) {
    const std::string text(size, 'x');
    const caors::io::byte_buffer_result encoded = caors::serialization::encode_messagepack(
        variant::from(std::string_view{text.data(), text.size()}));
    REQUIRE(encoded.succeeded());
    REQUIRE_FALSE(encoded.value.empty());
    CHECK(byte_at(encoded.value, 0) == marker);
  };

  require_marker(31, 0xBF);
  require_marker(32, 0xD9);
  require_marker(255, 0xD9);
  require_marker(256, 0xDA);
  require_marker(65535, 0xDA);
  require_marker(65536, 0xDB);
}

TEST_CASE("serialization - MessagePack selects container length markers",
          "[serialization][messagepack]") {
  const auto require_array_marker = [](std::size_t size, std::uint8_t marker,
                                       std::span<const std::uint8_t> encoded_size) {
    variant::array_type values;
    values.resize(size);
    const caors::io::byte_buffer_result encoded =
        caors::serialization::encode_messagepack(variant::array(std::move(values)));
    REQUIRE(encoded.succeeded());
    CHECK(byte_at(encoded.value, 0) == marker);
    for (std::size_t index = 0; index < encoded_size.size(); ++index) {
      CHECK(byte_at(encoded.value, index + 1) == encoded_size[index]);
    }
  };

  const auto require_map_marker = [](std::size_t size, std::uint8_t marker,
                                     std::span<const std::uint8_t> encoded_size) {
    variant::dictionary_type entries;
    entries.resize(size);
    const caors::io::byte_buffer_result encoded =
        caors::serialization::encode_messagepack(variant::dictionary(std::move(entries)));
    REQUIRE(encoded.succeeded());
    CHECK(byte_at(encoded.value, 0) == marker);
    for (std::size_t index = 0; index < encoded_size.size(); ++index) {
      CHECK(byte_at(encoded.value, index + 1) == encoded_size[index]);
    }
  };

  const std::array<std::uint8_t, 0> fixed_size{};
  const std::array<std::uint8_t, 2> size_16{{0x00, 0x10}};
  const std::array<std::uint8_t, 2> maximum_size_16{{0xFF, 0xFF}};
  const std::array<std::uint8_t, 4> minimum_size_32{{0x00, 0x01, 0x00, 0x00}};

  require_array_marker(15, 0x9F, fixed_size);
  require_array_marker(16, 0xDC, size_16);
  require_array_marker(65535, 0xDC, maximum_size_16);
  require_array_marker(65536, 0xDD, minimum_size_32);

  require_map_marker(15, 0x8F, fixed_size);
  require_map_marker(16, 0xDE, size_16);
  require_map_marker(65535, 0xDE, maximum_size_16);
  require_map_marker(65536, 0xDF, minimum_size_32);
}

TEST_CASE("serialization - MessagePack typed APIs round trip reflected objects",
          "[serialization][messagepack]") {
  require_reader_actor_metadata();
  const reader_actor source{7, 95.5F, true, "Ada"};

  const caors::io::byte_buffer_result encoded = caors::to_messagepack(source);
  REQUIRE(encoded.succeeded());

  reader_actor decoded{};
  REQUIRE(caors::read_messagepack_from(decoded, encoded.value).succeeded());
  CHECK(decoded.id == source.id);
  CHECK(decoded.health == source.health);
  CHECK(decoded.active == source.active);
  CHECK(decoded.name == source.name);

  const reader_actor previous = decoded;
  const std::array truncated{std::byte{0x81}, std::byte{0xA2}, std::byte{'i'}};
  const caors::error status = caors::read_messagepack_from(decoded, truncated);
  CHECK(status == caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});
  CHECK(decoded.id == previous.id);
  CHECK(decoded.health == previous.health);
  CHECK(decoded.active == previous.active);
  CHECK(decoded.name == previous.name);
}

TEST_CASE("serialization - MessagePack typed APIs preserve context semantics",
          "[serialization][messagepack]") {
  require_serializable_property_type<codec_string_view_actor>(
      "caors.test.serialization.messagepack_string_view_actor", "name",
      &codec_string_view_actor::name);

  const codec_string_view_actor source{"Mina"};
  const caors::io::byte_buffer_result encoded = caors::to_messagepack(source);
  REQUIRE(encoded.succeeded());

  codec_string_view_actor rejected{};
  CHECK(caors::read_messagepack_from(rejected, encoded.value).value() == caors::errc::unsupported);

  caors::serialization::string_storage storage;
  caors::tracking_resource result_tracking{&caors::get_system_memory_resource()};
  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary_resource{64, &temporary_tracking};
  caors::serialization::context context;
  context.state.storage.string_view_storage = &storage;
  context.resolvers.memory.resource = &result_tracking;
  context.resolvers.memory.temporary_resource = &temporary_resource;
  codec_string_view_actor decoded{};
  REQUIRE(caors::read_messagepack_from(context, decoded, encoded.value).succeeded());
  CHECK(decoded.name == "Mina");
  CHECK(result_tracking.statistics().bytes_in_use == 0);
  CHECK(temporary_resource.active_scope_count() == 0);

  temporary_resource.reset();
  CHECK(temporary_tracking.statistics().bytes_in_use == 0);
}
