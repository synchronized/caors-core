// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/backend.h"
#include "caors/io/detail/error.h"
#include "caors/io/memory_file_system.h"
#include "caors/io/native_file_system.h"
namespace caors::io {
namespace {
template <typename T, typename Operation>
io_error run_on_worker(tasks::executor worker, io_completion<T> receipt, Operation operation) {
  if (!worker)
    return {io_errc::completion_rejected};
  auto status = worker.post(
      tasks::task{[receipt = std::move(receipt), operation = std::move(operation)]() mutable {
#if CAORS_HAS_EXCEPTIONS
        try {
#endif
          if (receipt.stop_token().stop_requested())
            (void)receipt.complete(io_failure<T>({io_errc::cancelled}));
          else
            (void)receipt.complete(operation());
#if CAORS_HAS_EXCEPTIONS
        } catch (const std::bad_alloc&) {
          (void)receipt.complete(io_failure<T>({io_errc::out_of_memory}));
        } catch (...) {
          (void)receipt.complete(io_failure<T>({io_errc::backend_failure}));
        }
#endif
      }});
  return status
             ? io_error{io_errc::completion_rejected, 0, static_cast<std::int64_t>(status.value())}
             : io_error{};
}
class native_backend final : public backend {
public:
  native_backend(path_view root, tasks::executor worker, memory_resource* resource)
      : files_(std::make_shared<native_file_system>(root, resource)), worker_(std::move(worker)) {}
  io_error open(path location, open_options options, memory_resource* resource,
                io_completion<std::shared_ptr<async_file>> completion) override {
    if (options.access != file_access::read || options.creation != file_creation::open_existing)
      return {io_errc::unsupported};
    if (!worker_)
      return {io_errc::completion_rejected};
    auto status = worker_.post(tasks::task{[files = files_, worker = worker_,
                                            location = std::move(location), resource,
                                            receipt = std::move(completion)]() mutable {
#if CAORS_HAS_EXCEPTIONS
      try {
#endif
        if (receipt.stop_token().stop_requested()) {
          (void)receipt.complete(io_failure<std::shared_ptr<async_file>>({io_errc::cancelled}));
          return;
        }
        auto opened = files->open_random_read(location, resource);
        if (!opened)
          (void)receipt.complete(io_failure<std::shared_ptr<async_file>>(opened.error()));
        else
          (void)receipt.complete({{}, make_worker_file(std::move(opened.value), worker, files)});
#if CAORS_HAS_EXCEPTIONS
      } catch (const std::bad_alloc&) {
        (void)receipt.complete(io_failure<std::shared_ptr<async_file>>({io_errc::out_of_memory}));
      } catch (...) {
        (void)receipt.complete(io_failure<std::shared_ptr<async_file>>({io_errc::backend_failure}));
      }
#endif
    }});
    return status ? io_error{io_errc::completion_rejected, 0,
                             static_cast<std::int64_t>(status.value())}
                  : io_error{};
  }

  io_error write(path location, byte_buffer bytes, io_completion<std::uint64_t> receipt) override {
    return run_on_worker(
        worker_, std::move(receipt),
        [files = files_, location = std::move(location), bytes = std::move(bytes)] {
          auto status = files->write_all(location, std::span<const std::byte>{bytes});
          return io_result<std::uint64_t>{detail::convert_error(status), status ? 0 : bytes.size()};
        });
  }
  io_error stat(path location, io_completion<file_status> receipt) override {
    return run_on_worker(
        worker_, std::move(receipt), [files = files_, location = std::move(location)] {
          auto value = files->status(location);
          return io_result<file_status>{detail::convert_error(value.error()), value.value};
        });
  }
  io_error list(path location, memory_resource* resource,
                io_completion<directory_entry_list> receipt) override {
    return run_on_worker(worker_, std::move(receipt),
                         [files = files_, location = std::move(location), resource] {
                           auto value = files->list(location, resource);
                           return io_result<directory_entry_list>{
                               detail::convert_error(value.error()), std::move(value.value)};
                         });
  }

private:
  std::shared_ptr<native_file_system> files_;
  tasks::executor worker_;
};
} // namespace
backend::~backend() = default;
io_error backend::write(path, byte_buffer, io_completion<std::uint64_t>) {
  return {io_errc::unsupported};
}
io_error backend::stat(path, io_completion<file_status>) { return {io_errc::unsupported}; }
io_error backend::list(path, memory_resource*, io_completion<directory_entry_list>) {
  return {io_errc::unsupported};
}
std::shared_ptr<backend> make_native_backend(path_view root, tasks::executor worker,
                                             memory_resource* resource) {
  return std::make_shared<native_backend>(root, std::move(worker), resource);
}
memory_backend::memory_backend(memory_resource* resource)
    : files_(std::make_shared<memory_file_system>(resource)) {}
error memory_backend::store(path_view location, std::span<const std::byte> bytes) {
  std::lock_guard lock{mutex_};
  return files_->write_all(location, bytes);
}
io_error memory_backend::open(path location, open_options options, memory_resource* resource,
                              io_completion<std::shared_ptr<async_file>> completion) {
  if (options.access != file_access::read || options.creation != file_creation::open_existing)
    return {io_errc::unsupported};
  io_result<std::shared_ptr<random_access_reader>> opened;
  {
    std::lock_guard lock{mutex_};
    opened = files_->open_random_read(location, resource);
  }
  if (!opened)
    return opened.error();
  auto file = make_worker_file(std::move(opened.value), tasks::inline_executor(), files_);
  (void)completion.complete({{}, std::move(file)});
  return {};
}
io_error memory_backend::write(path location, byte_buffer bytes,
                               io_completion<std::uint64_t> receipt) {
  if (receipt.stop_token().stop_requested())
    return {io_errc::cancelled};
  error status;
  {
    std::lock_guard lock{mutex_};
    status = files_->write_all(location, std::span<const std::byte>{bytes});
  }
  (void)receipt.complete({detail::convert_error(status), status ? 0 : bytes.size()});
  return {};
}
io_error memory_backend::stat(path location, io_completion<file_status> receipt) {
  file_status_result value;
  {
    std::lock_guard lock{mutex_};
    value = files_->status(location);
  }
  (void)receipt.complete({detail::convert_error(value.error()), value.value});
  return {};
}
io_error memory_backend::list(path location, memory_resource* resource,
                              io_completion<directory_entry_list> receipt) {
  directory_entry_list_result value;
  {
    std::lock_guard lock{mutex_};
    value = files_->list(location, resource);
  }
  (void)receipt.complete({detail::convert_error(value.error()), std::move(value.value)});
  return {};
}
std::shared_ptr<memory_backend> make_memory_backend(memory_resource* resource) {
  return std::make_shared<memory_backend>(resource);
}
} // namespace caors::io
