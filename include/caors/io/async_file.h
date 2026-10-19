// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_ASYNC_FILE_H_
#define CAORS_IO_ASYNC_FILE_H_

#include "caors/io/async_request.h"
#include "caors/io/route_snapshot.h"

#include <limits>
#include <span>

namespace caors::io {
/** @brief 显式关闭成功时的结果。 */
struct close_result {};
/**
 * @brief 拥有式异步随机读取文件协议；宿主可用 libuv 等实现，不向 CAORS 暴露其类型。
 * @note 接受的读必须拥有真实操作缓冲；多个 offset 互不干扰。关闭后拒绝新读，等待在途清理，
 * 仅关闭一次，重复关闭订阅同一关闭结果。析构请求关闭但不得阻塞宿主 loop。
 */
class CAORS_API async_file {
public:
  virtual ~async_file();
  /** @brief 打开时固定的长度；文件增长不扩大此范围。 */
  [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;
  /** @brief 解析来源的进程内实例标记；由 request_open 系列保留，直接创建的文件可为 0。 */
  [[nodiscard]] virtual std::uint64_t source_id() const noexcept { return 0; }
  /** @brief 单个范围的后端上限；须在分配前验证。 */
  [[nodiscard]] virtual std::uint64_t max_read_size() const noexcept {
    return (std::numeric_limits<std::size_t>::max)();
  }
  /** @brief 启动精确范围读取，读满才成功；默认实现不调用 read_all。 */
  [[nodiscard]] virtual io_error start_read(std::uint64_t offset, std::uint64_t length,
                                            memory_resource* resource,
                                            io_completion<byte_buffer> completion) = 0;
  /** @brief 发起或订阅显式关闭；宿主必须维持 loop 至全部在途操作及关闭完成。 */
  [[nodiscard]] virtual io_error start_close(io_completion<close_result> completion) = 0;
};
/** @brief 显式 worker 适配所需的同步随机读取接口；read_at 可以短读，不得隐式读取整个文件。 */
class CAORS_API random_access_reader {
public:
  virtual ~random_access_reader();
  [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;
  [[nodiscard]] virtual io_result<std::size_t> read_at(std::uint64_t offset,
                                                       std::span<std::byte> buffer) = 0;
  /** @brief 显式关闭，错误可查询；析构仅作 RAII 兜底。 */
  [[nodiscard]] virtual io_error close() = 0;
};
/** @brief 创建占用 worker 的同步适配文件；seek/read 在内部串行，不宣称内核异步或并行读。 */
[[nodiscard]] CAORS_API std::shared_ptr<async_file>
make_worker_file(std::shared_ptr<random_access_reader> reader, tasks::executor worker,
                 std::shared_ptr<void> lifetime = {});
/** @brief 验证整数、打开长度、后端与请求预算，成功后通过 completion 通知；绝不隐式全文件回退。 */
[[nodiscard]] CAORS_API io_result<io_request<byte_buffer>>
request_read(std::shared_ptr<async_file> file, std::uint64_t offset, std::uint64_t length,
             std::uint64_t max_bytes, tasks::executor completion,
             std::function<void(const io_result<byte_buffer>&)> callback = {},
             memory_resource* resource = nullptr);
/** @brief 有明确总结果预算的整文件读取，沿用精确范围协议。 */
[[nodiscard]] CAORS_API io_result<io_request<byte_buffer>>
read_all_bounded(std::shared_ptr<async_file> file, std::uint64_t max_bytes,
                 tasks::executor completion,
                 std::function<void(const io_result<byte_buffer>&)> callback = {},
                 memory_resource* resource = nullptr);
/** @brief 显式异步关闭；结果与 executor 拒绝均可通过返回句柄查询。 */
[[nodiscard]] CAORS_API io_result<io_request<close_result>>
request_close(std::shared_ptr<async_file> file, tasks::executor completion,
              std::function<void(const io_result<close_result>&)> callback = {});
/** @brief 原生异步打开；未声明支持的后端直接返回 unsupported，无同步回退。 */
[[nodiscard]] CAORS_API io_result<io_request<std::shared_ptr<async_file>>>
request_open(resolved_file file, tasks::executor completion,
             std::function<void(const io_result<std::shared_ptr<async_file>>&)> callback = {},
             memory_resource* resource = nullptr);
/** @brief 显式 worker 打开适配；只调用 open_random_read，不使用默认 open_read/read_all。 */
[[nodiscard]] CAORS_API io_result<io_request<std::shared_ptr<async_file>>> request_open_on_worker(
    resolved_file file, tasks::executor worker, tasks::executor completion,
    std::function<void(const io_result<std::shared_ptr<async_file>>&)> callback = {},
    memory_resource* resource = nullptr);
} // namespace caors::io
#endif
