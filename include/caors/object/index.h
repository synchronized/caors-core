// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_INDEX_H_
#define CAORS_OBJECT_INDEX_H_

#include "caors/core/containers.h"
#include "caors/object/id.h"

#include <cstddef>

namespace caors::objects {

/**
 * @brief 持久对象 GUID 和运行期对象 id 的双向索引。
 *
 * `object_index` 不拥有对象，也不负责生成 GUID。它只保存加载期或运行期建立的
 * `object_guid <-> object_id` 映射，用于把跨存档稳定的对象身份解析为当前进程里的运行期对象 id。
 * 该类型不提供内部线程同步；跨线程访问时调用方需要自行加锁。
 */
class CAORS_API object_index final {
public:
  explicit object_index(memory_resource* resource = nullptr);
  ~object_index();

  object_index(const object_index&) = delete;
  object_index& operator=(const object_index&) = delete;
  object_index(object_index&&) = delete;
  object_index& operator=(object_index&&) = delete;

  /** @brief 查询索引使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询当前绑定数量。 */
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /** @brief 判断持久 GUID 是否已经绑定。 */
  [[nodiscard]] bool contains(object_guid guid) const;

  /** @brief 判断运行期 id 是否已经绑定。 */
  [[nodiscard]] bool contains(object_id id) const;

  /**
   * @brief 绑定持久 GUID 和运行期 id。
   *
   * nil GUID 或无效 id 会失败。同一对 GUID/id 重复绑定会视为成功；同一个 GUID 或 id 绑定到另一个
   * 值会失败。
   */
  bool bind(object_guid guid, object_id id);

  /** @brief 按持久 GUID 解绑。 */
  bool unbind(object_guid guid) noexcept;

  /** @brief 按运行期 id 解绑。 */
  bool unbind(object_id id) noexcept;

  /** @brief 清空全部绑定。 */
  void clear() noexcept;

  /** @brief 通过持久 GUID 查找运行期对象 id。未命中时返回无效 id。 */
  [[nodiscard]] object_id find(object_guid guid) const;

  /** @brief 通过运行期对象 id 查找持久 GUID。未命中时返回 nil GUID。 */
  [[nodiscard]] object_guid guid_of(object_id id) const;

private:
  using guid_map = core::hash_map<object_guid, object_id, object_guid_hash>;
  using id_map = core::hash_map<object_id, object_guid, object_id_hash>;

  memory_resource* resource_ = nullptr;
  guid_map ids_by_guid_;
  id_map guids_by_id_;
};

} // namespace caors::objects

#endif
