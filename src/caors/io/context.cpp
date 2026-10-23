// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/context.h"
#include "caors/io/detail/error.h"
#include "caors/io/location.h"
#include "file_requests.h"
#include "read_operation.h"
#include "vfs_operations.h"
namespace caors::io {
io_context::io_context(const vfs& routes, io_context_options options)
    : routes_(&routes), options_(std::move(options)) {}
io_context::io_context(vfs_snapshot snapshot, io_context_options options)
    : snapshot_(std::move(snapshot)), options_(std::move(options)) {}
result<backend_location> io_context::resolve(path_view location) const {
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
        auto opened = detail::open_file(
            std::move(resolved.value), config.completion,
            [op, config, budget = options.max_read_bytes, write_budget = options.max_write_bytes,
             access = options.access](const auto& value) {
              auto receipt = std::move(op->receipt);
              if (!value)
                (void)receipt.complete(io_failure<file>(value.error()));
              else
                (void)receipt.complete({{},
                                        file{value.value, config.completion, config.resource,
                                             budget, write_budget, access}});
            },
            config.resource, options);
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
request<std::uint64_t>
io_context::write(path_view location, byte_buffer bytes,
                  std::function<void(const io_result<std::uint64_t>&)> callback) const {
  const auto expected = bytes.size();
  auto lifetime = std::make_shared<std::shared_ptr<backend>>();
  return detail::submit_request<std::uint64_t>(
      options_.completion, std::move(callback),
      [this, location, lifetime,
       bytes = std::move(bytes)](io_completion<std::uint64_t> receipt) mutable {
        auto resolved = resolve(location);
        if (!resolved)
          return detail::convert_error(resolved.error());
        *lifetime = resolved->backend;
        return detail::start_write(std::move(resolved.value), std::move(bytes), std::move(receipt));
      },
      [expected](io_result<std::uint64_t>& value) {
        if (value && value.value != expected)
          value = io_failure<std::uint64_t>({io_errc::backend_failure});
      },
      lifetime);
}
request<file_status>
io_context::stat(path_view location,
                 std::function<void(const io_result<file_status>&)> callback) const {
  auto lifetime = std::make_shared<std::shared_ptr<backend>>();
  return detail::submit_request<file_status>(
      options_.completion, std::move(callback),
      [this, location, lifetime](io_completion<file_status> receipt) {
        auto resolved = resolve(location);
        if (!resolved)
          return detail::convert_error(resolved.error());
        *lifetime = resolved->backend;
        return detail::start_stat(std::move(resolved.value), std::move(receipt));
      },
      {}, lifetime);
}
request<directory_entry_list>
io_context::list(path_view location,
                 std::function<void(const io_result<directory_entry_list>&)> callback) const {
  auto lifetime = std::make_shared<std::shared_ptr<backend>>();
  auto directory = path{location, options_.resource};
  return detail::submit_request<directory_entry_list>(
      options_.completion, std::move(callback),
      [this, location, lifetime](io_completion<directory_entry_list> receipt) {
        auto resolved = resolve(location);
        if (!resolved)
          return detail::convert_error(resolved.error());
        *lifetime = resolved->backend;
        return detail::start_list(std::move(resolved.value), options_.resource, std::move(receipt));
      },
      [directory = std::move(directory),
       resource = options_.resource](io_result<directory_entry_list>& value) {
        if (!value)
          return;
        const auto base = normalize_location(location_view{directory.view()}, resource);
        const uri_view parsed{base.string_view()};
        for (auto& entry : value.value) {
          const auto name = entry.name.string_view();
          if (name.empty() || name == "." || name == ".." ||
              name.find_first_of("/\\") != std::string_view::npos) {
            value.status = {io_errc::backend_failure};
            value.value.clear();
            return;
          }
          // 子项名称是文件名，即使带冒号也不能当成新 URI 覆盖所属目录。
          auto joined = join_path(path_view{parsed.path()}, entry.name.view(), resource);
          auto logical =
              detail::make_location_from_parts(parsed.scheme(), detail::uses_authority_form(parsed),
                                               parsed.authority(), joined, resource);
          entry.path = path{logical.string_view(), resource};
        }
      },
      lifetime);
}
std::uint64_t file::size() const noexcept { return handle_ ? handle_->size() : 0; }
request<byte_buffer> file::read(file_read_options options,
                                std::function<void(const io_result<byte_buffer>&)> callback) const {
  if (handle_ && access_ == file_access::write)
    return detail::unwrap_request(io_failure<io_request<byte_buffer>>({io_errc::permission_denied}),
                                  executor_, std::move(callback));
  auto started = detail::read_file(handle_, options.offset, options.length, budget_, executor_,
                                   callback, resource_);
  return detail::unwrap_request(std::move(started), executor_, std::move(callback));
}
request<std::uint64_t>
file::write(file_write_options options, byte_buffer bytes,
            std::function<void(const io_result<std::uint64_t>&)> callback) const {
  const auto length = bytes.size();
  return detail::submit_request<std::uint64_t>(
      executor_, std::move(callback),
      [handle = handle_, options, access = access_, budget = write_budget_,
       bytes = std::move(bytes)](auto receipt) mutable {
        if (!handle)
          return io_error{io_errc::invalid_argument};
        if (access == file_access::read)
          return io_error{io_errc::permission_denied};
        if (bytes.size() > budget)
          return io_error{io_errc::limit_exceeded};
        if (bytes.size() > (std::numeric_limits<std::uint64_t>::max)() - options.offset)
          return io_error{io_errc::out_of_range};
        return handle->start_write(options.offset, std::move(bytes), std::move(receipt));
      },
      [length](io_result<std::uint64_t>& value) {
        if (value && value.value != length)
          value = io_failure<std::uint64_t>({io_errc::backend_failure});
      },
      handle_);
}
request<close_result>
file::close(std::function<void(const io_result<close_result>&)> callback) const {
  auto started = detail::close_file(handle_, executor_, callback);
  return detail::unwrap_request(std::move(started), executor_, std::move(callback));
}
} // namespace caors::io
