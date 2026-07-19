// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_FILE_SYSTEM_H_
#define CAORS_IO_FILE_SYSTEM_H_

#include "caors/io/stream.h"

#include <span>
#include <string_view>

namespace caors::io {

/**
 * @brief 同步文件系统抽象。
 *
 * `file_system` 表示一类可用字符串路径定位的字节来源。本地文件、内存文件、资源包和远程网络资源都
 * 可以实现该接口。路径语义由具体实现定义。
 */
class CAORS_API file_system {
public:
  file_system() = default;
  virtual ~file_system();

  file_system(const file_system&) = delete;
  file_system& operator=(const file_system&) = delete;
  file_system(file_system&&) = delete;
  file_system& operator=(file_system&&) = delete;

  /** @brief 判断路径是否存在。 */
  [[nodiscard]] virtual bool exists(path_view path) const = 0;

  /** @brief 读取整个文件或对象内容。 */
  [[nodiscard]] virtual byte_buffer_result read_all(path_view path,
                                                    memory_resource* resource = nullptr) = 0;

  /**
   * @brief 打开同步输入流。
   *
   * 默认实现通过 `read_all()` 建立内存输入流，保证所有后端立即可用。
   * 需要真正分块读取的后端可以覆盖该函数。
   */
  [[nodiscard]] virtual input_stream_result open_read(path_view path,
                                                      memory_resource* resource = nullptr);

  /**
   * @brief 打开同步输出流。
   *
   * 默认实现建立内存缓冲输出流，并在显式 `close()` 时通过 `write_all()` 提交。
   * 析构函数不会隐式提交失败状态，调用方必须检查 `close()` 返回值。
   */
  [[nodiscard]] virtual output_stream_result open_write(path_view path,
                                                        memory_resource* resource = nullptr);

  /** @brief 写出整个字节缓冲区。 */
  [[nodiscard]] virtual error write_all(path_view path, std::span<const std::byte> bytes) = 0;

  /** @brief 写出字符串内容。 */
  [[nodiscard]] error write_all(path_view path, std::string_view text) {
    return write_all(path, std::span<const std::byte>{
                               reinterpret_cast<const std::byte*>(text.data()), text.size()});
  }
};

} // namespace caors::io

#endif
