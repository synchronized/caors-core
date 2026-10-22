// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "read_operation.h"
#include "file_requests.h"

namespace caors::io::detail {
namespace {
struct read_context {
  resolved_file source;
  tasks::executor worker;
  std::uint64_t max_bytes;
  memory_resource* resource;
};
struct read_operation {
  read_context context;
  io_completion<byte_buffer> completion;
  io::io_request<std::shared_ptr<io::async_file>> opening;
  io::io_request<io::byte_buffer> reading;
  io::io_request<io::close_result> closing;
  std::shared_ptr<io::async_file> file;
  io::io_result<io::byte_buffer> result;
};
void finish(const std::shared_ptr<read_operation>& op, io::io_error status = {}) {
  if (status && !op->result.status)
    op->result.status = status;
  auto completion = std::move(op->completion);
  (void)completion.complete(std::move(op->result));
}
void close(const std::shared_ptr<read_operation>& op) {
  auto closed = io::detail::close_file(op->file, op->context.worker,
                                       [op](const auto& value) { finish(op, value.error()); });
  if (!closed)
    finish(op, closed.error());
  else
    op->closing = std::move(closed.value);
}
void begin_read(const std::shared_ptr<read_operation>& op) {
  auto reading = io::detail::read_file_bounded(
      op->file, op->context.max_bytes, op->context.worker,
      [op](const auto& bytes) {
#if CAORS_HAS_EXCEPTIONS
        try {
#endif
          if (op->completion.stop_token().stop_requested()) {
            op->result.status = {io::io_errc::cancelled};
          } else if (!bytes) {
            op->result.status = bytes.error();
          } else {
            auto owned = io::make_byte_buffer(op->context.resource);
            owned.assign(bytes.value.begin(), bytes.value.end());
            op->result.value = std::move(owned);
          }
#if CAORS_HAS_EXCEPTIONS
        } catch (const std::bad_alloc&) {
          op->result.status = {io::io_errc::out_of_memory};
        } catch (...) {
          op->result.status = {io::io_errc::backend_failure};
        }
#endif
        close(op);
      },
      op->context.resource);
  if (!reading) {
    op->result.status = reading.error();
    close(op);
  } else {
    op->reading = std::move(reading.value);
  }
}
} // namespace
io_error start_read_file(resolved_file source, tasks::executor executor, std::uint64_t max_bytes,
                         memory_resource* resource, io_completion<byte_buffer> completion,
                         bool worker_mode) {
  read_context context{std::move(source), std::move(executor), max_bytes, resource};
  if (!context.source.backend || !context.worker)
    return {io::io_errc::invalid_argument};
  auto op = std::make_shared<read_operation>();
  op->context = std::move(context);
  op->completion = std::move(completion);
  auto on_open = [op](const auto& opened) {
    if (!opened) {
      finish(op, opened.error());
      return;
    }
    op->file = opened.value;
    // 取消发生在打开后、通知前时，不再分配读缓冲，但仍关闭已打开文件。
    if (op->completion.stop_token().stop_requested()) {
      op->result.status = {io::io_errc::cancelled};
      close(op);
    } else {
      begin_read(op);
    }
  };
  auto opened =
      !worker_mode
          ? io::detail::open_file(std::move(op->context.source), op->context.worker, on_open,
                                  op->context.resource)
          : io::detail::open_file_on_worker(std::move(op->context.source), op->context.worker,
                                            op->context.worker, on_open, op->context.resource);
  if (!opened)
    return opened.error();
  op->opening = std::move(opened.value);
  return {};
}
} // namespace caors::io::detail
