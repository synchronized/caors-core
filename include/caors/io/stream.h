// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_STREAM_H_
#define CAORS_IO_STREAM_H_

#include "caors/io/types.h"
#include "caors/memory/unique_ptr.h"

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

/** @brief 基于 `io::byte_buffer` 的同步输入流。 */
class CAORS_API byte_buffer_input_stream final : public input_stream {
public:
  explicit byte_buffer_input_stream(byte_buffer bytes);

  /** @brief 从内部字节缓冲区读取最多 `buffer.size()` 字节。 */
  [[nodiscard]] size_result read(std::span<std::byte> buffer) override;

  /** @brief 判断内部读取游标是否已经到达缓冲区末尾。 */
  [[nodiscard]] bool eof() const noexcept override;

private:
  byte_buffer bytes_;
  std::size_t position_ = 0;
};

/** @brief 拥有式输入流指针，使用 CAORS erased deleter 管理销毁。 */
using input_stream_ptr = memory::unique_ptr<input_stream>;

/** @brief 拥有式输出流指针，使用 CAORS erased deleter 管理销毁。 */
using output_stream_ptr = memory::unique_ptr<output_stream>;

/** @brief 输入流打开结果。 */
using input_stream_result = result<input_stream_ptr>;

/** @brief 输出流打开结果。 */
using output_stream_result = result<output_stream_ptr>;

/** @brief 使用指定资源构造拥有式内存输入流。 */
[[nodiscard]] CAORS_API input_stream_ptr make_input_stream(byte_buffer bytes,
                                                           memory_resource* resource = nullptr);

} // namespace caors::io

#endif
