// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "../polymorphic/serialization_polymorphic_object_test_support.h"
#include "caors/memory.h"
#include "caors/serialization/messagepack.h"
#include "caors/serialization/operations.h"
#include "serialization_test_support.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <thread>

namespace caors::tests::serialization {

struct nested_messagepack_callback_state {
  std::size_t scope_count_before = 0;
  std::size_t scope_count_after = 0;
  bool inner_succeeded = false;
};

struct nested_messagepack_inner_actor {
  int value = 0;
};

struct nested_messagepack_callback_value {
  int value = 0;
  nested_messagepack_callback_state* state = nullptr;
};

struct nested_messagepack_callback_actor {
  int prefix = 0;
  nested_messagepack_callback_value nested{};
  int suffix = 0;
};

} // namespace caors::tests::serialization

namespace caors::serialization {

template <> struct codec_traits<caors::tests::serialization::nested_messagepack_callback_value> {
  using value_type = caors::tests::serialization::nested_messagepack_callback_value;

  static constexpr bool can_write = true;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = false;

  [[nodiscard]] static caors::error write(const context& context, writer& writer,
                                          const value_type& value) {
    auto* stack = dynamic_cast<caors::stack_resource*>(context.effective_temporary_resource());
    if (value.state != nullptr && stack != nullptr) {
      value.state->scope_count_before = stack->active_scope_count();
    }

    const caors::tests::serialization::nested_messagepack_inner_actor inner_value{value.value};
    const caors::io::byte_buffer_result inner = to_messagepack(context, inner_value);
    if (value.state != nullptr) {
      value.state->inner_succeeded = inner.succeeded();
      if (stack != nullptr) {
        value.state->scope_count_after = stack->active_scope_count();
      }
    }
    if (inner.failed()) {
      return inner.status;
    }

    return writer.write_i64(value.value);
  }
};

} // namespace caors::serialization

namespace {

using caors::meta::variant;
using namespace caors::tests::serialization;

[[nodiscard]] std::uint8_t byte_at(const caors::io::byte_buffer& bytes, std::size_t index) {
  return std::to_integer<std::uint8_t>(bytes[index]);
}

class chunked_input_stream final : public caors::io::input_stream {
public:
  chunked_input_stream(std::span<const std::byte> bytes, std::size_t chunk_size)
      : bytes_(bytes), chunk_size_(chunk_size) {}

  [[nodiscard]] caors::io::size_result read(std::span<std::byte> buffer) override {
    const std::size_t count = std::min({buffer.size(), chunk_size_, bytes_.size() - position_});
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(position_), count, buffer.begin());
    position_ += count;
    return caors::ok(count);
  }

  [[nodiscard]] bool eof() const noexcept override { return position_ == bytes_.size(); }

private:
  std::span<const std::byte> bytes_{};
  std::size_t chunk_size_ = 0;
  std::size_t position_ = 0;
};

class failing_input_stream final : public caors::io::input_stream {
public:
  [[nodiscard]] caors::io::size_result read(std::span<std::byte>) override {
    return caors::fail<std::size_t>({caors::error_domain::io, caors::errc::backend_failure});
  }

  [[nodiscard]] bool eof() const noexcept override { return false; }
};

class chunked_output_stream final : public caors::io::output_stream {
public:
  explicit chunked_output_stream(
      std::size_t chunk_size,
      std::size_t fail_after_writes = std::numeric_limits<std::size_t>::max())
      : chunk_size_(chunk_size), fail_after_writes_(fail_after_writes) {}

  [[nodiscard]] caors::io::size_result write(std::span<const std::byte> buffer) override {
    if (write_count_++ >= fail_after_writes_) {
      return caors::fail<std::size_t>({caors::error_domain::io, caors::errc::backend_failure});
    }
    const std::size_t count = std::min(buffer.size(), chunk_size_);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(count));
    return caors::ok(count);
  }

  caors::io::byte_buffer bytes{};

private:
  std::size_t chunk_size_ = 0;
  std::size_t fail_after_writes_ = 0;
  std::size_t write_count_ = 0;
};

void require_messagepack_schema_actor_metadata() {
  REQUIRE(registration_result_ok(caors::registration::type_<reader_unknown_field_actor>(
                                     "caors.test.serialization.messagepack_schema_actor")
                                     .meta()
                                     .property("id", &reader_unknown_field_actor::id)
                                     .serialization()
                                     .field("id")
                                     .commit()));
}

[[nodiscard]] caors::error
migrate_messagepack_schema_actor(const caors::serialization::schema_migration_request& request,
                                 caors::variant& value, void*) {
  if (request.source_version != 1 || request.target_version != 2) {
    return {caors::error_domain::serialization, caors::errc::unsupported};
  }

  caors::variant::object_type* members = value.try_mutable_object();
  if (members == nullptr) {
    return {};
  }

  const caors::variant* legacy_id = value.find_member_variant(caors::string_name{"legacy_id"});
  if (legacy_id != nullptr && value.find_member_variant(caors::string_name{"id"}) == nullptr) {
    members->push_back({caors::string_name{"id"}, 0, *legacy_id});
  }

  members->erase(std::remove_if(members->begin(), members->end(),
                                [](const caors::meta::variant_member& member) {
                                  return member.name == caors::string_name{"legacy_id"} ||
                                         member.name == caors::string_name{"removed"};
                                }),
                 members->end());
  return {};
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

TEST_CASE("serialization - MessagePack streams handle short reads and writes",
          "[serialization][messagepack][stream]") {
  const variant source = variant::from(std::string_view{"messagepack-stream"});
  const caors::io::byte_buffer_result expected = caors::serialization::encode_messagepack(source);
  REQUIRE(expected.succeeded());

  chunked_output_stream output{2};
  caors::serialization::messagepack_writer writer{output};
  REQUIRE(caors::serialization::write_value_to(writer, source).succeeded());
  CHECK(output.bytes == expected.value);

  chunked_input_stream input{output.bytes, 3};
  const caors::serialization::variant_result decoded =
      caors::serialization::parse_messagepack(input);
  REQUIRE(decoded.succeeded());
  REQUIRE(decoded.value.is_string());
  CHECK(decoded.value.as_string() == "messagepack-stream");
}

TEST_CASE("serialization - MessagePack streams propagate backend failures",
          "[serialization][messagepack][stream]") {
  failing_input_stream input;
  const caors::serialization::variant_result read = caors::serialization::parse_messagepack(input);
  CHECK(read.status == caors::error{caors::error_domain::io, caors::errc::backend_failure});

  chunked_output_stream output{1, 1};
  caors::serialization::messagepack_writer writer{output};
  const caors::error written = writer.write_string("failure");
  CHECK(written == caors::error{caors::error_domain::io, caors::errc::backend_failure});
  CHECK(output.bytes.size() == 1);
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

  const std::array array{std::byte{0x92}, std::byte{0xC0}, std::byte{0xC0}};
  caors::serialization::messagepack_options container_options;
  container_options.max_container_size = 1;
  const caors::serialization::variant_result array_result =
      caors::serialization::parse_messagepack(array, container_options);
  CHECK(array_result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});

  const std::array map{std::byte{0x82}, std::byte{0xC0}, std::byte{0xC0}, std::byte{0xC0},
                       std::byte{0xC0}};
  const caors::serialization::variant_result map_result =
      caors::serialization::parse_messagepack(map, container_options);
  CHECK(map_result.status ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_argument});

  container_options.max_container_size = 2;
  CHECK(caors::serialization::parse_messagepack(array, container_options).succeeded());
  CHECK(caors::serialization::parse_messagepack(map, container_options).succeeded());
}

TEST_CASE("serialization - MessagePack temporary storage remains isolated across threads",
          "[serialization][messagepack][memory][stress]") {
  constexpr std::size_t thread_count = 4;
  constexpr std::size_t iteration_count = 128;
  const std::array encoded{std::byte{0x92}, std::byte{0x01}, std::byte{0x02}};
  std::array<caors::stack_resource*, thread_count> resources{};
  std::array<std::thread, thread_count> threads;
  std::barrier ready{static_cast<std::ptrdiff_t>(thread_count)};
  std::atomic<bool> succeeded{true};

  for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    threads[thread_index] = std::thread{[&, thread_index] {
      resources[thread_index] = &caors::library_temporary_resource();
      for (std::size_t iteration = 0; iteration < iteration_count; ++iteration) {
        const caors::serialization::variant_result decoded =
            caors::serialization::parse_messagepack(encoded);
        if (decoded.failed() || !decoded.value.is_array() || decoded.value.as_array().size() != 2) {
          succeeded.store(false, std::memory_order_relaxed);
          break;
        }
      }
      ready.arrive_and_wait();
    }};
  }

  for (std::thread& thread : threads) {
    thread.join();
  }

  CHECK(succeeded.load(std::memory_order_relaxed));
  for (std::size_t left = 0; left < resources.size(); ++left) {
    REQUIRE(resources[left] != nullptr);
    for (std::size_t right = left + 1; right < resources.size(); ++right) {
      CHECK(resources[left] != resources[right]);
    }
  }
}

TEST_CASE("serialization - MessagePack releases deep map staging after success and failure",
          "[serialization][messagepack][memory][stress]") {
  constexpr std::size_t depth = 64;
  caors::io::byte_buffer encoded;
  encoded.reserve(depth * 3 + 1);
  for (std::size_t index = 0; index < depth; ++index) {
    encoded.push_back(std::byte{0x81});
    encoded.push_back(std::byte{0xA1});
    encoded.push_back(std::byte{'x'});
  }
  encoded.push_back(std::byte{0xC0});

  caors::tracking_resource result_tracking{&caors::get_system_memory_resource()};
  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary{64, &temporary_tracking};
  caors::serialization::messagepack_options options;
  options.max_parse_depth = depth;

  {
    const caors::serialization::variant_result decoded =
        caors::serialization::parse_messagepack(encoded, options, &result_tracking, &temporary);
    REQUIRE(decoded.succeeded());
    CHECK(temporary.active_scope_count() == 0);
    CHECK(temporary.statistics().peak_chunk_count > 1);
  }
  CHECK(result_tracking.statistics().bytes_in_use == 0);

  encoded.pop_back();
  {
    const caors::serialization::variant_result decoded =
        caors::serialization::parse_messagepack(encoded, options, &result_tracking, &temporary);
    CHECK(decoded.failed());
    CHECK(temporary.active_scope_count() == 0);
  }
  CHECK(result_tracking.statistics().bytes_in_use == 0);

  temporary.reset();
  CHECK(temporary_tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("serialization - MessagePack releases temporary storage for large arrays",
          "[serialization][messagepack][memory][stress]") {
  constexpr std::size_t element_count = 4096;
  caors::io::byte_buffer encoded;
  encoded.reserve(element_count + 5);
  encoded.push_back(std::byte{0xDD});
  encoded.push_back(std::byte{0x00});
  encoded.push_back(std::byte{0x00});
  encoded.push_back(std::byte{0x10});
  encoded.push_back(std::byte{0x00});
  encoded.insert(encoded.end(), element_count, std::byte{0x00});

  caors::tracking_resource result_tracking{&caors::get_system_memory_resource()};
  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary{128, &temporary_tracking};

  {
    const caors::serialization::variant_result decoded = caors::serialization::parse_messagepack(
        encoded, caors::serialization::messagepack_options{}, &result_tracking, &temporary);
    REQUIRE(decoded.succeeded());
    REQUIRE(decoded.value.is_array());
    CHECK(decoded.value.as_array().size() == element_count);
    CHECK(temporary.active_scope_count() == 0);
  }
  CHECK(result_tracking.statistics().bytes_in_use == 0);

  temporary.reset();
  CHECK(temporary_tracking.statistics().bytes_in_use == 0);
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

TEST_CASE("serialization - MessagePack owned results use the requested resource",
          "[serialization][messagepack][memory]") {
  caors::tracking_resource result_resource{&caors::get_system_memory_resource()};
  const variant source = variant::from(std::string_view{"resource-owned-messagepack"});

  {
    const caors::io::byte_buffer_result encoded =
        caors::serialization::encode_messagepack(source, &result_resource);
    REQUIRE(encoded.succeeded());
    CHECK(encoded.value.get_allocator().resource() == &result_resource);

    caors::io::byte_buffer stream_bytes{encoded.value.begin(), encoded.value.end(),
                                        caors::io::byte_buffer::allocator_type{&result_resource}};
    caors::io::byte_buffer_input_stream stream{std::move(stream_bytes)};
    const caors::serialization::variant_result decoded = caors::serialization::parse_messagepack(
        stream, caors::serialization::messagepack_options{}, &result_resource);
    REQUIRE(decoded.succeeded());
    CHECK(decoded.value.resource() == &result_resource);
    REQUIRE(decoded.value.is_string());
    CHECK(decoded.value.as_string() == "resource-owned-messagepack");
  }

  CHECK(result_resource.statistics().bytes_in_use == 0);
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
  CHECK(decoded.name == "Mina");
}

TEST_CASE("serialization - nested MessagePack codec preserves outer temporary frames",
          "[serialization][messagepack][memory]") {
  REQUIRE(registration_result_ok(caors::registration::type_<nested_messagepack_inner_actor>(
                                     "caors.test.serialization.nested_messagepack_inner_actor")
                                     .meta()
                                     .property("value", &nested_messagepack_inner_actor::value)
                                     .serialization()
                                     .field("value")
                                     .commit()));
  REQUIRE(registration_result_ok(caors::registration::type_<nested_messagepack_callback_value>(
                                     "caors.test.serialization.nested_messagepack_callback_value")
                                     .commit()));
  REQUIRE(registration_result_ok(caors::registration::type_<nested_messagepack_callback_actor>(
                                     "caors.test.serialization.nested_messagepack_callback_actor")
                                     .meta()
                                     .property("prefix", &nested_messagepack_callback_actor::prefix)
                                     .property("nested", &nested_messagepack_callback_actor::nested)
                                     .property("suffix", &nested_messagepack_callback_actor::suffix)
                                     .serialization()
                                     .field("prefix")
                                     .field("nested")
                                     .field("suffix")
                                     .commit()));

  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary_resource{64, &temporary_tracking};
  caors::serialization::context context;
  context.resolvers.memory.temporary_resource = &temporary_resource;

  nested_messagepack_callback_state callback_state{};
  const nested_messagepack_callback_actor source{
      11, nested_messagepack_callback_value{22, &callback_state}, 33};
  const caors::io::byte_buffer_result encoded = caors::to_messagepack(context, source);

  CAPTURE(encoded.status.domain(), encoded.status.value(), callback_state.inner_succeeded,
          callback_state.scope_count_before, callback_state.scope_count_after);
  REQUIRE(encoded.succeeded());
  CHECK(callback_state.inner_succeeded);
  CHECK(callback_state.scope_count_before == 1);
  CHECK(callback_state.scope_count_after == 1);
  CHECK(temporary_resource.active_scope_count() == 0);
  CHECK(temporary_resource.statistics().mark_count == 2);
  CHECK(temporary_resource.statistics().rewind_count == 2);

  const caors::serialization::variant_result decoded =
      caors::serialization::parse_messagepack(encoded.value);
  REQUIRE(decoded.succeeded());
  REQUIRE(decoded.value.is_object());
  REQUIRE(decoded.value.find_member_variant(caors::string_name{"prefix"}) != nullptr);
  REQUIRE(decoded.value.find_member_variant(caors::string_name{"nested"}) != nullptr);
  REQUIRE(decoded.value.find_member_variant(caors::string_name{"suffix"}) != nullptr);
  CHECK(decoded.value.find_member_variant(caors::string_name{"prefix"})->as_u64() == 11);
  CHECK(decoded.value.find_member_variant(caors::string_name{"nested"})->as_u64() == 22);
  CHECK(decoded.value.find_member_variant(caors::string_name{"suffix"})->as_u64() == 33);

  temporary_resource.reset();
  CHECK(temporary_tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("serialization - MessagePack typed APIs preserve unknown field policies",
          "[serialization][messagepack][schema]") {
  require_messagepack_schema_actor_metadata();

  variant root = variant::object({});
  REQUIRE(root.add_member(caors::string_name{"id"}, 0, variant::from(std::int64_t{7})));
  REQUIRE(root.add_member(caors::string_name{"extra"}, 0, variant::from(std::int64_t{99})));
  const caors::io::byte_buffer_result encoded = caors::serialization::encode_messagepack(root);
  REQUIRE(encoded.succeeded());

  reader_unknown_field_actor ignored{};
  REQUIRE(caors::read_messagepack_from(ignored, encoded.value).succeeded());
  CHECK(ignored.id == 7);

  caors::serialization::context reject_context;
  reject_context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::reject;
  reader_unknown_field_actor rejected{};
  CHECK(caors::read_messagepack_from(reject_context, rejected, encoded.value) ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_field});

  caors::variant::object_type preserved_fields;
  caors::serialization::context preserve_context;
  preserve_context.config.schema.unknown_fields =
      caors::serialization::unknown_field_policy::preserve;
  preserve_context.state.schema.preserved_unknown_fields = &preserved_fields;
  reader_unknown_field_actor preserved{};
  REQUIRE(caors::read_messagepack_from(preserve_context, preserved, encoded.value).succeeded());
  CHECK(preserved.id == 7);
  REQUIRE(preserved_fields.size() == 1);
  CHECK(preserved_fields.front().name == caors::string_name{"extra"});
  CHECK(preserved_fields.front().value.as_u64() == 99);
}

TEST_CASE("serialization - preserved MessagePack fields outlive temporary storage",
          "[serialization][messagepack][schema][memory]") {
  require_messagepack_schema_actor_metadata();

  variant extra = variant::object({});
  REQUIRE(extra.add_member(caors::string_name{"name"}, 0,
                           variant::from(std::string_view{"persistent"})));
  variant root = variant::object({});
  REQUIRE(root.add_member(caors::string_name{"id"}, 0, variant::from(std::int64_t{7})));
  REQUIRE(root.add_member(caors::string_name{"extra"}, 0, std::move(extra)));
  const caors::io::byte_buffer_result encoded = caors::serialization::encode_messagepack(root);
  REQUIRE(encoded.succeeded());

  caors::tracking_resource result_tracking{&caors::get_system_memory_resource()};
  caors::tracking_resource temporary_tracking{&caors::get_system_memory_resource()};
  caors::stack_resource temporary_resource{64, &temporary_tracking};

  {
    caors::variant::object_type preserved_fields{
        caors::variant::object_type::allocator_type{&result_tracking}};
    caors::serialization::context context;
    context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::preserve;
    context.state.schema.preserved_unknown_fields = &preserved_fields;
    context.resolvers.memory.resource = &result_tracking;
    context.resolvers.memory.temporary_resource = &temporary_resource;

    reader_unknown_field_actor decoded{};
    REQUIRE(caors::read_messagepack_from(context, decoded, encoded.value).succeeded());
    REQUIRE(preserved_fields.size() == 1);
    CHECK(result_tracking.statistics().bytes_in_use > 0);
    CHECK(temporary_resource.active_scope_count() == 0);

    temporary_resource.reset();
    CHECK(temporary_tracking.statistics().bytes_in_use == 0);

    const variant* name =
        preserved_fields.front().value.find_member_variant(caors::string_name{"name"});
    REQUIRE(name != nullptr);
    REQUIRE(name->try_as_string() != nullptr);
    CHECK(*name->try_as_string() == "persistent");
  }

  CHECK(result_tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("serialization - MessagePack typed APIs migrate DOM before strict validation",
          "[serialization][messagepack][schema]") {
  require_messagepack_schema_actor_metadata();

  variant root = variant::object({});
  REQUIRE(root.add_member(caors::string_name{"legacy_id"}, 0, variant::from(std::int64_t{42})));
  REQUIRE(root.add_member(caors::string_name{"removed"}, 0, variant::from(std::int64_t{99})));
  const caors::io::byte_buffer_result encoded = caors::serialization::encode_messagepack(root);
  REQUIRE(encoded.succeeded());

  caors::serialization::schema_migration_ops migration{&migrate_messagepack_schema_actor};
  caors::serialization::context context;
  context.config.fields.schema_version = 2;
  context.config.schema.source_version = 1;
  context.config.schema.migration = &migration;
  context.config.schema.unknown_fields = caors::serialization::unknown_field_policy::reject;

  reader_unknown_field_actor migrated{};
  REQUIRE(caors::read_messagepack_from(context, migrated, encoded.value).succeeded());
  CHECK(migrated.id == 42);
}

TEST_CASE("serialization - MessagePack typed APIs round trip nested and standard containers",
          "[serialization][messagepack][typed]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<nested_stats>("caors.test.serialization.nested_stats")
          .meta()
          .property("level", &nested_stats::level)
          .property("score", &nested_stats::score)
          .serialization()
          .commit()));
  REQUIRE(registration_result_ok(
      caors::registration::type_<nested_actor>("caors.test.serialization.nested_actor")
          .meta()
          .property("id", &nested_actor::id)
          .property("stats", &nested_actor::stats)
          .serialization()
          .commit()));
  require_serializable_property_type<vector_actor>("caors.test.serialization.vector_actor",
                                                   "scores", &vector_actor::scores);
  require_serializable_property_type<tuple_actor>("caors.test.serialization.tuple_actor", "entry",
                                                  &tuple_actor::entry);
  require_serializable_property_type<variant_actor>("caors.test.serialization.variant_actor",
                                                    "entry", &variant_actor::entry);

  const nested_actor nested_source{7, nested_stats{3, 12.5F}};
  const caors::io::byte_buffer_result nested_bytes = caors::to_messagepack(nested_source);
  REQUIRE(nested_bytes.succeeded());
  nested_actor nested_decoded{};
  REQUIRE(caors::read_messagepack_from(nested_decoded, nested_bytes.value).succeeded());
  CHECK(nested_decoded.id == 7);
  CHECK(nested_decoded.stats.level == 3);
  CHECK(nested_decoded.stats.score == 12.5F);

  const vector_actor vector_source{{1, 2, 3}};
  const caors::io::byte_buffer_result vector_bytes = caors::to_messagepack(vector_source);
  REQUIRE(vector_bytes.succeeded());
  vector_actor vector_decoded{};
  REQUIRE(caors::read_messagepack_from(vector_decoded, vector_bytes.value).succeeded());
  CHECK(vector_decoded.scores == caors::vector<int>{1, 2, 3});

  const tuple_actor tuple_source{{9, caors::string{"nine"}, 2.5F}};
  const caors::io::byte_buffer_result tuple_bytes = caors::to_messagepack(tuple_source);
  REQUIRE(tuple_bytes.succeeded());
  tuple_actor tuple_decoded{};
  REQUIRE(caors::read_messagepack_from(tuple_decoded, tuple_bytes.value).succeeded());
  CHECK(std::get<0>(tuple_decoded.entry) == 9);
  CHECK(std::get<1>(tuple_decoded.entry) == "nine");
  CHECK(std::get<2>(tuple_decoded.entry) == 2.5F);

  variant_actor variant_source{};
  variant_source.entry = caors::string{"selected"};
  const caors::io::byte_buffer_result variant_bytes = caors::to_messagepack(variant_source);
  REQUIRE(variant_bytes.succeeded());
  variant_actor variant_decoded{};
  REQUIRE(caors::read_messagepack_from(variant_decoded, variant_bytes.value).succeeded());
  REQUIRE(std::holds_alternative<caors::string>(variant_decoded.entry));
  CHECK(std::get<caors::string>(variant_decoded.entry) == "selected");
}

TEST_CASE("serialization - MessagePack typed APIs preserve polymorphic envelopes",
          "[serialization][messagepack][polymorphic]") {
  using namespace caors::tests::serialization::polymorphic_object;
  require_metadata();

  player source{};
  source.level = 17;
  source.name = "Mina";
  const actor& source_base = source;

  caors::serialization::context context;
  const caors::io::byte_buffer_result encoded = caors::to_messagepack(context, source_base);
  REQUIRE(encoded.succeeded());

  player decoded{};
  actor& decoded_base = decoded;
  REQUIRE(caors::read_messagepack_from(context, decoded_base, encoded.value).succeeded());
  CHECK(decoded.type_id() == player::static_type_id());
  CHECK(decoded.level == 17);
  CHECK(decoded.name == "Mina");
}

TEST_CASE("serialization - MessagePack typed APIs preserve external references",
          "[serialization][messagepack][external-ref]") {
  require_serializable_property_type<object_ptr_actor>("caors.test.serialization.object_ptr_actor",
                                                       "target", &object_ptr_actor::target);

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::objects::object_ref_resolver resolver = make_test_object_ref_resolver(resolver_data);
  caors::serialization::context context;
  context.resolvers.external_refs.resolver = &resolver;

  const object_ptr_actor source{caors::object_ptr<object_ptr_target>{&target}};
  const caors::io::byte_buffer_result encoded = caors::to_messagepack(context, source);
  REQUIRE(encoded.succeeded());

  object_ptr_actor decoded{};
  REQUIRE(caors::read_messagepack_from(context, decoded, encoded.value).succeeded());
  CHECK(decoded.target.get() == &target);
}

TEST_CASE("serialization - MessagePack typed read failures stop at the failing field",
          "[serialization][messagepack][transaction]") {
  require_reader_actor_metadata();

  variant root = variant::object({});
  REQUIRE(root.add_member(caors::string_name{"id"}, 0, variant::from(std::int64_t{99})));
  REQUIRE(
      root.add_member(caors::string_name{"health"}, 0, variant::from(std::string_view{"invalid"})));
  REQUIRE(root.add_member(caors::string_name{"active"}, 0, variant::from(false)));
  REQUIRE(
      root.add_member(caors::string_name{"name"}, 0, variant::from(std::string_view{"changed"})));
  const caors::io::byte_buffer_result encoded = caors::serialization::encode_messagepack(root);
  REQUIRE(encoded.succeeded());

  reader_actor target{7, 95.5F, true, "original"};
  const caors::error status = caors::read_messagepack_from(target, encoded.value);
  REQUIRE(status.failed());
  CHECK(status.domain() == caors::error_domain::serialization);
  CHECK(target.id == 99);
  CHECK(target.health == 95.5F);
  CHECK(target.active);
  CHECK(target.name == "original");
}
