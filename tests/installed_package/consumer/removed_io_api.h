// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_TEST_REMOVED_IO_API_H_
#define CAORS_TEST_REMOVED_IO_API_H_
#include "caors/io/legacy.h"
#include <utility>
// 故意包含完整兼容头，确保被删除函数没有通过旧聚合头重新暴露。
#if __has_include("caors/io/detail/read_operation.h") ||                                           \
                  __has_include("caors/io/detail/vfs_operations.h")
#error "Internal IO orchestration headers must not be installed"
#endif
namespace caors::tests::removed_io_api {
template <typename T>
concept old_open =
    requires(T value, tasks::executor executor) { request_open(std::move(value), executor); };
template <typename T>
concept old_worker_open = requires(T value, tasks::executor executor) {
  request_open_on_worker(std::move(value), executor, executor);
};
template <typename T>
concept old_read =
    requires(T value, tasks::executor executor) { request_read(value, 0, 1, 1, executor); };
template <typename T>
concept old_bounded_read =
    requires(T value, tasks::executor executor) { read_all_bounded(value, 1, executor); };
template <typename T>
concept old_close = requires(T value, tasks::executor executor) { request_close(value, executor); };
static_assert(!old_open<io::resolved_file>);
static_assert(!old_worker_open<io::resolved_file>);
static_assert(!old_read<std::shared_ptr<io::async_file>>);
static_assert(!old_bounded_read<std::shared_ptr<io::async_file>>);
static_assert(!old_close<std::shared_ptr<io::async_file>>);
} // namespace caors::tests::removed_io_api
#endif
