// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/file_system.h"

#include <utility>

namespace caors::io {

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
