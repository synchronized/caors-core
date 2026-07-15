// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_SERIALIZATION_H_
#define CAORS_RESOURCES_SERIALIZATION_H_

#include "caors/resources/cache.h"
#include "caors/resources/dependency_graph.h"
#include "caors/resources/manager.h"
#include "caors/resources/ref.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/types.h"

#include <string>
#include <string_view>

namespace caors::resources {

using resource_ref_resolution = serialization::external_ref_resolution;
using resource_ref_resolver_ops = serialization::external_ref_resolver_ops;
using resource_dependency_collector_ops = serialization::external_ref_collector_ops;

/** @brief 资源引用在通用外部引用系统中的 domain。 */
[[nodiscard]] inline string_name resource_ref_domain() { return string_name{"resource"}; }

/** @brief 构造资源引用对应的通用外部引用。 */
[[nodiscard]] inline serialization::external_ref make_resource_external_ref(resource_guid guid,
                                                                            type_id type = {}) {
  return serialization::external_ref{resource_ref_domain(), guid.value, type};
}

/** @brief 序列化过程中发现的资源依赖。 */
struct resource_dependency {
  resources::resource_guid guid{};
  type_id type{};
};

/** @brief 保存一次序列化遍历收集到的资源依赖。 */
class resource_dependency_list {
public:
  explicit resource_dependency_list(memory::memory_resource* resource = nullptr)
      : dependencies_(allocator<resource_dependency>{resource}) {}

  void clear() noexcept { dependencies_.clear(); }
  [[nodiscard]] bool empty() const noexcept { return dependencies_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return dependencies_.size(); }
  [[nodiscard]] const vector<resource_dependency>& entries() const noexcept {
    return dependencies_;
  }

  void collect(resources::resource_guid guid, type_id type) {
    if (!guid) {
      return;
    }

    dependencies_.push_back(resource_dependency{guid, type});
  }

  void collect(const serialization::external_ref& ref) {
    if (ref.domain != resource_ref_domain() || !ref.id) {
      return;
    }

    collect(resource_guid{ref.id}, ref.type);
  }

  [[nodiscard]] resource_dependency_collector_ops collector() noexcept {
    return resource_dependency_collector_ops{&resource_dependency_list::collect_dependency, this};
  }

private:
  static void collect_dependency(const serialization::external_ref& ref, void* user_data) {
    auto* list = static_cast<resource_dependency_list*>(user_data);
    if (list != nullptr) {
      list->collect(ref);
    }
  }

  vector<resource_dependency> dependencies_;
};

/**
 * @brief 把依赖收集结果追加写入资源依赖图。
 *
 * `resource` 是拥有这些依赖的资源 GUID。依赖条目中的 `type` 只用于上层资源数据库或调试诊断；
 * 资源依赖图当前只保存 GUID 关系。返回值是新增的依赖边数量。
 */
[[nodiscard]] inline std::size_t
add_resource_dependencies(resources::dependency_graph& graph, resources::resource_guid resource,
                          const resource_dependency_list& dependencies) {
  if (!resource) {
    return 0;
  }

  std::size_t added = 0;
  for (const resource_dependency& dependency : dependencies.entries()) {
    const bool exists = graph.contains(resource, dependency.guid);
    if (graph.add(resource, dependency.guid) && !exists) {
      ++added;
    }
  }

  return added;
}

/**
 * @brief 用依赖收集结果替换资源依赖图中某个资源的直接依赖。
 *
 * 该函数适合资源重新导入、重新序列化或热重载后刷新资源数据库中的依赖关系。返回值是替换后新增的
 * 直接依赖边数量。
 */
[[nodiscard]] inline std::size_t
replace_resource_dependencies(resources::dependency_graph& graph, resources::resource_guid resource,
                              const resource_dependency_list& dependencies) {
  if (!resource) {
    return 0;
  }

  graph.clear_dependencies(resource);
  return add_resource_dependencies(graph, resource, dependencies);
}

namespace detail {

[[nodiscard]] inline void* manager_resource_resolve(const serialization::external_ref& ref,
                                                    void* user_data) noexcept {
  if (ref.domain != resource_ref_domain()) {
    return nullptr;
  }

  auto* manager = static_cast<resources::manager*>(user_data);
  return manager != nullptr ? manager->find(resource_guid{ref.id}) : nullptr;
}

template <typename T>
[[nodiscard]] void* cache_resource_resolve(const serialization::external_ref& ref,
                                           void* user_data) noexcept {
  if (ref.domain != resource_ref_domain()) {
    return nullptr;
  }

  auto* cache = static_cast<resources::cache<T>*>(user_data);
  return cache != nullptr ? cache->find(resource_guid{ref.id}) : nullptr;
}

} // namespace detail

/** @brief 使用资源索引创建资源引用 resolver。 */
[[nodiscard]] inline resource_ref_resolver_ops
make_resource_resolver(resources::manager& manager) noexcept {
  return resource_ref_resolver_ops{&detail::manager_resource_resolve, &manager};
}

/** @brief 使用资源缓存创建资源引用 resolver。 */
template <typename T>
[[nodiscard]] resource_ref_resolver_ops
make_resource_resolver(resources::cache<T>& cache) noexcept {
  return resource_ref_resolver_ops{&detail::cache_resource_resolve<T>, &cache};
}

} // namespace caors::resources

namespace caors::serialization::detail {

/** @brief 按字符串 GUID 写出资源标识。 */
[[nodiscard]] inline error write_resource_guid(writer& writer, resources::resource_guid guid) {
  const std::string text = guid.string();
  return writer.write_string(std::string_view{text.data(), text.size()});
}

/** @brief 从字符串 GUID 读入资源标识。 */
[[nodiscard]] inline error read_resource_guid(reader& reader, resources::resource_guid& guid) {
  string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  const auto parsed = resources::resource_guid::parse(std::string_view{text.data(), text.size()});
  if (!parsed.has_value()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  guid = *parsed;
  return {};
}

/** @brief 构造资源标识的序列化中间值。 */
[[nodiscard]] inline variant make_resource_guid_value(resources::resource_guid guid) {
  const std::string text = guid.string();
  return variant::from(std::string_view{text.data(), text.size()});
}

/** @brief 查询资源类型对应的运行期类型 id；类型未注册时返回 invalid。 */
template <typename T> [[nodiscard]] inline type_id resource_type_id() noexcept {
  const meta::type type = meta::find<T>();
  return type.registered() ? type.id() : type_id::invalid();
}

/** @brief 向上下文外部引用收集器记录一个资源引用。 */
inline void collect_resource_dependency(const serialization::context& context,
                                        resources::resource_guid guid, type_id type) {
  const external_ref_collector_ops* collector = context.effective_external_ref_collector();
  if (collector != nullptr && collector->can_collect() && guid) {
    (*collector)(resources::make_resource_external_ref(guid, type));
  }
}

/** @brief 按上下文策略解析资源引用。 */
template <typename T>
[[nodiscard]] inline error resolve_resource_reference(resources::ref<T>& value,
                                                      const serialization::context& context) {
  const external_ref_resolution resolution = context.effective_external_ref_resolution();
  if (!value.has_guid() || resolution == external_ref_resolution::keep_unresolved) {
    return {};
  }

  const external_ref_resolver_ops* resolver = context.effective_external_ref_resolver();
  if (resolver == nullptr || !resolver->can_resolve()) {
    return resolution == external_ref_resolution::require_resolved
               ? error{error_domain::serialization, errc::invalid_argument}
               : error{};
  }

  void* resolved = (*resolver)(
      resources::make_resource_external_ref(value.guid(), detail::resource_type_id<T>()));
  auto* resource = static_cast<resources::resource*>(resolved);
  if (resource == nullptr) {
    value.unresolve();
    return resolution == external_ref_resolution::require_resolved
               ? error{error_domain::resources, errc::not_found}
               : error{};
  }

  T* typed = dynamic_cast<T*>(resource);
  if (typed == nullptr) {
    value.unresolve();
    return {error_domain::resources, errc::invalid_type};
  }

  if (!value.resolve(typed)) {
    return {error_domain::resources, errc::inconsistent_type};
  }

  return {};
}

} // namespace caors::serialization::detail

namespace caors::serialization {

/** @brief `resource_guid` 的字符串 GUID 序列化 codec。 */
template <> struct codec_traits<resources::resource_guid> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, resources::resource_guid value) {
    return detail::write_resource_guid(writer, value);
  }

  [[nodiscard]] static error read(reader& reader, resources::resource_guid& value) {
    return detail::read_resource_guid(reader, value);
  }

  [[nodiscard]] static variant_result make_value(resources::resource_guid value) {
    return {{}, detail::make_resource_guid_value(value)};
  }
};

/** @brief 资源引用按稳定 `resource_guid` 序列化，不序列化运行期资源指针。 */
template <typename T> struct codec_traits<resources::ref<T>> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const resources::ref<T>& value) {
    if (!value.has_guid()) {
      return writer.write_null();
    }

    return detail::write_resource_guid(writer, value.guid());
  }

  [[nodiscard]] static error write(const serialization::context& context, writer& writer,
                                   const resources::ref<T>& value) {
    if (value.has_guid()) {
      detail::collect_resource_dependency(context, value.guid(), detail::resource_type_id<T>());
    }

    return write(writer, value);
  }

  [[nodiscard]] static error read(reader& reader, resources::ref<T>& value) {
    if (reader.is_null()) {
      value.reset();
      return {};
    }

    resources::resource_guid guid{};
    const error status = detail::read_resource_guid(reader, guid);
    if (status) {
      return status;
    }

    value.reset(guid);
    return {};
  }

  [[nodiscard]] static error read(const serialization::context& context, reader& reader,
                                  resources::ref<T>& value) {
    const error status = read(reader, value);
    if (status || !value.has_guid()) {
      return status;
    }

    detail::collect_resource_dependency(context, value.guid(), detail::resource_type_id<T>());
    return detail::resolve_resource_reference(value, context);
  }

  [[nodiscard]] static variant_result make_value(const resources::ref<T>& value) {
    if (!value.has_guid()) {
      return {{}, variant::null()};
    }

    return {{}, detail::make_resource_guid_value(value.guid())};
  }

  [[nodiscard]] static variant_result make_value(const serialization::context& context,
                                                 const resources::ref<T>& value) {
    if (value.has_guid()) {
      detail::collect_resource_dependency(context, value.guid(), detail::resource_type_id<T>());
    }

    return make_value(value);
  }
};

} // namespace caors::serialization

namespace caors {

using resources::add_resource_dependencies;
using resources::make_resource_resolver;
using resources::replace_resource_dependencies;
using resource_dependency = resources::resource_dependency;
using resource_dependency_collector_ops = resources::resource_dependency_collector_ops;
using resource_dependency_list = resources::resource_dependency_list;
using resource_ref_resolution = resources::resource_ref_resolution;
using resource_ref_resolver_ops = resources::resource_ref_resolver_ops;

} // namespace caors

#endif
