// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/file_system.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace caors::io {
namespace {

class byte_buffer_input_stream final : public input_stream {
public:
  explicit byte_buffer_input_stream(byte_buffer bytes) : bytes_(std::move(bytes)) {}

  [[nodiscard]] size_result read(std::span<std::byte> buffer) override {
    if (buffer.empty() || position_ >= bytes_.size()) {
      return ok(std::size_t{0});
    }

    const std::size_t count = std::min(buffer.size(), bytes_.size() - position_);
    std::memcpy(buffer.data(), bytes_.data() + position_, count);
    position_ += count;
    return ok(count);
  }

  [[nodiscard]] bool eof() const noexcept override { return position_ >= bytes_.size(); }

private:
  byte_buffer bytes_;
  std::size_t position_ = 0;
};

[[nodiscard]] input_stream_ptr make_input_stream(byte_buffer bytes, memory_resource* resource) {
  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* stream = memory::make<byte_buffer_input_stream>(resolved, std::move(bytes));
  input_stream_ptr result{stream};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    memory::destroy(static_cast<byte_buffer_input_stream*>(stored),
                    static_cast<memory_resource*>(user_data));
  });
  return result;
}

} // namespace

file_system::~file_system() = default;

input_stream_result file_system::open_read(path_view path, memory_resource* resource) {
  byte_buffer_result bytes = read_all(path, resource);
  if (bytes.failed()) {
    return fail<input_stream_ptr>(bytes.error());
  }

  return ok(make_input_stream(std::move(bytes.value), resource));
}

output_stream_result file_system::open_write(path_view, memory_resource*) {
  return fail<output_stream_ptr>({error_domain::io, errc::unsupported});
}

} // namespace caors::io
