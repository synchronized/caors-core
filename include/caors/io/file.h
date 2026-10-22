// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_FILE_H_
#define CAORS_IO_FILE_H_
#include "caors/io/request.h"
#include <cstdint>

namespace caors::io {
class async_file;
/** @brief 精确范围读取；先检查预算再分配，短读为失败。 */
struct file_read_options {
  std::uint64_t offset = 0;
  std::uint64_t length = 0;
};
class io_context;
/** @brief 拥有式只读文件句柄；复制共享文件，显式关闭作用于全部副本，析构非阻塞兜底。 */
class CAORS_API file {
public:
  file() = default;
  /** @brief 是否持有实际文件；不表示尚未关闭。 */
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(handle_); }
  /** @brief 打开时的固定长度，空句柄为零。 */
  [[nodiscard]] std::uint64_t size() const noexcept;
  /** @brief 在既定预算内精确读取范围；结果拥有缓冲区，通知使用打开时的完成执行器。 */
  [[nodiscard]] request<byte_buffer>
  read(file_read_options options,
       std::function<void(const io_result<byte_buffer>&)> callback = {}) const;
  /** @brief 拒绝新操作并等待已接受操作清理；共享副本重复关闭订阅同一结果。 */
  [[nodiscard]] request<close_result>
  close(std::function<void(const io_result<close_result>&)> callback = {}) const;

private:
  friend class io_context;
  file(std::shared_ptr<async_file> handle, tasks::executor executor, memory_resource* resource,
       std::uint64_t budget)
      : handle_(std::move(handle)), executor_(std::move(executor)), resource_(resource),
        budget_(budget) {}
  std::shared_ptr<async_file> handle_;
  tasks::executor executor_;
  memory_resource* resource_ = nullptr;
  std::uint64_t budget_ = 0;
};
} // namespace caors::io
#endif
