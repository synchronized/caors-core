// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_FILE_SYSTEM_H_
#define CAORS_IO_FILE_SYSTEM_H_

#include "caors/io/async_file.h"
#include "caors/io/metadata.h"
#include "caors/io/route_snapshot.h"
#include "caors/io/stream.h"

#include <chrono>
#include <cstdint>
#include <span>
#include <string_view>

namespace caors::io {

/**
 * @brief 文件系统后端能力声明。
 *
 * 该结构描述后端整体支持的能力，供资源系统、编辑器和工具层做路径路由或 UI 决策。具体路径仍可能因为
 * 权限、缺失或后端状态返回错误。
 */
struct file_system_capabilities {
  /** @brief 支持按路径读取字节或打开输入流。 */
  bool read = false;
  /** @brief 支持按路径写入字节或打开输出流。 */
  bool write = false;
  /** @brief 支持基础属性查询。 */
  bool status = false;
  /** @brief 支持目录枚举。 */
  bool list = false;
  /** @brief 支持文件或目录变化监听。 */
  bool watch = false;
  /** @brief 支持随机访问或 seek 语义。 */
  bool seek = false;
};

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

  /** @brief 无挂载叶根的进程内实例标记；不作为持久标识或内容修订号。 */
  [[nodiscard]] std::uint64_t source_instance() const noexcept { return source_instance_; }

  /** @brief 捕获内置路由的稳定状态；叶后端须使用 route_snapshot::capture 提供根所有权。 */
  [[nodiscard]] result<route_snapshot> snapshot() const;

  /** @brief 是否转发至可变化的路由；自定义路由必须覆盖为 true，并实现 freeze_routes。 */
  [[nodiscard]] virtual bool routes_paths() const noexcept { return false; }

  /** @brief 路由扩展：解析到最终拥有式后端；叶后端默认返回 unsupported。 */
  [[nodiscard]] virtual result<resolved_file> resolve_owned(path_view location) const;

  /**
   * @brief 路由扩展：捕获不可变副本；默认仅允许有 owner 的叶后端。
   * @note 自定义路由不能沿用叶实现，否则无法保证嵌套快照稳定；实现不得调用实时路由解析。
   */
  [[nodiscard]] virtual result<std::shared_ptr<file_system>>
  freeze_routes(detail::snapshot_context& context, std::shared_ptr<void> owner) const;

  /** @brief 原生异步打开可选能力；默认 unsupported，不进行同步回退。 */
  [[nodiscard]] virtual io_error start_open(resolved_file location, memory_resource* resource,
                                            io_completion<std::shared_ptr<async_file>> completion);
  /** @brief 带访问选项的兼容协议；默认仅转发只读已有文件，其余返回 unsupported。 */
  [[nodiscard]] virtual io_error
  start_open_options(resolved_file location, open_options options, memory_resource* resource,
                     io_completion<std::shared_ptr<async_file>> completion);
  /** @brief 显式同步 worker 适配的可选能力；默认 unsupported，不回退 open_read/read_all。 */
  [[nodiscard]] virtual io_result<std::shared_ptr<random_access_reader>>
  open_random_read(path_view location, memory_resource* resource = nullptr);

  /** @brief 判断路径是否存在。 */
  [[nodiscard]] virtual bool exists(path_view path) const = 0;

  /** @brief 返回该文件系统后端声明的整体能力。 */
  [[nodiscard]] virtual file_system_capabilities capabilities() const noexcept;

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
  [[nodiscard]] virtual directory_entry_list_result
  list(path_view directory, memory_resource* result_resource = nullptr);

  /** @brief 读取整个文件或对象内容。 */
  [[nodiscard]] virtual byte_buffer_result read_all(path_view path,
                                                    memory_resource* result_resource = nullptr) = 0;

  /**
   * @brief 打开同步输入流。
   *
   * 默认实现通过 `read_all()` 建立内存输入流，保证所有后端立即可用。
   * 需要真正分块读取的后端可以覆盖该函数。
   */
  [[nodiscard]] virtual input_stream_result open_read(path_view path,
                                                      memory_resource* result_resource = nullptr);

  /**
   * @brief 打开同步输出流。
   *
   * 默认实现建立内存缓冲输出流，并在显式 `close()` 时通过 `write_all()` 提交。
   * 析构函数不会隐式提交失败状态，调用方必须检查 `close()` 返回值。
   */
  [[nodiscard]] virtual output_stream_result open_write(path_view path,
                                                        memory_resource* result_resource = nullptr);

  /** @brief 写出整个字节缓冲区。 */
  [[nodiscard]] virtual error write_all(path_view path, std::span<const std::byte> bytes) = 0;

  /** @brief 写出字符串内容。 */
  [[nodiscard]] error write_all(path_view path, std::string_view text) {
    return write_all(path, std::span<const std::byte>{
                               reinterpret_cast<const std::byte*>(text.data()), text.size()});
  }

private:
  const std::uint64_t source_instance_ = detail::next_route_id();
};

} // namespace caors::io

#endif
