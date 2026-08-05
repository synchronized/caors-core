// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/stream.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace caors::io {

input_stream::~input_stream() = default;
output_stream::~output_stream() = default;

error output_stream::write_all(std::span<const std::byte> buffer) {
  std::span<const std::byte> remaining = buffer;
  while (!remaining.empty()) {
    size_result written = write(remaining);
    if (written.failed()) {
      return written.error();
    }
    if (*written == 0U) {
      return {error_domain::io, errc::backend_failure};
    }

    remaining = remaining.subspan(*written);
  }

  return {};
}

error output_stream::write_all(std::string_view text) {
  return write_all(std::span<const std::byte>{
      reinterpret_cast<const std::byte*>(text.data()),
      text.size(),
  });
}

error output_stream::flush() { return {}; }

error output_stream::close() { return flush(); }

byte_buffer_input_stream::byte_buffer_input_stream(byte_buffer bytes) : bytes_(std::move(bytes)) {}

size_result byte_buffer_input_stream::read(std::span<std::byte> buffer) {
  if (buffer.empty() || position_ >= bytes_.size()) {
    return ok(std::size_t{0});
  }

  const std::size_t count = std::min(buffer.size(), bytes_.size() - position_);
  std::memcpy(buffer.data(), bytes_.data() + position_, count);
  position_ += count;
  return ok(count);
}

bool byte_buffer_input_stream::eof() const noexcept { return position_ >= bytes_.size(); }

input_stream_ptr make_input_stream(byte_buffer bytes, memory_resource* result_resource) {
  memory_resource* resolved =
      result_resource != nullptr ? result_resource : get_default_memory_resource();
  auto* stream = memory::make<byte_buffer_input_stream>(resolved, std::move(bytes));
  input_stream_ptr result{stream};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    memory::destroy(static_cast<byte_buffer_input_stream*>(stored),
                    static_cast<memory_resource*>(user_data));
  });
  return result;
}

} // namespace caors::io
