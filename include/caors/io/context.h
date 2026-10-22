// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_CONTEXT_H_
#define CAORS_IO_CONTEXT_H_
#include "caors/io/file.h"
#include "caors/io/options.h"
#include "caors/io/vfs.h"
namespace caors::io {
/** @brief 用户通知调度及借用的字节 allocator；两者必须覆盖实际操作与结果寿命。 */
struct io_context_options {
  tasks::executor completion;
  memory_resource* resource = nullptr;
};
/**
 * @brief 文件访问入口，统一单层请求、结果预算和完成调度；不拥有线程池或 allocator。
 * @note vfs 构造借用配置用于后续调用，已发请求拥有路由；快照构造固定整个批次路由。
 */
class CAORS_API io_context {
public:
  /** @brief 借用活跃路由表，每次发起时捕获稳定路由。 */
  io_context(const vfs& routes, io_context_options options);
  io_context(vfs&&, io_context_options) = delete;
  /** @brief 拥有批次快照，不再访问原 vfs。 */
  io_context(vfs_snapshot snapshot, io_context_options options);
  /** @brief 异步打开已有文件，返回单层请求；错误通过同一调度交付或句柄查询。 */
  [[nodiscard]] request<file> open(path_view location, open_options options = {},
                                   std::function<void(const io_result<file>&)> callback = {}) const;
  /** @brief 整体读取并关闭文件；关闭失败也报告失败，取消不会阻塞等待真实清理。 */
  [[nodiscard]] request<byte_buffer>
  read(path_view location, read_options options = {},
       std::function<void(const io_result<byte_buffer>&)> callback = {}) const;

private:
  [[nodiscard]] result<resolved_file> resolve(path_view location) const;
  const vfs* routes_ = nullptr;
  vfs_snapshot snapshot_;
  io_context_options options_;
};
} // namespace caors::io
#endif
