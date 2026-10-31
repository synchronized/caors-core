// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_RESULT_H_
#define CAORS_IO_RESULT_H_
#include "caors/io/types.h"
#include <cstdint>
namespace caors::io {
/** @brief IO 失败分类；后端原始诊断独立保留，不要求分配错误字符串。 */
enum class io_errc {
  success,
  invalid_argument,
  not_found,
  unsupported,
  cancelled,
  closed,
  out_of_range,
  limit_exceeded,
  short_read,
  permission_denied,
  backend_failure,
  completion_rejected,
  out_of_memory,
  pending
};
/** @brief 不借用外部字符串的 IO 诊断；backend_id 由宿主分配进程内稳定标识。 */
struct io_error {
  io_errc value = io_errc::success;
  std::uint64_t backend_id = 0;
  std::int64_t native_code = 0;
  [[nodiscard]] explicit operator bool() const noexcept { return value != io_errc::success; }
};
/** @brief 带拥有式结果与 IO 诊断的返回值。 */
template <typename T> using io_result = result<T, io_error>;
/** @brief 构造 IO 失败结果。 */
template <typename T> io_result<T> io_failure(io_error error) { return {error, T{}}; }

} // namespace caors::io
#endif
