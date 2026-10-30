// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_FILE_SERVICE_H_
#define CAORS_IO_FILE_SERVICE_H_
#include "caors/io/backend.h"
#include "caors/io/file.h"
#include "caors/io/uri.h"
namespace caors::io {
namespace detail {
struct vfs_state;
struct vfs_routes;
class vfs_access;
}
class file_service;
/** @brief 完成通知执行器与借用的分配器，须覆盖实际操作、结果和清理寿命。 */
struct file_service_options {
  tasks::executor completion;
  memory_resource* resource = nullptr;
};
/** @brief 固定路由和调度的访问批次；保活后端但不冻结文件内容，不拥有线程或分配器。 */
class CAORS_API file_batch {
public:
  file_batch() = default;
  file_batch(const file_service&) = delete;
  file_batch& operator=(const file_service&) = delete;
  /** @brief 按权限及创建选项异步打开文件，返回单层请求；错误通过同一调度交付或句柄查询。 */
  [[nodiscard]] request<file> open(path_view location, open_options options = {},
                                   std::function<void(const io_result<file>&)> callback = {}) const;
  /** @brief 整体读取并关闭文件；关闭失败也报告失败，取消不会阻塞等待真实清理。 */
  [[nodiscard]] request<byte_buffer>
  read(path_view location, read_options options = {},
       std::function<void(const io_result<byte_buffer>&)> callback = {}) const;

  /** @brief 范围读取并自动关闭，越界和预算失败仍清理已打开文件。 */
  [[nodiscard]] request<byte_buffer>
  read_range(path_view location, range_read_options range,
             std::function<void(const io_result<byte_buffer>&)> callback = {}) const;
  /** @brief 创建或截断整文件写入；输入拥有至完成，取消不能回滚已经发生的写入。 */
  [[nodiscard]] request<std::uint64_t>
  write(path_view location, byte_buffer bytes,
        std::function<void(const io_result<std::uint64_t>&)> callback = {}) const;
  /** @brief 异步查询属性，缺失、权限及后端失败通过结果报告。 */
  [[nodiscard]] request<file_status>
  stat(path_view location, std::function<void(const io_result<file_status>&)> callback = {}) const;
  /** @brief 枚举直接子项，结果路径属于本次逻辑目录，不暴露后端本地路径。 */
  [[nodiscard]] request<directory_entry_list>
  list(path_view location,
       std::function<void(const io_result<directory_entry_list>&)> callback = {}) const;

private:
  friend class file_service;
  friend class detail::vfs_access;
  [[nodiscard]] result<backend_location> resolve(path_view location) const;
  std::shared_ptr<detail::vfs_state> routes_;
  std::shared_ptr<const detail::vfs_routes> snapshot_;
  file_service_options options_;
};
/** @brief 统一文件服务，拥有挂载及完成配置；在途操作与捕获批次可独立于服务继续清理。
 * @note 挂载更新可与请求捕获并发；后端线程约束独立。析构不阻塞，借用执行环境须覆盖真实清理。
 */
class CAORS_API file_service final : public file_batch {
public:
  explicit file_service(file_service_options options = {});
  file_service(const file_service&) = delete;
  file_service& operator=(const file_service&) = delete;
  file_service(file_service&&) = delete;
  file_service& operator=(file_service&&) = delete;
  /** @brief 挂载拥有式后端及宿主凭据；重复前缀失败。 */
  [[nodiscard]] error mount(uri_view prefix, std::shared_ptr<backend> target,
                            path_view target_prefix = {}, std::shared_ptr<void> lifetime = {});
  /** @brief 移除挂载，不影响已发请求或批次。 */
  [[nodiscard]] bool unmount(uri_view prefix);
  /** @brief 清空挂载，不等待在途操作。 */
  void clear();
  /** @brief 查询活跃挂载数。 */
  [[nodiscard]] std::size_t mount_count() const noexcept;
  /** @brief 捕获固定挂载与当前调度配置；批次可独立于服务使用。 */
  [[nodiscard]] result<file_batch> snapshot() const;
};
}
#endif
