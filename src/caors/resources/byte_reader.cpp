// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/resources/byte_reader.h"
#include "caors/io/detail/read_operation.h"
namespace caors::resources {
byte_reader::~byte_reader() = default;
io::io_error file_byte_reader::read(read_context context, read_completion completion) {
  return io::detail::start_read_file(std::move(context.source), std::move(context.worker),
                                     context.max_bytes, context.resource, std::move(completion),
                                     mode_ == file_read_mode::worker_adapter);
}
} // namespace caors::resources
