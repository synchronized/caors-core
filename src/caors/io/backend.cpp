// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io/backend.h"
#include "caors/io/detail/error.h"
#include "caors/io/memory_file_system.h"
#include "caors/io/native_file_system.h"
namespace caors::io {
namespace {
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

private:
  std::shared_ptr<native_file_system> files_;
  tasks::executor worker_;
};
} // namespace
backend::~backend() = default;
std::shared_ptr<backend> make_native_backend(path_view root, tasks::executor worker,
                                             memory_resource* resource) {
  return std::make_shared<native_backend>(root, std::move(worker), resource);
}
memory_backend::memory_backend(memory_resource* resource)
    : files_(std::make_shared<memory_file_system>(resource)) {}
error memory_backend::store(path_view location, std::span<const std::byte> bytes) {
  return files_->write_all(location, bytes);
}
io_error memory_backend::open(path location, open_options options, memory_resource* resource,
                              io_completion<std::shared_ptr<async_file>> completion) {
  if (options.access != file_access::read || options.creation != file_creation::open_existing)
    return {io_errc::unsupported};
  return files_->start_open({files_, std::move(location), files_->source_instance()}, resource,
                            std::move(completion));
}
std::shared_ptr<memory_backend> make_memory_backend(memory_resource* resource) {
  return std::make_shared<memory_backend>(resource);
}
} // namespace caors::io
