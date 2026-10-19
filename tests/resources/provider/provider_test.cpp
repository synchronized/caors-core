// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：resources_provider_provider_reads_bytes_from_file_source_test.cpp

#include "resources_test_common.h"

#include <array>

using namespace caors::tests::resources;

namespace {

[[nodiscard]] caors::io::byte_buffer make_bytes(std::string_view text,
                                                caors::memory_resource* resource = nullptr) {
  caors::io::byte_buffer bytes = caors::io::make_byte_buffer(resource);
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  bytes.insert(bytes.end(), first, first + text.size());
  return bytes;
}

[[nodiscard]] std::string bytes_to_string(const caors::io::byte_buffer& bytes) {
  return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

class streaming_source final : public caors::resources::source {
public:
  streaming_source() : bytes_(make_bytes("remote-bytes")) {}

  [[nodiscard]] bool exists(caors::io::location_view location) const override {
    return location.string_view() == remote_location;
  }

  [[nodiscard]] caors::io::byte_buffer_result
  read(caors::io::location_view location, caors::memory_resource* resource = nullptr) override {
    ++read_count;
    if (!exists(location)) {
      return caors::fail<caors::io::byte_buffer>(
          {caors::error_domain::resources, caors::errc::not_found});
    }

    caors::io::byte_buffer copy = caors::io::make_byte_buffer(resource);
    copy.insert(copy.end(), bytes_.begin(), bytes_.end());
    return caors::ok(std::move(copy));
  }

  [[nodiscard]] caors::io::input_stream_result
  open_read(caors::io::location_view location,
            caors::memory_resource* resource = nullptr) override {
    ++open_read_count;
    if (!exists(location)) {
      return caors::fail<caors::io::input_stream_ptr>(
          {caors::error_domain::resources, caors::errc::not_found});
    }

    caors::io::byte_buffer copy = caors::io::make_byte_buffer(resource);
    copy.insert(copy.end(), bytes_.begin(), bytes_.end());
    return caors::ok(caors::io::make_input_stream(std::move(copy), resource));
  }

  [[nodiscard]] caors::error write(caors::io::location_view location,
                                   std::span<const std::byte> bytes) override {
    ++write_count;
    if (!exists(location)) {
      return {caors::error_domain::resources, caors::errc::not_found};
    }

    bytes_.assign(bytes.begin(), bytes.end());
    return {};
  }

  [[nodiscard]] caors::io::output_stream_result
  open_write(caors::io::location_view location,
             caors::memory_resource* resource = nullptr) override {
    ++open_write_count;
    return caors::resources::source::open_write(location, resource);
  }

  [[nodiscard]] std::string bytes_text() const { return bytes_to_string(bytes_); }

  static constexpr std::string_view remote_location = "remote://textures/albedo.bin";
  int read_count = 0;
  int open_read_count = 0;
  int write_count = 0;
  int open_write_count = 0;

private:
  caors::io::byte_buffer bytes_;
};

} // namespace

TEST_CASE("resources - provider reads bytes from file source", "[resources]") {
  const std::filesystem::path directory = make_temp_dir();
  const std::filesystem::path file = directory / "texture.txt";
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  REQUIRE(std::filesystem::create_directories(directory, ec));
  REQUIRE_FALSE(ec);

  caors::io::native_file_system native;
  const std::string file_path = path_string(file);
  REQUIRE(native.write_all(file_path, std::string_view{"texture-bytes"}).succeeded());

  caors::resources::file_source source{native};
  caors::resources::provider provider;

  REQUIRE(provider.bind(test_guid_a, source, file_path).succeeded());
  CHECK(provider.size() == 1);
  CHECK(provider.contains(test_guid_a));
  CHECK(provider.exists(test_guid_a));

  const caors::resources::source_location location = provider.find(test_guid_a);
  REQUIRE(location);
  CHECK(location.source == &source);
  CHECK(location.location.string_view() == file_path);

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::io::byte_buffer_result bytes = provider.read(test_guid_a, &tracking);

  REQUIRE(bytes.succeeded());
  CHECK(bytes.value.get_allocator().resource() == &tracking);
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "texture-bytes");

  caors::io::input_stream_result stream = provider.open_read(test_guid_a, &tracking);
  REQUIRE(stream.succeeded());
  REQUIRE(stream.value != nullptr);
  std::array<std::byte, 7> chunk{};
  caors::io::size_result read = stream.value->read(chunk);
  REQUIRE(read.succeeded());
  CHECK(*read == chunk.size());
  CHECK(std::string_view{reinterpret_cast<const char*>(chunk.data()), chunk.size()} == "texture");

  std::filesystem::remove_all(directory, ec);
}

TEST_CASE("resources - provider supports streaming custom sources", "[resources]") {
  streaming_source source;
  caors::resources::provider provider;

  REQUIRE(provider.bind(test_guid_a, source, streaming_source::remote_location).succeeded());

  caors::io::input_stream_result input = provider.open_read(test_guid_a);
  REQUIRE(input.succeeded());
  REQUIRE(input.value != nullptr);

  std::array<std::byte, 12> buffer{};
  caors::io::size_result read = input.value->read(buffer);
  REQUIRE(read.succeeded());
  CHECK(*read == buffer.size());
  CHECK(std::string_view{reinterpret_cast<const char*>(buffer.data()), buffer.size()} ==
        "remote-bytes");
  CHECK(source.open_read_count == 1);
  CHECK(source.read_count == 0);

  caors::io::output_stream_result output = provider.open_write(test_guid_a);
  REQUIRE(output.succeeded());
  REQUIRE(output.value != nullptr);
  REQUIRE(output.value->write_all(std::string_view{"streamed-update"}).succeeded());
  REQUIRE(output.value->close().succeeded());

  CHECK(source.open_write_count == 1);
  CHECK(source.write_count == 1);
  CHECK(source.bytes_text() == "streamed-update");
}

// 原测试文件：resources_provider_provider_reports_invalid_and_missing_resources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - provider reports invalid and missing resources", "[resources]") {
  caors::io::native_file_system native;
  caors::resources::file_source source{native};
  caors::resources::provider provider;

  caors::error invalid_guid = provider.bind(caors::resource_guid::nil(), source, "asset.txt");
  REQUIRE(invalid_guid.failed());
  CHECK(invalid_guid.domain() == caors::error_domain::resources);
  CHECK(invalid_guid.value() == caors::errc::invalid_argument);

  caors::error invalid_write =
      provider.write(caors::resource_guid::nil(), std::string_view{"data"});
  REQUIRE(invalid_write.failed());
  CHECK(invalid_write.domain() == caors::error_domain::resources);
  CHECK(invalid_write.value() == caors::errc::invalid_argument);

  caors::io::input_stream_result invalid_stream = provider.open_read(caors::resource_guid::nil());
  REQUIRE(invalid_stream.failed());
  CHECK(invalid_stream.error().domain() == caors::error_domain::resources);
  CHECK(invalid_stream.error().value() == caors::errc::invalid_argument);

  caors::io::output_stream_result invalid_output = provider.open_write(caors::resource_guid::nil());
  REQUIRE(invalid_output.failed());
  CHECK(invalid_output.error().domain() == caors::error_domain::resources);
  CHECK(invalid_output.error().value() == caors::errc::invalid_argument);

  caors::error invalid_location = provider.bind(test_guid_a, source, "");
  REQUIRE(invalid_location.failed());
  CHECK(invalid_location.domain() == caors::error_domain::resources);
  CHECK(invalid_location.value() == caors::errc::invalid_argument);

  REQUIRE(provider.bind(test_guid_a, source, "missing.txt").succeeded());
  caors::error duplicate = provider.bind(test_guid_a, source, "other.txt");
  REQUIRE(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::resources);
  CHECK(duplicate.value() == caors::errc::already_exists);

  caors::io::byte_buffer_result missing_guid = provider.read(test_guid_b);
  REQUIRE(missing_guid.failed());
  CHECK(missing_guid.status.domain() == caors::error_domain::resources);
  CHECK(missing_guid.status.value() == caors::errc::not_found);

  caors::io::input_stream_result missing_stream = provider.open_read(test_guid_b);
  REQUIRE(missing_stream.failed());
  CHECK(missing_stream.error().domain() == caors::error_domain::resources);
  CHECK(missing_stream.error().value() == caors::errc::not_found);

  caors::io::output_stream_result missing_output = provider.open_write(test_guid_b);
  REQUIRE(missing_output.failed());
  CHECK(missing_output.error().domain() == caors::error_domain::resources);
  CHECK(missing_output.error().value() == caors::errc::not_found);

  caors::error missing_write = provider.write(test_guid_b, std::string_view{"data"});
  REQUIRE(missing_write.failed());
  CHECK(missing_write.domain() == caors::error_domain::resources);
  CHECK(missing_write.value() == caors::errc::not_found);

  caors::io::byte_buffer_result missing_file = provider.read(test_guid_a);
  REQUIRE(missing_file.failed());
  CHECK(missing_file.status.domain() == caors::error_domain::io);
  CHECK(missing_file.status.value() == caors::errc::not_found);
}

// 原测试文件：resources_provider_provider_reports_unsupported_writes_for_read_only_sources_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - provider reports unsupported writes for read only sources", "[resources]") {
  read_only_source source;
  caors::resources::provider provider;

  REQUIRE(provider.bind(test_guid_a, source, "assets/generated.txt").succeeded());

  caors::error status = provider.write(test_guid_a, std::string_view{"generated-bytes"});
  REQUIRE(status.failed());
  CHECK(status.domain() == caors::error_domain::resources);
  CHECK(status.value() == caors::errc::unsupported);

  caors::io::output_stream_result output = provider.open_write(test_guid_a);
  REQUIRE(output.succeeded());
  REQUIRE(output.value != nullptr);
  REQUIRE(output.value->write_all(std::string_view{"generated-bytes"}).succeeded());
  caors::error close_status = output.value->close();
  REQUIRE(close_status.failed());
  CHECK(close_status.domain() == caors::error_domain::resources);
  CHECK(close_status.value() == caors::errc::unsupported);
}

// 原测试文件：resources_provider_provider_writes_bytes_through_file_source_test.cpp

#include "resources_test_common.h"

using namespace caors::tests::resources;

TEST_CASE("resources - provider writes bytes through file source", "[resources]") {
  caors::io::memory_file_system files;
  caors::resources::file_source source{files};
  caors::resources::provider provider;

  REQUIRE(provider.bind(test_guid_a, source, "assets/generated.txt").succeeded());
  REQUIRE(provider.write(test_guid_a, std::string_view{"generated-bytes"}).succeeded());

  caors::io::byte_buffer_result bytes = files.read_all("assets/generated.txt");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "generated-bytes");

  REQUIRE(provider.write(test_guid_a, std::string_view{"updated-bytes"}).succeeded());
  bytes = provider.read(test_guid_a);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "updated-bytes");

  caors::io::output_stream_result output = provider.open_write(test_guid_a);
  REQUIRE(output.succeeded());
  REQUIRE(output.value != nullptr);
  REQUIRE(output.value->write_all(std::string_view{"streamed-"}).succeeded());
  REQUIRE(output.value->write_all(std::string_view{"bytes"}).succeeded());
  REQUIRE(output.value->close().succeeded());

  bytes = provider.read(test_guid_a);
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        "streamed-bytes");
}

TEST_CASE("resources - source snapshot owns strings and root lifetime independently",
          "[resources][vfs]") {
  std::weak_ptr<int> pin;
  std::shared_ptr<const caors::resources::provider_snapshot> snapshot;
  {
    auto lifetime = std::make_shared<int>(42);
    pin = lifetime;
    auto fs = std::make_shared<caors::io::memory_file_system>();
    REQUIRE_FALSE(fs->write_all("data", std::string_view{"owned"}));
    caors::resources::file_source source{fs, lifetime};
    caors::resources::provider provider;
    std::string location = "data";
    REQUIRE_FALSE(provider.bind(test_guid_a, source, location));
    snapshot = provider.snapshot();
    location.assign("changed");
    provider.clear();
  }
  REQUIRE_FALSE(pin.expired());
  auto file = snapshot->resolve(test_guid_a);
  REQUIRE(file);
  snapshot.reset();
  CHECK_FALSE(pin.expired());
  CHECK(file->path.string_view() == "data");
  auto bytes = file->backend->read_all(file->path);
  REQUIRE(bytes);
  CHECK(bytes_to_string(*bytes) == "owned");
  file = caors::fail<caors::io::resolved_file>(
      {caors::error_domain::resources, caors::errc::cancelled});
  CHECK(pin.expired());
}
TEST_CASE("resources - borrowed leaf snapshot fails explicitly while synchronous read remains",
          "[resources][vfs]") {
  caors::io::memory_file_system fs;
  REQUIRE_FALSE(fs.write_all("data", std::string_view{"sync"}));
  caors::resources::file_source source{fs};
  caors::resources::provider provider;
  REQUIRE_FALSE(provider.bind(test_guid_a, source, "data"));
  auto snapshot = provider.snapshot();
  CHECK(snapshot->resolve(test_guid_a).error().value() == caors::errc::unsupported);
  CHECK(provider.read(test_guid_a).succeeded());
}
