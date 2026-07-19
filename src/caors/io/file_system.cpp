// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/file_system.h"

#include <utility>

namespace caors::io {
namespace {

class buffered_file_output_stream final : public output_stream {
public:
  buffered_file_output_stream(file_system& target, path_view path, memory_resource* resource)
      : target_(&target), path_(path, resource), buffer_(make_byte_buffer(resource)) {}

  [[nodiscard]] size_result write(std::span<const std::byte> buffer) override {
    if (closed_) {
      return fail<std::size_t>({error_domain::io, errc::invalid_argument});
    }

    buffer_.insert(buffer_.end(), buffer.begin(), buffer.end());
    return ok(buffer.size());
  }

  [[nodiscard]] error close() override {
    if (closed_) {
      return {};
    }

    error status = target_->write_all(path_, buffer_);
    if (status) {
      return status;
    }

    closed_ = true;
    return {};
  }

private:
  file_system* target_ = nullptr;
  path path_;
  byte_buffer buffer_;
  bool closed_ = false;
};

} // namespace

file_system::~file_system() = default;

file_status_result file_system::status(path_view) const {
  return fail<file_status>({error_domain::io, errc::unsupported});
}

directory_entry_list_result file_system::list(path_view, memory_resource*) {
  return fail<directory_entry_list>({error_domain::io, errc::unsupported});
}

input_stream_result file_system::open_read(path_view path, memory_resource* resource) {
  byte_buffer_result bytes = read_all(path, resource);
  if (bytes.failed()) {
    return fail<input_stream_ptr>(bytes.error());
  }

  return ok(make_input_stream(std::move(bytes.value), resource));
}

output_stream_result file_system::open_write(path_view path, memory_resource* resource) {
  if (path.empty()) {
    return fail<output_stream_ptr>({error_domain::io, errc::invalid_argument});
  }

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* stream = memory::make<buffered_file_output_stream>(resolved, *this, path, resolved);
  output_stream_ptr result{stream};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    memory::destroy(static_cast<buffered_file_output_stream*>(stored),
                    static_cast<memory_resource*>(user_data));
  });
  return ok(std::move(result));
}

} // namespace caors::io
