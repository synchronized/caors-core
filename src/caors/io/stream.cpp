// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/stream.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace caors::io {

input_stream::~input_stream() = default;
output_stream::~output_stream() = default;

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

input_stream_ptr make_input_stream(byte_buffer bytes, memory_resource* resource) {
  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* stream = memory::make<byte_buffer_input_stream>(resolved, std::move(bytes));
  input_stream_ptr result{stream};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    memory::destroy(static_cast<byte_buffer_input_stream*>(stored),
                    static_cast<memory_resource*>(user_data));
  });
  return result;
}

} // namespace caors::io
