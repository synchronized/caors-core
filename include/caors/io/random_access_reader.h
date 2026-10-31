// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_RANDOM_ACCESS_READER_H_
#define CAORS_IO_RANDOM_ACCESS_READER_H_
#include "caors/io/result.h"
#include <span>
namespace caors::io {
/** @brief 显式 worker 适配所需的同步随机读取接口；read_at 可以短读，不得隐式读取整个文件。 */
class CAORS_API random_access_reader {
public:
  virtual ~random_access_reader();
  /** @brief 当前句柄可见长度；不要求追踪其他句柄的修改。 */
  [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;
  /** @brief 同步读取到借用缓冲，返回实际字节数；缓冲仅需覆盖本次调用。 */
  [[nodiscard]] virtual io_result<std::size_t> read_at(std::uint64_t offset,
                                                       std::span<std::byte> buffer) = 0;
  /** @brief 可选随机写入，允许短写；默认不支持，由 worker 适配累计到写满。 */
  [[nodiscard]] virtual io_result<std::size_t> write_at(std::uint64_t offset,
                                                        std::span<const std::byte> bytes);
  /** @brief 显式关闭，错误可查询；析构仅作 RAII 兜底。 */
  [[nodiscard]] virtual io_error close() = 0;
};
} // namespace caors::io
#endif
