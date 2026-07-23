// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_TYPES_H_
#define CAORS_SERIALIZATION_TYPES_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any.h"
#include "caors/meta/value/variant.h"
#include "caors/result.h"
#include "caors/serialization/external_ref.h"
#include "caors/serialization/type_schema.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace caors::serialization {

struct context;

/** @brief 按运行期类型构造和销毁实例的函数表。 */
struct polymorphic_factory_ops {
  using create_fn = void* (*)(type_id type, memory::memory_resource* resource, void* user_data);
  using destroy_fn = void (*)(void* instance, void* user_data) noexcept;

  create_fn create = nullptr;
  destroy_fn destroy = nullptr;
  void* user_data = nullptr;

  [[nodiscard]] constexpr bool can_create() const noexcept { return create != nullptr; }
  [[nodiscard]] constexpr bool can_destroy() const noexcept { return destroy != nullptr; }
  [[nodiscard]] constexpr bool valid() const noexcept { return can_create() && can_destroy(); }

  [[nodiscard]] void* create_type(type_id type, memory::memory_resource* resource) const {
    return create != nullptr ? create(type, resource, user_data) : nullptr;
  }

  void destroy_type(void* instance) const noexcept {
    if (destroy != nullptr && instance != nullptr) {
      destroy(instance, user_data);
    }
  }
};

inline constexpr std::size_t construction_request_no_index = static_cast<std::size_t>(-1);

/** @brief 序列化读入时需要新建运行期值的位置。 */
enum class construction_site {
  value,
  field,
  indirect_value,
  sequence_element,
  set_element,
  associative_key,
  associative_value,
  pair_first,
  pair_second,
  tuple_element,
  variant_alternative,
  polymorphic_value,
};

/** @brief 序列化读入时传给自定义构造策略的上下文信息。 */
struct construction_request {
  meta::type type{};
  meta::type owner_type{};
  string_name field_name{};
  construction_site site = construction_site::value;
  std::size_t index = construction_request_no_index;
};

/** @brief 自定义构造策略返回的运行期对象和匹配销毁函数。 */
struct construction_result {
  void* value = nullptr;
  meta::resource_destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return value != nullptr && destroy != nullptr;
  }
};

/**
 * @brief 多态对象和值 wrapper 的 envelope 输出配置。
 *
 * 字段名保持空值时使用默认 `$type`、`$value` 和 `$type_name`。`$type` 始终写入稳定
 * `type_guid`，`$type_name` 只用于调试输出，默认不写出，读入时也不作为类型身份依据。
 */
struct polymorphic_options {
  string_name type_field{};
  string_name value_field{};
  string_name type_name_field{};
  bool write_type_name = false;
};

/** @brief JSON 对象遇到重复 key 时的解析策略。 */
enum class json_duplicate_key_policy {
  /** @brief 保留所有同名成员，维持 DOM 原始形态。 */
  keep_all,
  /** @brief 保留第一项，后续同名成员会被忽略。 */
  use_first,
  /** @brief 保留最后一项，后续同名成员会覆盖前项。 */
  use_last,
  /** @brief 遇到重复 key 直接解析失败。 */
  reject,
};

/** @brief JSON writer / reader 的格式层选项。 */
struct json_options {
  /** @brief 是否输出换行和缩进；默认输出紧凑 JSON。 */
  bool pretty = false;
  /** @brief `pretty` 开启时每一层使用的空格数。 */
  std::uint32_t indent = 2;
  /** @brief parser 允许的最大对象 / 数组嵌套深度；0 表示不限制。 */
  std::uint32_t max_parse_depth = 64;
  /** @brief parser 是否允许对象和数组末尾多一个逗号。 */
  bool allow_trailing_commas = false;
  /** @brief parser 遇到对象重复 key 时的处理策略。 */
  json_duplicate_key_policy duplicate_key_policy = json_duplicate_key_policy::keep_all;
};

/** @brief 字段版本过滤相关的序列化语义选项。 */
struct field_filter_options {
  /**
   * @brief 当前读写使用的 schema version。
   *
   * `0` 表示不启用字段版本过滤。传入非 0 版本时，调度层只处理当前版本可见的字段。
   */
  std::uint32_t schema_version = 0;
};

/** @brief 读入对象时遇到 schema 未声明字段的处理策略。 */
enum class unknown_field_policy {
  /** @brief 忽略未知字段，保持默认兼容行为。 */
  ignore,
  /** @brief 把未知字段复制到上下文状态提供的保留列表中。 */
  preserve,
  /** @brief 遇到未知字段立即返回 `errc::invalid_field`。 */
  reject,
};

/** @brief 读入对象时遇到 schema 已声明但输入缺失字段的处理策略。 */
enum class missing_field_policy {
  /** @brief 缺失字段返回 `errc::not_found`，保持普通反序列化默认行为。 */
  reject,
  /** @brief 缺失字段保留目标对象当前值，适合配置对象先应用默认值再局部覆盖。 */
  keep_existing,
  /** @brief 缺失字段优先写入 `meta::property_schema::default_value`，没有默认值时保留当前值。 */
  apply_default,
};

/** @brief schema 迁移函数收到的上下文信息。 */
struct schema_migration_request {
  /** @brief 目标 C++ 类型。 */
  meta::type type{};
  /** @brief 输入数据声明的 schema version；0 表示调用方未声明。 */
  std::uint32_t source_version = 0;
  /** @brief 本次读入希望使用的目标 schema version；0 表示不启用版本过滤。 */
  std::uint32_t target_version = 0;
};

/** @brief DOM 读入前的 schema 迁移函数表。 */
struct schema_migration_ops {
  using migrate_fn = error (*)(const schema_migration_request& request, variant& value,
                               void* user_data);

  migrate_fn migrate = nullptr;
  void* user_data = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept { return migrate != nullptr; }

  [[nodiscard]] error operator()(const schema_migration_request& request, variant& value) const {
    return migrate != nullptr ? migrate(request, value, user_data) : error{};
  }
};

/** @brief schema 兼容性相关的序列化语义选项。 */
struct schema_options {
  unknown_field_policy unknown_fields = unknown_field_policy::ignore;
  missing_field_policy missing_fields = missing_field_policy::reject;
  /** @brief 输入数据声明的 schema version；用于迁移函数判断来源版本。 */
  std::uint32_t source_version = 0;
  /** @brief 可选 DOM 迁移函数；只在 DOM 读入或 JSON 需要先解析为 DOM 时调用。 */
  const schema_migration_ops* migration = nullptr;
};

/**
 * @brief `std::string_view` 反序列化使用的多段字符串存储。
 *
 * `std::string_view` 不拥有字符数据。反序列化读回 view 字段时，调用方应让该 storage 的生命周期
 * 长于目标对象中所有读回的 view。每次 `store()` 都会追加一段独立字符串，避免多个 view
 * 字段互相覆盖。
 */
struct string_storage {
  explicit string_storage(memory::memory_resource* resource = nullptr)
      : values_(core::make_list<string>(resource)) {}

  [[nodiscard]] std::string_view store(string value) {
    values_.push_back(std::move(value));
    const string& stored = values_.back();
    return {stored.data(), stored.size()};
  }

  void clear() noexcept { values_.clear(); }
  [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

private:
  list<string> values_;
};

/** @brief 运行期工厂相关的序列化语义选项。 */
struct factory_options {
  const polymorphic_factory_ops* polymorphic_factory = nullptr;
};

/** @brief 反序列化需要创建运行期值时使用的构造策略。 */
struct construction_options {
  using construct_fn = construction_result (*)(const construction_request& request,
                                               const serialization::context& context,
                                               void* user_data);

  construct_fn construct = nullptr;
  void* user_data = nullptr;
  bool use_registered_factory = true;
  bool use_registered_constructor = true;
  /** @brief 是否允许使用类型静态节点提供的默认或资源感知构造操作作为最终兜底。 */
  bool allow_type_default_construction = true;

  [[nodiscard]] constexpr bool has_custom_construct() const noexcept {
    return construct != nullptr;
  }

  [[nodiscard]] construction_result create(const construction_request& request,
                                           const serialization::context& context) const {
    return construct != nullptr ? construct(request, context, user_data) : construction_result{};
  }
};

/** @brief 递归和输入规模相关的序列化限制。 */
struct limit_options {
  /**
   * @brief 最大嵌套深度。
   *
   * 默认值用于防止裸指针、wrapper、容器或对象图意外形成无限递归。`0` 表示不限制深度。
   */
  std::uint32_t max_depth = 64;
};

/** @brief 运行期构造和销毁相关的序列化语义选项。 */
struct memory_options {
  memory::memory_resource* resource = nullptr;
};

/** @brief 借用视图反序列化相关的外部存储选项。 */
struct storage_options {
  string_storage* string_view_storage = nullptr;
};

/** @brief schema 策略读入阶段可写入的外部状态。 */
struct schema_state {
  variant::object_type* preserved_unknown_fields = nullptr;
};

/** @brief 单次序列化调度的策略配置。 */
struct context_config {
  field_filter_options fields{};
  schema_options schema{};
  limit_options limits{};
  polymorphic_options polymorphic{};
  json_options json{};
};

/** @brief 单次序列化调度可使用的外部服务和解析器。 */
struct context_resolvers {
  external_ref_options external_refs{};
  factory_options factories{};
  construction_options construction{};
  memory_options memory{};
};

/** @brief 单次序列化调度的可变运行期状态。 */
struct context_state {
  storage_options storage{};
  schema_state schema{};
  construction_request construction{};
  std::uint32_t current_depth = 0;
};

/**
 * @brief 单次序列化调度上下文。
 *
 * `config.fields.schema_version == 0` 表示不启用字段版本过滤。传入非 0 版本时，调度层只处理当前
 * 版本可见的字段。
 */
struct context {
  meta::context* type_context = nullptr;
  context_config config{};
  context_resolvers resolvers{};
  context_state state{};

  [[nodiscard]] meta::context& effective_type_context() const noexcept {
    return type_context != nullptr ? *type_context : meta::default_context();
  }

  [[nodiscard]] std::uint32_t effective_schema_version() const noexcept {
    return config.fields.schema_version;
  }

  [[nodiscard]] std::uint32_t effective_schema_source_version() const noexcept {
    return config.schema.source_version;
  }

  [[nodiscard]] const serialization::schema_migration_ops*
  effective_schema_migration() const noexcept {
    return config.schema.migration;
  }

  [[nodiscard]] const serialization::external_ref_resolver_ops*
  effective_external_ref_resolver() const noexcept {
    return resolvers.external_refs.resolver;
  }

  [[nodiscard]] const serialization::external_ref_collector_ops*
  effective_external_ref_collector() const noexcept {
    return resolvers.external_refs.collector;
  }

  [[nodiscard]] external_ref_resolution effective_external_ref_resolution() const noexcept {
    return resolvers.external_refs.resolution;
  }

  [[nodiscard]] string_storage* effective_string_view_storage() const noexcept {
    return state.storage.string_view_storage;
  }

  [[nodiscard]] const serialization::polymorphic_factory_ops*
  effective_polymorphic_factory() const noexcept {
    return resolvers.factories.polymorphic_factory;
  }

  [[nodiscard]] memory::memory_resource* effective_memory_resource() const noexcept {
    return resolvers.memory.resource;
  }

  [[nodiscard]] const serialization::construction_options&
  effective_construction_options() const noexcept {
    return resolvers.construction;
  }

  [[nodiscard]] construction_request
  make_construction_request(meta::type type, construction_site site,
                            std::size_t index = construction_request_no_index) const noexcept {
    construction_request request = state.construction;
    request.type = type;
    request.site = site;
    request.index = index;
    return request;
  }

  [[nodiscard]] std::uint32_t current_depth() const noexcept { return state.current_depth; }

  [[nodiscard]] bool at_depth_limit() const noexcept {
    return config.limits.max_depth != 0 && state.current_depth >= config.limits.max_depth;
  }

  /** @brief 创建下一层嵌套上下文，失败时不修改输出参数。 */
  [[nodiscard]] error enter_nested_value(context& nested) const noexcept {
    if (at_depth_limit()) {
      return {error_domain::serialization, errc::recursion_limit};
    }

    nested = *this;
    ++nested.state.current_depth;
    return {};
  }

  /** @brief 创建字段读入使用的嵌套上下文，并写入当前字段构造请求。 */
  [[nodiscard]] error enter_nested_field_value(context& nested, meta::type owner_type,
                                               string_name field_name,
                                               meta::type field_type) const noexcept {
    if (const error status = enter_nested_value(nested); status) {
      return status;
    }

    nested.state.construction.owner_type = owner_type;
    nested.state.construction.field_name = field_name;
    nested.state.construction.type = field_type;
    nested.state.construction.site = construction_site::field;
    nested.state.construction.index = construction_request_no_index;
    return {};
  }
};

/** @brief 文本序列化结果。 */
using serialization_text_result = result<string>;

/** @brief 序列化中间值构造结果。 */
using variant_result = result<variant>;

} // namespace caors::serialization

#endif
