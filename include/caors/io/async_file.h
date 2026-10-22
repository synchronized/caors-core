// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_ASYNC_FILE_H_
#define CAORS_IO_ASYNC_FILE_H_

#include "caors/io/async_request.h"
#include "caors/io/options.h"

#include <limits>
#include <span>

namespace caors::io {
/**
 * @brief 拥有式异步随机文件协议；宿主可用 libuv 等实现，不向 CAORS 暴露其类型。
 * @note 接受的读必须拥有真实操作缓冲；多个 offset 互不干扰。关闭后拒绝新读，等待在途清理，
 * 仅关闭一次，重复关闭订阅同一关闭结果。析构请求关闭但不得阻塞宿主 loop。
 */
class CAORS_API async_file {
public:
  virtual ~async_file();
  /** @brief 只读时固定为打开长度；可写实现须反映本句柄成功写入的扩展。 */
  [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;
  /** @brief 解析来源的进程内实例标记；由打开编排保留，直接创建的文件可为 0。 */
  [[nodiscard]] virtual std::uint64_t source_id() const noexcept { return 0; }
  /** @brief 单个范围的后端上限；须在分配前验证。 */
  [[nodiscard]] virtual std::uint64_t max_read_size() const noexcept {
    return (std::numeric_limits<std::size_t>::max)();
  }
  /** @brief 启动精确范围读取，读满才成功；默认实现不调用 read_all。 */
  [[nodiscard]] virtual io_error start_read(std::uint64_t offset, std::uint64_t length,
                                            memory_resource* resource,
                                            io_completion<byte_buffer> completion) = 0;
  /** @brief 精确范围写入，消费拥有式缓冲；默认不支持，关闭必须等待已接受写入清理。 */
  [[nodiscard]] virtual io_error start_write(std::uint64_t offset, byte_buffer bytes,
                                             io_completion<std::uint64_t> completion);
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
  /** @brief 可选随机写入，允许短写；默认不支持，由 worker 适配累计到写满。 */
  [[nodiscard]] virtual io_result<std::size_t> write_at(std::uint64_t offset,
                                                        std::span<const std::byte> bytes);
  /** @brief 显式关闭，错误可查询；析构仅作 RAII 兜底。 */
  [[nodiscard]] virtual io_error close() = 0;
};
/** @brief 创建占用 worker 的同步适配文件；seek/read 在内部串行，不宣称内核异步或并行读。 */
[[nodiscard]] CAORS_API std::shared_ptr<async_file>
make_worker_file(std::shared_ptr<random_access_reader> reader, tasks::executor worker,
                 std::shared_ptr<void> lifetime = {});
} // namespace caors::io
#endif
