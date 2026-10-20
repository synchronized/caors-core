// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_IO_CHUNKED_FILE_H_
#define CAORS_IO_CHUNKED_FILE_H_

#include "caors/io/async_file.h"

namespace caors::io {
/** @brief 分块总处理预算和单块上限；调用者必须提供非零 chunk_bytes。 */
struct chunk_read_options {
  std::uint64_t max_bytes = 0;
  std::uint64_t chunk_bytes = 0;
};
/** @brief 已成功消费的字节数，不包含排队或仅完成读取的块。 */
struct chunk_summary {
  std::uint64_t bytes = 0;
};
/** @brief 在指定 executor 上同步消费一块；span 仅在调用期间有效，错误终止后续读取。 */
using chunk_consumer = std::function<io_error(std::uint64_t offset, std::span<const std::byte>)>;
/** @brief 完成/总字节进度；消费模式统计已消费量，聚合模式统计已并入结果量。 */
using chunk_progress = std::function<void(std::uint64_t completed, std::uint64_t total)>;
/**
 * @brief 串行分块聚合为完整缓冲；结果内存为文件大小，另有常数个临时块。
 * @note 持有文件但不关闭它。通知可早于返回；resource 须覆盖请求、结果及实际清理。
 */
[[nodiscard]] CAORS_API io_result<io_request<byte_buffer>>
read_all_chunked(std::shared_ptr<async_file> file, chunk_read_options options,
                 tasks::executor completion,
                 std::function<void(const io_result<byte_buffer>&)> callback = {},
                 chunk_progress progress = {}, memory_resource* resource = nullptr);
/**
 * @brief 逐块同步消费，最多一个未完成的范围读取，回调返回后才启动下一块。
 * @note 不累计整文件；完成及消费都在 completion 上执行，拒绝/丢弃可查询失败。
 * 取消不能抢占正在执行的消费者，副作用不回滚；文件由调用方负责关闭。
 */
[[nodiscard]] CAORS_API io_result<io_request<chunk_summary>>
consume_chunks(std::shared_ptr<async_file> file, chunk_read_options options,
               tasks::executor completion, chunk_consumer consumer,
               std::function<void(const io_result<chunk_summary>&)> callback = {},
               chunk_progress progress = {}, memory_resource* resource = nullptr);
} // namespace caors::io
#endif
