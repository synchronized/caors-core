// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/async_file.h"
#include "caors/io/file_system.h"
#include "file_requests.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

namespace caors::io {
namespace {
class pinned_file final : public async_file {
public:
  pinned_file(std::shared_ptr<void> owner, std::shared_ptr<async_file> target,
              std::uint64_t source_id)
      : owner_(std::move(owner)), target_(std::move(target)), source_id_(source_id) {}
  std::uint64_t size() const noexcept override { return target_->size(); }
  std::uint64_t source_id() const noexcept override { return source_id_; }
  std::uint64_t max_read_size() const noexcept override { return target_->max_read_size(); }
  io_error start_read(std::uint64_t offset, std::uint64_t length, memory_resource* resource,
                      io_completion<byte_buffer> completion) override {
    return target_->start_read(offset, length, resource, std::move(completion));
  }
  io_error start_write(std::uint64_t offset, byte_buffer bytes,
                       io_completion<std::uint64_t> completion) override {
    return target_->start_write(offset, std::move(bytes), std::move(completion));
  }
  io_error start_close(io_completion<close_result> completion) override {
    return target_->start_close(std::move(completion));
  }

private:
  std::shared_ptr<void> owner_;
  std::shared_ptr<async_file> target_;
  const std::uint64_t source_id_;
};

class worker_file final : public async_file, public std::enable_shared_from_this<worker_file> {
public:
  worker_file(std::shared_ptr<random_access_reader> reader, tasks::executor worker,
              std::shared_ptr<void> lifetime)
      : lifetime_(std::move(lifetime)), reader_(std::move(reader)), worker_(std::move(worker)),
        size_(reader_->size()) {}
  ~worker_file() override {
    if (close_attempted_)
      return;
    struct cleanup_resource {
      std::shared_ptr<void> lifetime;
      std::shared_ptr<random_access_reader> reader;
    };
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      auto cleanup = std::make_shared<cleanup_resource>(cleanup_resource{lifetime_, reader_});
      // 无 this 捕获，不等待队列；拒绝/丢弃时由 reader 的 RAII 析构兜底。
      (void)worker_.post(tasks::task{[cleanup] {
#if CAORS_HAS_EXCEPTIONS
        try {
#endif
          (void)cleanup->reader->close();
#if CAORS_HAS_EXCEPTIONS
        } catch (...) { /* 析构无错误接收者；需观察错误时必须显式关闭。 */
        }
#endif
      }});
#if CAORS_HAS_EXCEPTIONS
    } catch (...) { /* 分配失败仍由 reader 的 RAII 析构释放句柄。 */
    }
#endif
  }
  std::uint64_t size() const noexcept override { return size_; }
  io_error start_read(std::uint64_t offset, std::uint64_t length, memory_resource* resource,
                      io_completion<byte_buffer> completion) override {
    if (offset > size_ || length > size_ - offset)
      return {io_errc::out_of_range};
    if (length > (std::numeric_limits<std::size_t>::max)())
      return {io_errc::limit_exceeded};
    {
      std::lock_guard lock{mutex_};
      if (closing_)
        return {io_errc::closed};
      ++inflight_;
    }
    struct operation {
      std::shared_ptr<worker_file> file;
      io_completion<byte_buffer> completion;
      ~operation() {
        if (file)
          file->finish_operation();
      }
    };
    // 先安装计数回收凭据，再做可能失败的任务/缓冲分配。
    std::shared_ptr<operation> op;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      op = std::make_shared<operation>();
      op->file = shared_from_this();
      op->completion = std::move(completion);
      auto status = worker_.post(tasks::task{[op, offset, length, resource] {
        auto bytes = make_byte_buffer(resource);
#if CAORS_HAS_EXCEPTIONS
        try {
#endif
          bytes.resize(static_cast<std::size_t>(length));
          std::size_t total = 0;
          while (total < bytes.size()) {
            if (op->completion.stop_token().stop_requested()) {
              (void)op->completion.complete(io_failure<byte_buffer>({io_errc::cancelled}));
              return;
            }
            io_result<std::size_t> count;
            {
              // 显式 seek/read 适配器串行访问共享游标，多个请求不会相互覆盖定位。
              std::lock_guard lock{op->file->reader_mutex_};
              count = op->file->reader_->read_at(offset + total,
                                                 std::span<std::byte>{bytes}.subspan(total));
            }
            if (!count || count.value == 0 || count.value > bytes.size() - total) {
              auto error = count ? io_error{io_errc::short_read} : count.error();
              (void)op->completion.complete(io_failure<byte_buffer>(error));
              return;
            }
            total += count.value;
          }
          (void)op->completion.complete({{}, std::move(bytes)});
#if CAORS_HAS_EXCEPTIONS
        } catch (const std::bad_alloc&) {
          (void)op->completion.complete(io_failure<byte_buffer>({io_errc::out_of_memory}));
        } catch (...) {
          (void)op->completion.complete(io_failure<byte_buffer>({io_errc::backend_failure}));
        }
#endif
      }});
      return status ? io_error{io_errc::closed, 0, static_cast<std::int64_t>(status.value())}
                    : io_error{};
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      if (!op)
        finish_operation();
      throw;
    }
#endif
  }
  io_error start_write(std::uint64_t offset, byte_buffer bytes,
                       io_completion<std::uint64_t> completion) override {
    if (bytes.size() > (std::numeric_limits<std::uint64_t>::max)() - offset)
      return {io_errc::out_of_range};
    {
      std::lock_guard lock{mutex_};
      if (closing_)
        return {io_errc::closed};
      ++inflight_;
    }
    struct operation {
      std::shared_ptr<worker_file> file;
      io_completion<std::uint64_t> completion;
      ~operation() {
        if (file)
          file->finish_operation();
      }
    };
    std::shared_ptr<operation> op;
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      op = std::make_shared<operation>();
      op->file = shared_from_this();
      op->completion = std::move(completion);
      const auto status = worker_.post(tasks::task{[op, offset, bytes = std::move(bytes)] {
#if CAORS_HAS_EXCEPTIONS
        try {
#endif
          std::size_t total = 0;
          // 空写也交给后端验证访问权限和关闭状态，不把只读句柄伪装成写入成功。
          do {
            if (op->completion.stop_token().stop_requested()) {
              (void)op->completion.complete(io_failure<std::uint64_t>({io_errc::cancelled}));
              return;
            }
            io_result<std::size_t> count;
            {
              std::lock_guard lock{op->file->reader_mutex_};
              count = op->file->reader_->write_at(offset + total,
                                                  std::span<const std::byte>{bytes}.subspan(total));
              if (count && count.value > 0 && count.value <= bytes.size() - total)
                op->file->size_.store(
                    (std::max)(op->file->size_.load(), offset + total + count.value));
            }
            if (!count || count.value > bytes.size() - total ||
                (count.value == 0 && total != bytes.size())) {
              (void)op->completion.complete(io_failure<std::uint64_t>(
                  count ? io_error{io_errc::backend_failure} : count.error()));
              return;
            }
            total += count.value;
          } while (total < bytes.size());
          (void)op->completion.complete({{}, total});
#if CAORS_HAS_EXCEPTIONS
        } catch (const std::bad_alloc&) {
          (void)op->completion.complete(io_failure<std::uint64_t>({io_errc::out_of_memory}));
        } catch (...) {
          (void)op->completion.complete(io_failure<std::uint64_t>({io_errc::backend_failure}));
        }
#endif
      }});
      return status ? io_error{io_errc::completion_rejected} : io_error{};
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      if (!op)
        finish_operation();
      throw;
    }
#endif
  }
  io_error start_close(io_completion<close_result> completion) override {
    std::optional<io_error> closed;
    {
      std::lock_guard lock{mutex_};
      closed = closed_;
      if (!closed) {
        waiters_.push_back(std::move(completion));
        closing_ = true;
      }
    }
    if (closed)
      (void)completion.complete({*closed, {}});
    else
      maybe_close();
    return {};
  }

private:
  void finish_operation() noexcept {
    {
      std::lock_guard lock{mutex_};
      --inflight_;
    }
    maybe_close();
  }
  void complete_close(io_error error) noexcept {
    std::vector<io_completion<close_result>> waiters;
    {
      std::lock_guard lock{mutex_};
      if (closed_)
        return;
      closed_ = error;
      waiters.swap(waiters_);
    }
    for (auto& waiter : waiters)
      (void)waiter.complete({error, {}});
  }
  void maybe_close() noexcept {
    {
      std::lock_guard lock{mutex_};
      if (!closing_ || inflight_ != 0 || close_started_)
        return;
      close_started_ = true;
    }
    struct cleanup {
      std::shared_ptr<worker_file> file;
      bool ran = false;
      ~cleanup() {
        if (file && !ran)
          file->complete_close({io_errc::closed});
      }
    };
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      auto guard = std::make_shared<cleanup>();
      guard->file = shared_from_this();
      auto status = worker_.post(tasks::task{[guard] {
        guard->ran = true;
        guard->file->close_attempted_ = true;
        io_error error;
#if CAORS_HAS_EXCEPTIONS
        try {
#endif
          error = guard->file->reader_->close();
#if CAORS_HAS_EXCEPTIONS
        } catch (...) {
          error = {io_errc::backend_failure};
        }
#endif
        guard->file->complete_close(error);
      }});
      if (status)
        complete_close({io_errc::closed});
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      complete_close({io_errc::out_of_memory});
    }
#endif
  }
  std::shared_ptr<void> lifetime_;
  std::shared_ptr<random_access_reader> reader_;
  tasks::executor worker_;
  std::atomic<std::uint64_t> size_;
  std::mutex reader_mutex_;
  std::mutex mutex_;
  std::size_t inflight_ = 0;
  bool closing_ = false;
  bool close_started_ = false;
  bool close_attempted_ = false;
  std::optional<io_error> closed_;
  std::vector<io_completion<close_result>> waiters_;
};
}
async_file::~async_file() = default;
io_error async_file::start_write(std::uint64_t, byte_buffer, io_completion<std::uint64_t>) {
  return {io_errc::unsupported};
}
io_result<std::size_t> random_access_reader::write_at(std::uint64_t, std::span<const std::byte>) {
  return io_failure<std::size_t>({io_errc::unsupported});
}
io_error file_system::start_open_options(resolved_file location, open_options options,
                                         memory_resource* resource,
                                         io_completion<std::shared_ptr<async_file>> completion) {
  if (options.access != file_access::read || options.creation != file_creation::open_existing)
    return {io_errc::unsupported};
  return start_open(std::move(location), resource, std::move(completion));
}
random_access_reader::~random_access_reader() = default;

io_error file_system::start_open(resolved_file, memory_resource*,
                                 io_completion<std::shared_ptr<async_file>>) {
  return {io_errc::unsupported};
}
io_result<std::shared_ptr<random_access_reader>> file_system::open_random_read(path_view,
                                                                               memory_resource*) {
  return io_failure<std::shared_ptr<random_access_reader>>({io_errc::unsupported});
}
std::shared_ptr<async_file> make_worker_file(std::shared_ptr<random_access_reader> reader,
                                             tasks::executor worker,
                                             std::shared_ptr<void> lifetime) {
  if (!reader || !worker)
    return {};
  return std::make_shared<worker_file>(std::move(reader), std::move(worker), std::move(lifetime));
}
namespace detail {
io_result<io_request<byte_buffer>>
read_file(std::shared_ptr<async_file> file, std::uint64_t offset, std::uint64_t length,
          std::uint64_t max_bytes, tasks::executor completion,
          std::function<void(const io_result<byte_buffer>&)> callback, memory_resource* resource) {
  if (!file)
    return io_failure<io_request<byte_buffer>>({io_errc::invalid_argument});
  if (offset > file->size() || length > file->size() - offset)
    return io_failure<io_request<byte_buffer>>({io_errc::out_of_range});
  if (length > max_bytes || length > file->max_read_size() ||
      length > (std::numeric_limits<std::size_t>::max)() ||
      length > static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)()))
    return io_failure<io_request<byte_buffer>>({io_errc::limit_exceeded});
  return start_io<byte_buffer>(
      std::move(completion), std::move(callback),
      [file, offset, length, resource](auto receipt) {
        return file->start_read(offset, length, resource, std::move(receipt));
      },
      file,
      [length](auto& result) {
        if (result.error() || result.value.size() == length)
          return;
        const auto reason =
            result.value.size() < length ? io_errc::short_read : io_errc::backend_failure;
        result = io_failure<byte_buffer>({reason});
      });
}
io_result<io_request<byte_buffer>> read_file_bounded(
    std::shared_ptr<async_file> file, std::uint64_t max_bytes, tasks::executor completion,
    std::function<void(const io_result<byte_buffer>&)> callback, memory_resource* resource) {
  const auto length = file ? file->size() : 0;
  return read_file(std::move(file), 0, length, max_bytes, std::move(completion),
                   std::move(callback), resource);
}
io_result<io_request<close_result>>
close_file(std::shared_ptr<async_file> file, tasks::executor completion,
           std::function<void(const io_result<close_result>&)> callback) {
  if (!file)
    return io_failure<io_request<close_result>>({io_errc::invalid_argument});
  return start_io<close_result>(
      std::move(completion), std::move(callback),
      [file](auto receipt) { return file->start_close(std::move(receipt)); }, file);
}
io_result<io_request<std::shared_ptr<async_file>>>
open_file(resolved_file file, tasks::executor completion,
          std::function<void(const io_result<std::shared_ptr<async_file>>&)> callback,
          memory_resource* resource, open_options options) {
  if (!file.backend)
    return io_failure<io_request<std::shared_ptr<async_file>>>({io_errc::invalid_argument});
  auto lifetime = file.backend;
  const auto source_id = file.source_id;
  return start_io<std::shared_ptr<async_file>>(
      std::move(completion), std::move(callback),
      [file = std::move(file), resource, options](auto receipt) mutable {
        auto backend = file.backend;
        return backend->start_open_options(std::move(file), options, resource, std::move(receipt));
      },
      lifetime,
      [lifetime, source_id](auto& result) {
        if (result.error())
          return;
        if (!result.value) {
          result.status = {io_errc::backend_failure};
          return;
        }
        result.value = std::make_shared<pinned_file>(lifetime, std::move(result.value), source_id);
      });
}
io_result<io_request<std::shared_ptr<async_file>>>
open_file_on_worker(resolved_file file, tasks::executor worker, tasks::executor completion,
                    std::function<void(const io_result<std::shared_ptr<async_file>>&)> callback,
                    memory_resource* resource) {
  if (!file.backend || !worker)
    return io_failure<io_request<std::shared_ptr<async_file>>>({io_errc::invalid_argument});
  auto lifetime = file.backend;
  const auto source_id = file.source_id;
  return start_io<std::shared_ptr<async_file>>(
      std::move(completion), std::move(callback),
      [file = std::move(file), worker, resource](auto receipt) mutable {
        auto status = worker.post(tasks::task{
            [file = std::move(file), worker, resource, receipt = std::move(receipt)]() mutable {
#if CAORS_HAS_EXCEPTIONS
              try {
#endif
                auto opened = file.backend->open_random_read(file.path, resource);
                if (!opened) {
                  (void)receipt.complete(io_failure<std::shared_ptr<async_file>>(opened.error()));
                } else {
                  auto stream = make_worker_file(std::move(opened.value), worker, file.backend);
                  if (!stream)
                    (void)receipt.complete(
                        io_failure<std::shared_ptr<async_file>>({io_errc::backend_failure}));
                  else
                    (void)receipt.complete({{}, std::move(stream)});
                }
#if CAORS_HAS_EXCEPTIONS
              } catch (const std::bad_alloc&) {
                (void)receipt.complete(
                    io_failure<std::shared_ptr<async_file>>({io_errc::out_of_memory}));
              } catch (...) {
                (void)receipt.complete(
                    io_failure<std::shared_ptr<async_file>>({io_errc::backend_failure}));
              }
#endif
            }});
        return status ? io_error{io_errc::closed} : io_error{};
      },
      lifetime,
      [lifetime, source_id](auto& result) {
        if (result.error())
          return;
        if (!result.value) {
          result.status = {io_errc::backend_failure};
          return;
        }
        result.value = std::make_shared<pinned_file>(lifetime, std::move(result.value), source_id);
      });
}
} // namespace detail
} // namespace caors::io
