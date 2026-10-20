// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/chunked_file.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <type_traits>

namespace caors::io {
namespace {
template <typename T> T initial_value(memory_resource* resource) {
  if constexpr (std::is_same_v<T, byte_buffer>)
    return make_byte_buffer(resource);
  else
    return {};
}

template <typename T>
class chunk_operation final : public std::enable_shared_from_this<chunk_operation<T>> {
public:
  chunk_operation(std::shared_ptr<async_file> file, chunk_read_options options,
                  tasks::executor executor, chunk_consumer consumer, chunk_progress progress,
                  memory_resource* resource, io_completion<T> terminal)
      : file_(std::move(file)), executor_(std::move(executor)), consumer_(std::move(consumer)),
        progress_(std::move(progress)), resource_(resource), terminal_(std::move(terminal)),
        total_(file_->size()), block_((std::min)(options.chunk_bytes, file_->max_read_size())),
        value_(initial_value<T>(resource)) {
    if constexpr (std::is_same_v<T, byte_buffer>)
      value_.resize(static_cast<std::size_t>(total_));
  }
  ~chunk_operation() {
    // 内层通知被丢弃时不会调用推进函数；最后回调引用释放即收口外层凭证。
    if (!done_ && pending_) {
      if (auto result = pending_->result(); result && !*result)
        (void)terminal_.complete(io_failure<T>(result->error()));
    }
  }
  void start() {
    std::weak_ptr<chunk_operation> weak = this->shared_from_this();
    stop_ = std::make_unique<stop_callback>(terminal_.stop_token(), [weak] {
      if (auto self = weak.lock()) {
        std::shared_ptr<io_request<byte_buffer>> request;
        {
          std::lock_guard lock{self->mutex_};
          request = self->pending_;
        }
        if (request)
          request->cancel();
      }
    });
    drive();
  }

private:
  using stop_callback = std::stop_callback<std::function<void()>>;
  void finish(io_error error = {}) {
    {
      std::lock_guard lock{mutex_};
      if (done_)
        return;
      done_ = true;
      driving_ = false;
    }
    if (error)
      (void)terminal_.complete(io_failure<T>(error));
    else
      (void)terminal_.complete({{}, std::move(value_)});
  }
  void ready() {
    {
      std::lock_guard lock{mutex_};
      ready_ = true;
    }
    drive();
  }
  void drive() {
    auto self = this->shared_from_this();
    {
      std::lock_guard lock{mutex_};
      if (driving_ || done_)
        return;
      driving_ = true;
    }
#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      for (;;) {
        std::shared_ptr<io_request<byte_buffer>> completed;
        {
          std::lock_guard lock{mutex_};
          if (waiting_) {
            if (!ready_ || !pending_) {
              driving_ = false;
              return;
            }
            completed = std::move(pending_);
            waiting_ = false;
            ready_ = false;
          }
        }
        if (completed) {
          const auto result = completed->result();
          if (!result) {
            finish({io_errc::backend_failure});
            return;
          }
          if (!*result) {
            finish(result->error());
            return;
          }
          if (terminal_.stop_token().stop_requested()) {
            finish({io_errc::cancelled});
            return;
          }
          if constexpr (std::is_same_v<T, byte_buffer>) {
            std::copy(result->value.begin(), result->value.end(),
                      value_.begin() + static_cast<std::ptrdiff_t>(offset_));
          } else {
            const auto status = consumer_(offset_, result->value);
            if (status) {
              finish(status);
              return;
            }
          }
          offset_ += result->value.size();
          if constexpr (std::is_same_v<T, chunk_summary>)
            value_.bytes = offset_;
          if (progress_)
            progress_(offset_, total_);
        }
        // 即时完成只设置 ready 标记，当前唯一推进者迭代处理，避免每块递归调用。
        if (terminal_.stop_token().stop_requested()) {
          finish({io_errc::cancelled});
          return;
        }
        if (offset_ == total_) {
          finish();
          return;
        }
        const auto length = (std::min)(block_, total_ - offset_);
        {
          std::lock_guard lock{mutex_};
          waiting_ = true;
        }
        auto next = request_read(
            file_, offset_, length, length, executor_, [self](const auto&) { self->ready(); },
            resource_);
        if (!next) {
          finish(next.error());
          return;
        }
        auto request = std::make_shared<io_request<byte_buffer>>(std::move(next.value));
        {
          std::lock_guard lock{mutex_};
          pending_ = request;
        }
        if (terminal_.stop_token().stop_requested())
          request->cancel();
      }
#if CAORS_HAS_EXCEPTIONS
    } catch (const std::bad_alloc&) {
      finish({io_errc::out_of_memory});
    } catch (...) {
      finish({io_errc::backend_failure});
    }
#endif
  }
  std::shared_ptr<async_file> file_;
  tasks::executor executor_;
  chunk_consumer consumer_;
  chunk_progress progress_;
  memory_resource* resource_;
  io_completion<T> terminal_;
  const std::uint64_t total_;
  const std::uint64_t block_;
  T value_;
  std::unique_ptr<stop_callback> stop_;
  std::mutex mutex_;
  std::shared_ptr<io_request<byte_buffer>> pending_;
  std::uint64_t offset_ = 0;
  bool waiting_ = false;
  bool ready_ = false;
  bool driving_ = false;
  bool done_ = false;
};

template <typename T>
io_result<io_request<T>> start_chunks(std::shared_ptr<async_file> file, chunk_read_options options,
                                      tasks::executor executor, chunk_consumer consumer,
                                      std::function<void(const io_result<T>&)> callback,
                                      chunk_progress progress, memory_resource* resource) {
  if (!file || options.chunk_bytes == 0)
    return io_failure<io_request<T>>({io_errc::invalid_argument});
  if constexpr (std::is_same_v<T, chunk_summary>) {
    if (!consumer)
      return io_failure<io_request<T>>({io_errc::invalid_argument});
  }
  const auto size = file->size();
  if (size > options.max_bytes || (size && file->max_read_size() == 0))
    return io_failure<io_request<T>>({io_errc::limit_exceeded});
  if constexpr (std::is_same_v<T, byte_buffer>) {
    if (size > static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)()))
      return io_failure<io_request<T>>({io_errc::limit_exceeded});
  }
  options.chunk_bytes =
      (std::min)(options.chunk_bytes,
                 static_cast<std::uint64_t>((std::numeric_limits<std::ptrdiff_t>::max)()));
  return start_io<T>(executor, std::move(callback),
                     [file = std::move(file), options, executor, consumer = std::move(consumer),
                      progress = std::move(progress), resource](io_completion<T> terminal) mutable {
                       auto operation = std::make_shared<chunk_operation<T>>(
                           std::move(file), options, executor, std::move(consumer),
                           std::move(progress), resource, std::move(terminal));
                       operation->start();
                       return io_error{};
                     });
}
} // namespace

io_result<io_request<byte_buffer>>
read_all_chunked(std::shared_ptr<async_file> file, chunk_read_options options,
                 tasks::executor completion,
                 std::function<void(const io_result<byte_buffer>&)> callback,
                 chunk_progress progress, memory_resource* resource) {
  return start_chunks<byte_buffer>(std::move(file), options, std::move(completion), {},
                                   std::move(callback), std::move(progress), resource);
}
io_result<io_request<chunk_summary>>
consume_chunks(std::shared_ptr<async_file> file, chunk_read_options options,
               tasks::executor completion, chunk_consumer consumer,
               std::function<void(const io_result<chunk_summary>&)> callback,
               chunk_progress progress, memory_resource* resource) {
  return start_chunks<chunk_summary>(std::move(file), options, std::move(completion),
                                     std::move(consumer), std::move(callback), std::move(progress),
                                     resource);
}
} // namespace caors::io
