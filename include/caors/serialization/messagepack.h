// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_MESSAGEPACK_H_
#define CAORS_SERIALIZATION_MESSAGEPACK_H_

#include "caors/io/types.h"
#include "caors/serialization/types.h"
#include "caors/serialization/writer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace caors::serialization {

/** @brief MessagePack DOM 解析限制。 */
struct messagepack_options {
  /** @brief 允许的最大容器嵌套深度；0 表示不限制。 */
  std::uint32_t max_parse_depth = 64;
  /** @brief 单个字符串允许的最大字节数；0 表示不限制。 */
  std::size_t max_string_size = 64U * 1024U * 1024U;
  /** @brief 单个数组或 map 允许的最大元素数量；0 表示不限制。 */
  std::size_t max_container_size = 16U * 1024U * 1024U;
};

/** @brief 将通用 serialization writer 事件编码为标准 MessagePack。 */
class CAORS_API messagepack_writer final : public writer {
public:
  messagepack_writer();
  explicit messagepack_writer(io::byte_buffer& output);
  ~messagepack_writer() override;

  [[nodiscard]] format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const type_schema& type) override;
  [[nodiscard]] error begin_field(const field_schema& field) override;
  [[nodiscard]] error begin_array(std::size_t size) override;
  [[nodiscard]] error begin_element(std::size_t index) override;
  [[nodiscard]] error begin_dictionary(std::size_t size) override;
  [[nodiscard]] error begin_entry(std::size_t index) override;
  [[nodiscard]] error begin_key() override;
  [[nodiscard]] error end_key() override;
  [[nodiscard]] error begin_value() override;
  [[nodiscard]] error end_value() override;
  [[nodiscard]] error end_entry() override;
  [[nodiscard]] error end_dictionary() override;
  [[nodiscard]] error write_bool(bool value) override;
  [[nodiscard]] error write_i64(std::int64_t value) override;
  [[nodiscard]] error write_u64(std::uint64_t value) override;
  [[nodiscard]] error write_f64(double value) override;
  [[nodiscard]] error write_string(std::string_view value) override;
  [[nodiscard]] error write_null() override;
  [[nodiscard]] error end_element() override;
  [[nodiscard]] error end_array() override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

  /** @brief 返回当前根值编码后的字节。 */
  [[nodiscard]] const io::byte_buffer& bytes() const noexcept;
  /** @brief 清空输出和未完成的 writer 状态。 */
  void reset();

private:
  class implementation;
  std::unique_ptr<implementation> implementation_;
};

/** @brief 将 DOM 值编码为标准 MessagePack 字节。 */
[[nodiscard]] CAORS_API io::byte_buffer_result
encode_messagepack(const variant& value, memory_resource* resource = nullptr);

/**
 * @brief 从一个完整 MessagePack 根值解析 DOM。
 *
 * 字符串 key 的 map 解析为 object，其他 map 解析为 dictionary。bin、ext、保留标记和根值后的
 * 尾随数据在当前版本中返回 `errc::unsupported` 或 `errc::invalid_argument`。
 */
[[nodiscard]] CAORS_API variant_result parse_messagepack(std::span<const std::byte> bytes,
                                                         messagepack_options options = {});

} // namespace caors::serialization

#endif
