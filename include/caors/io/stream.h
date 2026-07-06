// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_STREAM_H_
#define CAORS_IO_STREAM_H_

#include "caors/io/types.h"

#include <span>

namespace caors::io {

/** @brief 同步输入字节流接口。 */
class CAORS_API input_stream {
public:
  input_stream() = default;
  virtual ~input_stream();

  input_stream(const input_stream&) = delete;
  input_stream& operator=(const input_stream&) = delete;
  input_stream(input_stream&&) = delete;
  input_stream& operator=(input_stream&&) = delete;

  /** @brief 读取最多 `buffer.size()` 字节，返回实际读取数量。 */
  [[nodiscard]] virtual size_result read(std::span<std::byte> buffer) = 0;

  /** @brief 判断流是否已经到达末尾。 */
  [[nodiscard]] virtual bool eof() const noexcept = 0;
};

/** @brief 同步输出字节流接口。 */
class CAORS_API output_stream {
public:
  output_stream() = default;
  virtual ~output_stream();

  output_stream(const output_stream&) = delete;
  output_stream& operator=(const output_stream&) = delete;
  output_stream(output_stream&&) = delete;
  output_stream& operator=(output_stream&&) = delete;

  /** @brief 写出最多 `buffer.size()` 字节，返回实际写出数量。 */
  [[nodiscard]] virtual size_result write(std::span<const std::byte> buffer) = 0;
};

} // namespace caors::io

namespace caors {

using io::input_stream;
using io::output_stream;

} // namespace caors

#endif
