// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_REQUEST_H_
#define CAORS_IO_REQUEST_H_
#include "caors/io/async_request.h"
namespace caors::io {
/** @brief 调用方的单层请求句柄；析构请求协作停止，不阻塞等待清理。 */
template <typename T> using request = io_request<T>;
namespace detail {
// 后端拒绝与执行失败对调用方统一经完成调度交付，保留旧后端启动握手。
template <typename T>
request<T> unwrap_request(io_result<io_request<T>> started, tasks::executor executor,
                          std::function<void(const io_result<T>&)> callback) {
  if (started)
    return std::move(started.value);
  auto status = started.error();
  auto failed = start_io<T>(executor, std::move(callback), [status](io_completion<T> receipt) {
    (void)receipt.complete(io_failure<T>(status));
    return io_error{};
  });
  return failed ? std::move(failed.value) : request<T>::failed(failed.error());
}
template <typename T, typename Starter>
request<T> submit_request(tasks::executor executor,
                          std::function<void(const io_result<T>&)> callback, Starter&& starter) {
  auto started = start_io<T>(executor, callback, std::forward<Starter>(starter));
  return unwrap_request(std::move(started), std::move(executor), std::move(callback));
}
} // namespace detail
} // namespace caors::io
#endif
