// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets/asset_manager.h"

#include "caors/io/file_system_resolver.h"
#include "caors/io/memory_file_system.h"
#include "caors/io/mount_file_system.h"
#include "caors/io/native_file_system.h"
#include "caors/io/uri.h"

#include <algorithm>
#include <optional>
#include <string>

namespace caors::assets {
namespace {

[[nodiscard]] std::string_view kind_name(factory_option_kind kind) noexcept {
  switch (kind) {
  case factory_option_kind::any:
    return "any";
  case factory_option_kind::boolean:
    return "boolean";
  case factory_option_kind::signed_integer:
    return "signed_integer";
  case factory_option_kind::unsigned_integer:
    return "unsigned_integer";
  case factory_option_kind::floating:
    return "floating";
  case factory_option_kind::string:
    return "string";
  case factory_option_kind::array:
    return "array";
  case factory_option_kind::dictionary:
    return "dictionary";
  case factory_option_kind::object:
    return "object";
  }

  return "unknown";
}

[[nodiscard]] bool matches_kind(const variant& value, factory_option_kind kind) noexcept {
  switch (kind) {
  case factory_option_kind::any:
    return true;
  case factory_option_kind::boolean:
    return value.is_bool();
  case factory_option_kind::signed_integer:
    return value.is_i64();
  case factory_option_kind::unsigned_integer:
    return value.is_u64();
  case factory_option_kind::floating:
    return value.is_f64();
  case factory_option_kind::string:
    return value.is_string();
  case factory_option_kind::array:
    return value.is_array();
  case factory_option_kind::dictionary:
    return value.is_dictionary();
  case factory_option_kind::object:
    return value.is_object();
  }

  return false;
}

[[nodiscard]] const factory_option_field* find_schema_field(const factory_option_schema& schema,
                                                            core::string_name name) noexcept {
  for (const factory_option_field& field : schema.fields) {
    if (field.name == name) {
      return &field;
    }
  }

  return nullptr;
}

void append_name(core::string& text, core::string_name name) {
  const std::string_view view = name.view();
  text.append(view.begin(), view.end());
}

[[nodiscard]] core::string make_reason(asset_build_result::reason_list& reasons) {
  return core::string{allocator<char>{reasons.get_allocator().resource()}};
}

void append_reason(asset_build_result::reason_list& reasons, core::string reason) {
  if (!reason.empty()) {
    reasons.push_back(std::move(reason));
  }
}

void append_reason_text(asset_build_result::reason_list& reasons, std::string_view text) {
  core::string reason = make_reason(reasons);
  reason.assign(text.begin(), text.end());
  append_reason(reasons, std::move(reason));
}

void assign_missing_option_reason(core::string* reason, core::string_name name) {
  if (reason == nullptr) {
    return;
  }

  reason->assign("缺少必填 options 字段：");
  append_name(*reason, name);
}

void assign_invalid_option_reason(core::string* reason, core::string_name name,
                                  factory_option_kind kind) {
  if (reason == nullptr) {
    return;
  }

  reason->assign("options 字段类型不匹配：");
  append_name(*reason, name);
  reason->append("，期望 ");
  const std::string_view expected = kind_name(kind);
  reason->append(expected.begin(), expected.end());
}

void assign_unknown_option_reason(core::string* reason, core::string_name name) {
  if (reason == nullptr) {
    return;
  }

  reason->assign("发现未声明的 options 字段：");
  append_name(*reason, name);
}

void assign_empty_option_reason(core::string* reason, core::string_name name) {
  if (reason == nullptr) {
    return;
  }

  reason->assign("options 字段不能为空：");
  append_name(*reason, name);
}

void assign_range_option_reason(core::string* reason, core::string_name name, double min,
                                double max) {
  if (reason == nullptr) {
    return;
  }

  reason->assign("options 字段超出范围：");
  append_name(*reason, name);
  reason->append("，期望范围 [");
  reason->append(std::to_string(min));
  reason->append(", ");
  reason->append(std::to_string(max));
  reason->append("]");
}

void assign_one_of_option_reason(core::string* reason, core::string_name name,
                                 const factory_option_field::value_list& values) {
  if (reason == nullptr) {
    return;
  }

  reason->assign("options 字段不是允许值：");
  append_name(*reason, name);
  if (values.empty()) {
    return;
  }

  reason->append("，允许值 ");
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      reason->append(", ");
    }
    append_name(*reason, values[index]);
  }
}

[[nodiscard]] std::optional<double> numeric_option_value(const variant& value) noexcept {
  if (const auto* number = value.try_as_i64()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_as_u64()) {
    return static_cast<double>(*number);
  }
  if (const auto* number = value.try_as_f64()) {
    return *number;
  }

  return std::nullopt;
}

[[nodiscard]] bool option_value_empty(const variant& value) noexcept {
  if (const auto* text = value.try_as_string()) {
    return text->empty();
  }
  if (const auto* values = value.try_as_array()) {
    return values->empty();
  }
  if (const auto* values = value.try_as_dictionary()) {
    return values->empty();
  }
  if (const auto* values = value.try_as_object()) {
    return values->empty();
  }

  return value.is_null();
}

[[nodiscard]] bool option_value_allowed(const variant& value,
                                        const factory_option_field::value_list& values) noexcept {
  if (values.empty()) {
    return true;
  }

  const string* text = value.try_as_string();
  if (text == nullptr) {
    return false;
  }

  const std::string_view view{text->data(), text->size()};
  for (core::string_name allowed : values) {
    if (allowed == view) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] error validate_options(const factory_option_schema& schema, const variant& options,
                                     asset_build_result::reason_list& reasons) {
  const std::size_t initial_reason_count = reasons.size();

  if (schema.fields.empty() && schema.allow_unknown) {
    return {};
  }

  if (options.is_null()) {
    for (const factory_option_field& field : schema.fields) {
      if (field.required) {
        core::string reason = make_reason(reasons);
        assign_missing_option_reason(&reason, field.name);
        append_reason(reasons, std::move(reason));
      }
    }

    return reasons.size() != initial_reason_count
               ? error{error_domain::resources, errc::invalid_argument}
               : error{};
  }

  if (!options.is_object()) {
    append_reason_text(reasons, "options 必须是 object 或 null");
    return {error_domain::resources, errc::invalid_argument};
  }

  for (const factory_option_field& field : schema.fields) {
    const variant* value = options.find_member_variant(field.name);
    if (value == nullptr || value->is_null()) {
      if (field.required) {
        core::string reason = make_reason(reasons);
        assign_missing_option_reason(&reason, field.name);
        append_reason(reasons, std::move(reason));
      }

      continue;
    }

    if (!matches_kind(*value, field.kind)) {
      core::string reason = make_reason(reasons);
      assign_invalid_option_reason(&reason, field.name, field.kind);
      append_reason(reasons, std::move(reason));
      continue;
    }

    if (field.non_empty && option_value_empty(*value)) {
      core::string reason = make_reason(reasons);
      assign_empty_option_reason(&reason, field.name);
      append_reason(reasons, std::move(reason));
    }

    if (field.has_range) {
      const std::optional<double> number = numeric_option_value(*value);
      if (!number.has_value()) {
        core::string reason = make_reason(reasons);
        assign_invalid_option_reason(&reason, field.name, factory_option_kind::floating);
        append_reason(reasons, std::move(reason));
      } else if (*number < field.range_min || *number > field.range_max) {
        core::string reason = make_reason(reasons);
        assign_range_option_reason(&reason, field.name, field.range_min, field.range_max);
        append_reason(reasons, std::move(reason));
      }
    }

    if (!option_value_allowed(*value, field.allowed_values)) {
      core::string reason = make_reason(reasons);
      assign_one_of_option_reason(&reason, field.name, field.allowed_values);
      append_reason(reasons, std::move(reason));
    }
  }

  if (!schema.allow_unknown) {
    for (const auto& member : options.as_object()) {
      if (find_schema_field(schema, member.name) == nullptr) {
        core::string reason = make_reason(reasons);
        assign_unknown_option_reason(&reason, member.name);
        append_reason(reasons, std::move(reason));
      }
    }
  }

  return reasons.size() != initial_reason_count
             ? error{error_domain::resources, errc::invalid_argument}
             : error{};
}

void set_build_failure(asset_build_result& result, asset_build_stage stage, std::size_t index,
                       error status, core::string_name name, core::string_name type,
                       resource_guid guid, std::string_view reason) {
  result.status = status;
  result.stage = stage;
  result.index = index;
  result.name = name;
  result.type = type;
  result.guid = guid;
  if (!reason.empty()) {
    result.add_reason(reason);
  }
}

} // namespace

error asset_manager::make_memory_file_system(file_system_factory_context& context,
                                             const project_file_system_config& config,
                                             void* user_data) {
  (void)user_data;
  auto* file_system = context.manager.create_owned<io::memory_file_system>(context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, *file_system);
}

error asset_manager::make_native_file_system(file_system_factory_context& context,
                                             const project_file_system_config& config,
                                             void* user_data) {
  (void)user_data;
  auto* file_system =
      config.root.empty()
          ? context.manager.create_owned<io::native_file_system>(context.resource)
          : context.manager.create_owned<io::native_file_system>(config.root, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, *file_system);
}

error asset_manager::make_resolver_file_system(file_system_factory_context& context,
                                               const project_file_system_config& config,
                                               void* user_data) {
  (void)user_data;
  auto* file_system = context.manager.create_owned<io::file_system_resolver>(context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, *file_system);
}

error asset_manager::make_mount_file_system(file_system_factory_context& context,
                                            const project_file_system_config& config,
                                            void* user_data) {
  (void)user_data;
  auto* file_system = context.manager.create_owned<io::mount_file_system>(context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, *file_system);
}

error asset_manager::make_memory_file_source(source_factory_context& context,
                                             const project_source_config& config, void* user_data) {
  (void)user_data;
  auto* file_system = context.manager.create_owned<io::memory_file_system>(context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = context.manager.create_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

error asset_manager::make_native_file_source(source_factory_context& context,
                                             const project_source_config& config, void* user_data) {
  (void)user_data;
  auto* file_system =
      config.root.empty()
          ? context.manager.create_owned<io::native_file_system>(context.resource)
          : context.manager.create_owned<io::native_file_system>(config.root, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = context.manager.create_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

error asset_manager::make_bound_file_source(source_factory_context& context,
                                            const project_source_config& config, void* user_data) {
  (void)user_data;
  const core::string_name file_system_name = config.file_system ? config.file_system : config.name;
  io::file_system* file_system = context.manager.find_file_system(file_system_name);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::not_found};
  }

  auto* source = context.manager.create_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

asset_manager::owned_runtime_object::owned_runtime_object(memory_resource* resource, void* value,
                                                          destroy_fn destroy,
                                                          bool config_created) noexcept
    : resource(resource), value(value), destroy(destroy), config_created(config_created) {}

asset_manager::owned_runtime_object::~owned_runtime_object() { reset(); }

asset_manager::owned_runtime_object::owned_runtime_object(owned_runtime_object&& other) noexcept
    : resource(other.resource), value(other.value), destroy(other.destroy),
      config_created(other.config_created) {
  other.resource = nullptr;
  other.value = nullptr;
  other.destroy = nullptr;
  other.config_created = false;
}

asset_manager::owned_runtime_object&
asset_manager::owned_runtime_object::operator=(owned_runtime_object&& other) noexcept {
  if (this != &other) {
    reset();
    resource = other.resource;
    value = other.value;
    destroy = other.destroy;
    config_created = other.config_created;
    other.resource = nullptr;
    other.value = nullptr;
    other.destroy = nullptr;
    other.config_created = false;
  }

  return *this;
}

void asset_manager::owned_runtime_object::reset() noexcept {
  if (value != nullptr && destroy != nullptr) {
    destroy(resource, value);
  }

  resource = nullptr;
  value = nullptr;
  destroy = nullptr;
  config_created = false;
}

asset_manager::asset_manager(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      manifest_(resource_), provider_(resource_), dependency_graph_(resource_),
      database_(resource_), file_systems_(typename file_system_map::allocator_type{resource_}),
      sources_(typename source_map::allocator_type{resource_}),
      file_system_factories_(typename file_system_factory_map::allocator_type{resource_}),
      source_factories_(typename source_factory_map::allocator_type{resource_}),
      type_factories_(typename type_factory_map::allocator_type{resource_}),
      owned_objects_(typename owned_object_list::allocator_type{resource_}),
      manual_type_bindings_(typename manual_type_map::allocator_type{resource_}) {
  database_.set_manifest(manifest_);
  database_.set_provider(provider_);
  database_.set_dependency_graph(dependency_graph_);
  (void)register_builtin_file_system_factories();
  (void)register_builtin_source_factories();
}

asset_manager::~asset_manager() {
  database_.clear_registered_types();
  provider_.clear();
  sources_.clear();
  file_systems_.clear();
  type_factories_.clear();
  source_factories_.clear();
  file_system_factories_.clear();
  manual_type_bindings_.clear();
  owned_objects_.clear();
}

error asset_manager::bind_file_system(core::string_name name, io::file_system& file_system) {
  if (!name) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = file_systems_.find(name);
  if (position != file_systems_.end()) {
    if (position->second.file_system != &file_system) {
      return {error_domain::resources, errc::already_exists};
    }

    return {};
  }

  file_systems_.emplace(name, file_system_binding{&file_system, building_config_runtime_});
  return {};
}

bool asset_manager::unbind_file_system(core::string_name name) {
  return file_systems_.erase(name) != 0;
}

io::file_system* asset_manager::find_file_system(core::string_name name) const {
  const auto position = file_systems_.find(name);
  return position != file_systems_.end() ? position->second.file_system : nullptr;
}

error asset_manager::register_file_system_factory(core::string_name type,
                                                  file_system_factory_ops factory) {
  if (!type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = file_system_factories_.find(type);
  if (position != file_system_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  file_system_factories_.emplace(type, factory);
  return {};
}

error asset_manager::bind_source(core::string_name name, resources::source& source) {
  if (!name) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = sources_.find(name);
  if (position != sources_.end()) {
    if (position->second.source != &source) {
      return {error_domain::resources, errc::already_exists};
    }

    return {};
  }

  sources_.emplace(name, source_binding{&source, building_config_runtime_});
  return {};
}

bool asset_manager::unbind_source(core::string_name name) { return sources_.erase(name) != 0; }

resources::source* asset_manager::find_source(core::string_name name) const {
  const auto position = sources_.find(name);
  return position != sources_.end() ? position->second.source : nullptr;
}

error asset_manager::register_source_factory(core::string_name type, source_factory_ops factory) {
  if (!type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = source_factories_.find(type);
  if (position != source_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  source_factories_.emplace(type, factory);
  return {};
}

error asset_manager::register_type_factory(core::string_name resource_type,
                                           type_factory_ops factory) {
  if (!resource_type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = type_factories_.find(resource_type);
  if (position != type_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data &&
                   position->second.cache_strategy == factory.cache_strategy &&
                   position->second.loader_strategy == factory.loader_strategy
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  type_factories_.emplace(resource_type, factory);
  return {};
}

error asset_manager::build(const project_config& config) { return build_detailed(config).status; }

asset_build_result asset_manager::build_detailed(const project_config& config) {
  asset_build_result result{resource_};
  error status = clear_config_runtime();
  if (status) {
    set_build_failure(result, asset_build_stage::clear_runtime, asset_build_result::npos, status,
                      {}, {}, {}, "清理上一轮配置运行期对象失败");
    return result;
  }

  manifest_.clear();
  provider_.clear();
  dependency_graph_.clear();

  for (std::size_t index = 0; index < config.file_systems.size(); ++index) {
    const project_file_system_config& file_system_config = config.file_systems[index];
    status = ensure_file_system_from_config(file_system_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::file_system, index, status,
                        file_system_config.name, file_system_config.type, {}, {});
      return result;
    }
  }

  for (std::size_t index = 0; index < config.file_systems.size(); ++index) {
    const project_file_system_config& file_system_config = config.file_systems[index];
    status = configure_file_system_from_config(file_system_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::file_system, index, status,
                        file_system_config.name, file_system_config.type, {}, {});
      return result;
    }
  }

  for (std::size_t index = 0; index < config.sources.size(); ++index) {
    const project_source_config& source_config = config.sources[index];
    status = ensure_source_from_config(source_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::source, index, status, source_config.name,
                        source_config.type, {}, {});
      return result;
    }
  }

  for (std::size_t index = 0; index < config.resource_types.size(); ++index) {
    const project_resource_type_config& type_config = config.resource_types[index];
    status = ensure_type_from_config(type_config, result.reasons);
    if (status) {
      set_build_failure(result, asset_build_stage::resource_type, index, status, type_config.type,
                        type_config.type, {}, {});
      return result;
    }
  }

  for (std::size_t index = 0; index < config.resources.size(); ++index) {
    const project_resource_config& resource_config = config.resources[index];
    if (!resource_config.guid || resource_config.location.empty()) {
      set_build_failure(result, asset_build_stage::resource, index,
                        {error_domain::resources, errc::invalid_argument}, resource_config.source,
                        resource_config.type, resource_config.guid,
                        "resource 配置缺少 guid 或 location");
      return result;
    }

    resources::source* source = resolve_resource_source(config, resource_config);
    if (source == nullptr) {
      set_build_failure(result, asset_build_stage::resource, index,
                        {error_domain::resources, errc::not_found}, resource_config.source,
                        resource_config.type, resource_config.guid, "resource 配置无法解析 source");
      return result;
    }

    status =
        manifest_.set(resource_config.guid, resource_config.location, resource_config.type,
                      std::string_view{resource_config.hash.data(), resource_config.hash.size()},
                      resource_config.version);
    if (status) {
      set_build_failure(result, asset_build_stage::resource, index, status, resource_config.source,
                        resource_config.type, resource_config.guid, "写入 manifest 失败");
      return result;
    }

    status = provider_.rebind(resource_config.guid, *source, resource_config.location);
    if (status) {
      set_build_failure(result, asset_build_stage::resource, index, status, resource_config.source,
                        resource_config.type, resource_config.guid, "绑定 provider source 失败");
      return result;
    }
  }

  for (std::size_t index = 0; index < config.dependencies.size(); ++index) {
    const project_dependency_config& dependency = config.dependencies[index];
    if (!dependency_graph_.add(dependency.resource, dependency.dependency)) {
      set_build_failure(result, asset_build_stage::dependency, index,
                        {error_domain::resources, errc::invalid_argument}, {}, {},
                        dependency.resource, "dependency 配置无效或存在自依赖");
      return result;
    }
  }

  return result;
}

error asset_manager::ensure_file_system_from_config(const project_file_system_config& config,
                                                    asset_build_result::reason_list& reasons) {
  if (!config.name || !config.type) {
    append_reason_text(reasons, "file_system 配置缺少 name 或 type");
    return {error_domain::resources, errc::invalid_argument};
  }

  if (find_file_system(config.name) != nullptr) {
    return {};
  }

  const auto position = file_system_factories_.find(config.type);
  if (position == file_system_factories_.end()) {
    append_reason_text(reasons, "file_system 配置没有匹配的 file_system factory");
    return {error_domain::resources, errc::not_found};
  }

  error status = validate_options(position->second.schema, config.options, reasons);
  if (status) {
    return status;
  }

  const std::size_t reason_count = reasons.size();
  const bool previous = building_config_runtime_;
  building_config_runtime_ = true;
  status = position->second(*this, config);
  building_config_runtime_ = previous;
  if (status && reasons.size() == reason_count) {
    append_reason_text(reasons, "file_system factory 返回错误");
  }
  return status;
}

error asset_manager::configure_file_system_from_config(const project_file_system_config& config,
                                                       asset_build_result::reason_list& reasons) {
  if (config.routes.empty()) {
    return {};
  }

  io::file_system* file_system = find_file_system(config.name);
  if (file_system == nullptr) {
    append_reason_text(reasons, "file_system 路由配置找不到目标 file_system");
    return {error_domain::resources, errc::not_found};
  }

  if (auto* resolver = dynamic_cast<io::file_system_resolver*>(file_system)) {
    for (const project_file_system_route_config& route : config.routes) {
      io::file_system* target = find_file_system(route.target);
      if (target == nullptr) {
        append_reason_text(reasons, "resolver 路由找不到 target file_system");
        return {error_domain::resources, errc::not_found};
      }

      const error status =
          resolver->mount(io::uri_view{route.prefix}, *target, route.target_prefix);
      if (status) {
        return status;
      }
    }

    return {};
  }

  if (auto* mount = dynamic_cast<io::mount_file_system*>(file_system)) {
    for (const project_file_system_route_config& route : config.routes) {
      const io::uri_view prefix_uri{route.prefix};
      if (prefix_uri.has_scheme()) {
        append_reason_text(reasons, "mount_file_system 路由 prefix 不能包含 URI scheme");
        return {error_domain::resources, errc::invalid_argument};
      }

      io::file_system* target = find_file_system(route.target);
      if (target == nullptr) {
        append_reason_text(reasons, "mount 路由找不到 target file_system");
        return {error_domain::resources, errc::not_found};
      }

      const error status = mount->mount(route.prefix, *target, route.target_prefix);
      if (status) {
        return status;
      }
    }

    return {};
  }

  append_reason_text(reasons, "只有 resolver 或 mount file_system 可以配置 routes");
  return {error_domain::resources, errc::invalid_argument};
}

error asset_manager::ensure_source_from_config(const project_source_config& config,
                                               asset_build_result::reason_list& reasons) {
  if (!config.name) {
    append_reason_text(reasons, "source 配置缺少 name");
    return {error_domain::resources, errc::invalid_argument};
  }

  if (find_source(config.name) != nullptr) {
    return {};
  }

  const auto position = source_factories_.find(config.type);
  if (position == source_factories_.end()) {
    append_reason_text(reasons, "source 配置没有匹配的 source factory");
    return {error_domain::resources, errc::not_found};
  }

  error status = validate_options(position->second.schema, config.options, reasons);
  if (status) {
    return status;
  }

  const std::size_t reason_count = reasons.size();
  const bool previous = building_config_runtime_;
  building_config_runtime_ = true;
  status = position->second(*this, config);
  building_config_runtime_ = previous;
  if (status && reasons.size() == reason_count) {
    append_reason_text(reasons, "source factory 返回错误");
  }
  return status;
}

error asset_manager::ensure_type_from_config(const project_resource_type_config& config,
                                             asset_build_result::reason_list& reasons) {
  if (!config.type) {
    append_reason_text(reasons, "resource type 配置缺少 type");
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = type_factories_.find(config.type);
  if (position == type_factories_.end()) {
    return {};
  }

  error status = validate_options(position->second.schema, config.options, reasons);
  if (status) {
    return status;
  }

  const std::size_t reason_count = reasons.size();
  const bool previous = building_config_runtime_;
  building_config_runtime_ = true;
  status = position->second(*this, config);
  building_config_runtime_ = previous;
  if (status && reasons.size() == reason_count) {
    append_reason_text(reasons, "resource type factory 返回错误");
  }
  return status;
}

error asset_manager::register_builtin_source_factories() {
  error status =
      register_source_factory(core::string_name{"memory_file_system"},
                              source_factory_ops{&asset_manager::make_memory_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"memory"},
                                   source_factory_ops{&asset_manager::make_memory_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"native_file_system"},
                                   source_factory_ops{&asset_manager::make_native_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"native"},
                                   source_factory_ops{&asset_manager::make_native_file_source});
  if (status) {
    return status;
  }

  return register_source_factory(core::string_name{"file_system"},
                                 source_factory_ops{&asset_manager::make_bound_file_source});
}

error asset_manager::register_builtin_file_system_factories() {
  error status = register_file_system_factory(
      core::string_name{"memory_file_system"},
      file_system_factory_ops{&asset_manager::make_memory_file_system});
  if (status) {
    return status;
  }

  status = register_file_system_factory(
      core::string_name{"memory"},
      file_system_factory_ops{&asset_manager::make_memory_file_system});
  if (status) {
    return status;
  }

  status = register_file_system_factory(
      core::string_name{"native_file_system"},
      file_system_factory_ops{&asset_manager::make_native_file_system});
  if (status) {
    return status;
  }

  status = register_file_system_factory(
      core::string_name{"native"},
      file_system_factory_ops{&asset_manager::make_native_file_system});
  if (status) {
    return status;
  }

  status = register_file_system_factory(
      core::string_name{"file_system_resolver"},
      file_system_factory_ops{&asset_manager::make_resolver_file_system});
  if (status) {
    return status;
  }

  status = register_file_system_factory(
      core::string_name{"resolver"},
      file_system_factory_ops{&asset_manager::make_resolver_file_system});
  if (status) {
    return status;
  }

  status =
      register_file_system_factory(core::string_name{"mount_file_system"},
                                   file_system_factory_ops{&asset_manager::make_mount_file_system});
  if (status) {
    return status;
  }

  return register_file_system_factory(
      core::string_name{"mount"}, file_system_factory_ops{&asset_manager::make_mount_file_system});
}

resources::source*
asset_manager::resolve_resource_source(const project_config& config,
                                       const project_resource_config& resource_config) const {
  if (resource_config.source) {
    return find_source(resource_config.source);
  }

  if (config.sources.size() == 1) {
    return find_source(config.sources.front().name);
  }

  if (sources_.size() == 1) {
    return sources_.begin()->second.source;
  }

  return nullptr;
}

error asset_manager::clear_config_runtime() {
  for (auto position = file_systems_.begin(); position != file_systems_.end();) {
    if (position->second.config_created) {
      position = file_systems_.erase(position);
    } else {
      ++position;
    }
  }

  for (auto position = sources_.begin(); position != sources_.end();) {
    if (position->second.config_created) {
      position = sources_.erase(position);
    } else {
      ++position;
    }
  }

  database_.clear_registered_types();
  const auto remove_config_object = [](const owned_runtime_object& object) {
    return object.config_created;
  };
  owned_objects_.erase(
      std::remove_if(owned_objects_.begin(), owned_objects_.end(), remove_config_object),
      owned_objects_.end());

  return rebuild_manual_type_bindings();
}

error asset_manager::rebuild_manual_type_bindings() {
  for (const auto& pair : manual_type_bindings_) {
    const manual_type_binding& binding = pair.second;
    if (binding.bind == nullptr || binding.cache == nullptr || binding.loader == nullptr) {
      return {error_domain::resources, errc::invalid_argument};
    }

    const error status = binding.bind(*this, binding.cache, binding.loader, pair.first);
    if (status) {
      return status;
    }
  }

  return {};
}

} // namespace caors::assets
