// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_INTERNAL_IO_FILE_REQUESTS_H_
#define CAORS_INTERNAL_IO_FILE_REQUESTS_H_
#include "caors/io/async_file.h"
#include "caors/io/backend.h"
#include "caors/io/route_snapshot.h"
// 仅供库内部编排与内部单元测试，不安装为公共 API。
namespace caors::io::detail {
/** @brief 验证整数、打开长度、后端与请求预算，成功后通过 completion 通知；绝不隐式全文件回退。 */
[[nodiscard]] CAORS_API io_result<io_request<byte_buffer>>
read_file(std::shared_ptr<async_file> file, std::uint64_t offset, std::uint64_t length,
          std::uint64_t max_bytes, tasks::executor completion,
          std::function<void(const io_result<byte_buffer>&)> callback = {},
          memory_resource* resource = nullptr);
/** @brief 有明确总结果预算的整文件读取，沿用精确范围协议。 */
[[nodiscard]] CAORS_API io_result<io_request<byte_buffer>>
read_file_bounded(std::shared_ptr<async_file> file, std::uint64_t max_bytes,
                  tasks::executor completion,
                  std::function<void(const io_result<byte_buffer>&)> callback = {},
                  memory_resource* resource = nullptr);
/** @brief 显式异步关闭；结果与 executor 拒绝均可通过返回句柄查询。 */
[[nodiscard]] CAORS_API io_result<io_request<close_result>>
close_file(std::shared_ptr<async_file> file, tasks::executor completion,
           std::function<void(const io_result<close_result>&)> callback = {});
/** @brief 原生异步打开；未声明支持的后端直接返回 unsupported，无同步回退。 */
[[nodiscard]] CAORS_API io_result<io_request<std::shared_ptr<async_file>>>
open_file(resolved_file file, tasks::executor completion,
          std::function<void(const io_result<std::shared_ptr<async_file>>&)> callback = {},
          memory_resource* resource = nullptr, open_options options = {});
/** @brief 直接打开新后端，不经过旧文件系统或同步适配。 */
[[nodiscard]] CAORS_API io_result<io_request<std::shared_ptr<async_file>>>
open_file(backend_location file, tasks::executor completion,
          std::function<void(const io_result<std::shared_ptr<async_file>>&)> callback = {},
          memory_resource* resource = nullptr, open_options options = {});
/** @brief 显式 worker 打开适配；只调用 open_random_read，不使用默认 open_read/read_all。 */
[[nodiscard]] CAORS_API io_result<io_request<std::shared_ptr<async_file>>> open_file_on_worker(
    resolved_file file, tasks::executor worker, tasks::executor completion,
    std::function<void(const io_result<std::shared_ptr<async_file>>&)> callback = {},
    memory_resource* resource = nullptr);
} // namespace caors::io::detail
#endif
