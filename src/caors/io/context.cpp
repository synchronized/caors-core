// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/context.h"
#include "caors/io/detail/error.h"
#include "caors/io/detail/read_operation.h"
namespace caors::io {
io_context::io_context(const vfs& routes, io_context_options options)
    : routes_(&routes), options_(std::move(options)) {}
io_context::io_context(vfs_snapshot snapshot, io_context_options options)
    : snapshot_(std::move(snapshot)), options_(std::move(options)) {}
result<resolved_file> io_context::resolve(path_view location) const {
  if (!routes_)
    return snapshot_.resolve(location);
  auto snapshot = routes_->snapshot();
  if (!snapshot)
    return {snapshot.error(), {}};
  return snapshot->resolve(location);
}
request<file> io_context::open(path_view location, open_options options,
                               std::function<void(const io_result<file>&)> callback) const {
  return detail::submit_request<file>(
      options_.completion, std::move(callback),
      [this, location, options, config = options_](io_completion<file> receipt) mutable {
        auto resolved = resolve(location);
        if (options.access != file_access::read || options.creation != file_creation::open_existing)
          return io_error{io_errc::unsupported};
        if (!resolved)
          return detail::convert_error(resolved.error());
        struct operation {
          io_completion<file> receipt;
          io_request<std::shared_ptr<async_file>> opening;
          using stop_callback = std::stop_callback<std::function<void()>>;
          std::unique_ptr<stop_callback> stop;
        };
        auto op = std::make_shared<operation>();
        auto stop = receipt.stop_token();
        op->receipt = std::move(receipt);
        auto opened = request_open(
            std::move(resolved.value), config.completion,
            [op, config, budget = options.max_read_bytes](const auto& value) {
              auto receipt = std::move(op->receipt);
              if (!value)
                (void)receipt.complete(io_failure<file>(value.error()));
              else
                (void)receipt.complete(
                    {{}, file{value.value, config.completion, config.resource, budget}});
            },
            config.resource);
        if (!opened)
          return opened.error();
        op->opening = std::move(opened.value);
        std::weak_ptr<operation> weak = op;
        op->stop = std::make_unique<operation::stop_callback>(stop, [weak] {
          if (auto operation = weak.lock())
            operation->opening.cancel();
        });
        return io_error{};
      });
}
request<byte_buffer>
io_context::read(path_view location, read_options options,
                 std::function<void(const io_result<byte_buffer>&)> callback) const {
  return detail::submit_request<byte_buffer>(
      options_.completion, std::move(callback),
      [this, location, options, config = options_](io_completion<byte_buffer> receipt) mutable {
        auto resolved = resolve(location);
        if (!resolved)
          return detail::convert_error(resolved.error());
        return detail::start_read_file(std::move(resolved.value), config.completion,
                                       options.max_bytes, config.resource, std::move(receipt));
      });
}
std::uint64_t file::size() const noexcept { return handle_ ? handle_->size() : 0; }
request<byte_buffer> file::read(file_read_options options,
                                std::function<void(const io_result<byte_buffer>&)> callback) const {
  auto started = request_read(handle_, options.offset, options.length, budget_, executor_, callback,
                              resource_);
  return detail::unwrap_request(std::move(started), executor_, std::move(callback));
}
request<close_result>
file::close(std::function<void(const io_result<close_result>&)> callback) const {
  auto started = request_close(handle_, executor_, callback);
  return detail::unwrap_request(std::move(started), executor_, std::move(callback));
}
} // namespace caors::io
