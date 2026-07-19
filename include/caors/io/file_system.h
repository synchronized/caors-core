// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_FILE_SYSTEM_H_
#define CAORS_IO_FILE_SYSTEM_H_

#include "caors/io/stream.h"

#include <chrono>
#include <cstdint>
#include <span>
#include <string_view>

namespace caors::io {

/** @brief 文件系统条目类型。 */
enum class file_kind {
  /** @brief 普通文件或可读取字节对象。 */
  file,
  /** @brief 目录或虚拟目录。 */
  directory,
};

/** @brief `file_system::list()` 返回的一条目录项。 */
struct directory_entry {
  /** @brief 直接子项名称，不包含父目录路径。 */
  caors::io::path name;
  /** @brief 可再次传给同一个 `file_system` 的路径。 */
  caors::io::path path;
  /** @brief 子项类型。 */
  file_kind kind = file_kind::file;
};

/** @brief 目录项列表，使用 CAORS allocator 管理存储。 */
using directory_entry_list = vector<directory_entry>;

/** @brief 目录枚举结果。 */
using directory_entry_list_result = result<directory_entry_list>;

/** @brief 文件或目录的基础属性。 */
struct file_status {
  /** @brief 文件或目录类型。 */
  file_kind kind = file_kind::file;
  /** @brief 字节大小；只有 `has_size == true` 时有效。 */
  std::uint64_t size = 0;
  /** @brief 是否提供了 `size`。目录或虚拟目录通常不提供。 */
  bool has_size = false;
  /** @brief 最后修改时间；只有 `has_modified_time == true` 时有效。 */
  std::chrono::system_clock::time_point modified_time{};
  /** @brief 是否提供了 `modified_time`。 */
  bool has_modified_time = false;
  /** @brief 是否只读；只有 `has_read_only == true` 时有效。 */
  bool read_only = false;
  /** @brief 是否提供了 `read_only`。 */
  bool has_read_only = false;
};

/** @brief 文件或目录状态查询结果。 */
using file_status_result = result<file_status>;

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

  /**
   * @brief 查询文件或目录的基础属性。
   *
   * 空路径表示文件系统根目录。默认实现返回 `errc::unsupported`。
   */
  [[nodiscard]] virtual file_status_result status(path_view path) const;

  /**
   * @brief 列出目录下的直接子项。
   *
   * 空路径表示文件系统根目录。默认实现返回 `errc::unsupported`，只支持读写的后端可以不覆盖。
   */
  [[nodiscard]] virtual directory_entry_list_result list(path_view directory,
                                                         memory_resource* resource = nullptr);

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
