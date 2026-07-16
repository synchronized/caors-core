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
