// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_BACKEND_H_
#define CAORS_IO_BACKEND_H_
#include "caors/io/async_request.h"
#include "caors/io/metadata.h"
#include "caors/io/options.h"
#include <mutex>
#include <span>
namespace caors::io {
class async_file;
class memory_file_system;
class file_system;
/**
 * @brief 独立文件后端协议，只处理后端相对路径，不管理挂载与用户调度。
 * @note
 * 接受后必须完成或销毁凭证；拒绝不得保留凭证。完成后清理宿主操作并释放其凭证，避免后端持有自身请求形成环。实现可绑定
 * worker 或宿主 loop。
 */
class CAORS_API backend {
public:
  virtual ~backend();
  /** @brief 打开只读已有文件；path 拥有字符串，resource 须覆盖实际操作与结果。 */
  [[nodiscard]] virtual io_error open(path path, open_options options, memory_resource* resource,
                                      io_completion<std::shared_ptr<async_file>> completion) = 0;
  /** @brief 整体创建或截断写入，持有 bytes 到完成；默认 unsupported，不承诺原子替换或落盘持久化。
   */
  [[nodiscard]] virtual io_error write(path location, byte_buffer bytes,
                                       io_completion<std::uint64_t> completion);
  /** @brief 查询属性；默认 unsupported。 */
  [[nodiscard]] virtual io_error stat(path location, io_completion<file_status> completion);
  /** @brief 枚举直接子项；默认 unsupported，结果缓冲由 resource 管理。 */
  [[nodiscard]] virtual io_error list(path location, memory_resource* resource,
                                      io_completion<directory_entry_list> completion);
};
/** @brief 本地文件操作适配；构造绑定可阻塞 worker，不隐式选择执行环境。 */
[[nodiscard]] CAORS_API std::shared_ptr<backend>
make_native_backend(path_view root, tasks::executor worker, memory_resource* resource = nullptr);
/** @brief 内存存储后端；索引操作串行化且不持锁回调，打开文件持有当时的内存内容。 */
class CAORS_API memory_backend final : public backend {
public:
  explicit memory_backend(memory_resource* resource = nullptr);
  [[nodiscard]] error store(path_view location, std::span<const std::byte> bytes);
  [[nodiscard]] error store(path_view location, std::string_view text) {
    return store(location, {reinterpret_cast<const std::byte*>(text.data()), text.size()});
  }
  [[nodiscard]] io_error open(path location, open_options options, memory_resource* resource,
                              io_completion<std::shared_ptr<async_file>> completion) override;

  /** @brief 整体创建或截断写入，持有 bytes 到完成；不承诺原子替换或落盘持久化。
   */
  [[nodiscard]] io_error write(path location, byte_buffer bytes,
                               io_completion<std::uint64_t> completion) override;
  /** @brief 查询属性；结果拥有数据。 */
  [[nodiscard]] io_error stat(path location, io_completion<file_status> completion) override;
  /** @brief 枚举直接子项；结果缓冲由 resource 管理。 */
  [[nodiscard]] io_error list(path location, memory_resource* resource,
                              io_completion<directory_entry_list> completion) override;

private:
  std::shared_ptr<memory_file_system> files_;
  std::mutex mutex_;
};
/** @brief 创建可挂载的内存后端，配置数据使用 store。 */
[[nodiscard]] CAORS_API std::shared_ptr<memory_backend>
make_memory_backend(memory_resource* resource = nullptr);
} // namespace caors::io
#endif
