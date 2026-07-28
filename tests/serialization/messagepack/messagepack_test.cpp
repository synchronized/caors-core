// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/messagepack.h"
#include "caors/serialization/operations.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace {

using caors::meta::variant;

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
  variant::array_type fixed_values;
  fixed_values.resize(15);
  const caors::io::byte_buffer_result fixed_array =
      caors::serialization::encode_messagepack(variant::array(std::move(fixed_values)));
  REQUIRE(fixed_array.succeeded());
  CHECK(byte_at(fixed_array.value, 0) == 0x9F);

  variant::array_type array16_values;
  array16_values.resize(16);
  const caors::io::byte_buffer_result array16 =
      caors::serialization::encode_messagepack(variant::array(std::move(array16_values)));
  REQUIRE(array16.succeeded());
  CHECK(byte_at(array16.value, 0) == 0xDC);
  CHECK(byte_at(array16.value, 1) == 0x00);
  CHECK(byte_at(array16.value, 2) == 0x10);

  variant::dictionary_type fixed_entries;
  fixed_entries.resize(15);
  const caors::io::byte_buffer_result fixed_map =
      caors::serialization::encode_messagepack(variant::dictionary(std::move(fixed_entries)));
  REQUIRE(fixed_map.succeeded());
  CHECK(byte_at(fixed_map.value, 0) == 0x8F);

  variant::dictionary_type map16_entries;
  map16_entries.resize(16);
  const caors::io::byte_buffer_result map16 =
      caors::serialization::encode_messagepack(variant::dictionary(std::move(map16_entries)));
  REQUIRE(map16.succeeded());
  CHECK(byte_at(map16.value, 0) == 0xDE);
  CHECK(byte_at(map16.value, 1) == 0x00);
  CHECK(byte_at(map16.value, 2) == 0x10);
}
