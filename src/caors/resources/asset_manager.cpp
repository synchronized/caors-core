// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/asset_manager.h"

#include "caors/io/memory_file_system.h"
#include "caors/io/native_file_system.h"

#include <algorithm>

namespace caors::resources {
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

[[nodiscard]] const factory_option_field*
find_schema_field(const factory_option_schema& schema, core::string_name name) noexcept {
  for (const factory_option_field& field : schema.fields) {
    if (field.name == name) {
      return &field;
    }
  }

  return nullptr;
}

void assign_reason(core::string* reason, std::string_view text) {
  if (reason != nullptr) {
    reason->assign(text.begin(), text.end());
  }
}

void append_name(core::string& text, core::string_name name) {
  const std::string_view view = name.view();
  text.append(view.begin(), view.end());
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

[[nodiscard]] error validate_options(const factory_option_schema& schema, const variant& options,
                                     core::string* reason) {
  if (schema.fields.empty() && schema.allow_unknown) {
    return {};
  }

  if (options.is_null()) {
    for (const factory_option_field& field : schema.fields) {
      if (field.required) {
        assign_missing_option_reason(reason, field.name);
        return {error_domain::resources, errc::invalid_argument};
      }
    }

    return {};
  }

  if (!options.is_object()) {
    assign_reason(reason, "options 必须是 object 或 null");
    return {error_domain::resources, errc::invalid_argument};
  }

  for (const factory_option_field& field : schema.fields) {
    const variant* value = options.find_member_variant(field.name);
    if (value == nullptr || value->is_null()) {
      if (field.required) {
        assign_missing_option_reason(reason, field.name);
        return {error_domain::resources, errc::invalid_argument};
      }

      continue;
    }

    if (!matches_kind(*value, field.kind)) {
      assign_invalid_option_reason(reason, field.name, field.kind);
      return {error_domain::resources, errc::invalid_argument};
    }
  }

  if (!schema.allow_unknown) {
    for (const auto& member : options.as_object()) {
      if (find_schema_field(schema, member.name) == nullptr) {
        assign_unknown_option_reason(reason, member.name);
        return {error_domain::resources, errc::invalid_argument};
      }
    }
  }

  return {};
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
      database_(resource_), sources_(typename source_map::allocator_type{resource_}),
      source_factories_(typename source_factory_map::allocator_type{resource_}),
      type_factories_(typename type_factory_map::allocator_type{resource_}),
      owned_objects_(typename owned_object_list::allocator_type{resource_}),
      owned_types_(typename owned_type_map::allocator_type{resource_}),
      manual_type_bindings_(typename manual_type_map::allocator_type{resource_}) {
  database_.set_manifest(manifest_);
  database_.set_provider(provider_);
  database_.set_dependency_graph(dependency_graph_);
  (void)register_builtin_source_factories();
}

asset_manager::~asset_manager() {
  database_.clear_registered_types();
  provider_.clear();
  sources_.clear();
  type_factories_.clear();
  source_factories_.clear();
  owned_types_.clear();
  manual_type_bindings_.clear();
  owned_objects_.clear();
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
                   position->second.user_data == factory.user_data
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  type_factories_.emplace(resource_type, factory);
  return {};
}

error asset_manager::build(const project_config& config) {
  return build_detailed(config).status;
}

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

  for (std::size_t index = 0; index < config.sources.size(); ++index) {
    const project_source_config& source_config = config.sources[index];
    core::string reason{allocator<char>{resource_}};
    status = ensure_source_from_config(source_config, &reason);
    if (status) {
      set_build_failure(result, asset_build_stage::source, index, status, source_config.name,
                        source_config.type, {}, reason);
      return result;
    }
  }

  for (std::size_t index = 0; index < config.resource_types.size(); ++index) {
    const project_resource_type_config& type_config = config.resource_types[index];
    core::string reason{allocator<char>{resource_}};
    status = ensure_type_from_config(type_config, &reason);
    if (status) {
      set_build_failure(result, asset_build_stage::resource_type, index, status, type_config.type,
                        type_config.type, {}, reason);
      return result;
    }
  }

  for (std::size_t index = 0; index < config.resources.size(); ++index) {
    const project_resource_config& resource_config = config.resources[index];
    if (!resource_config.guid || resource_config.location.empty()) {
      set_build_failure(result, asset_build_stage::resource, index,
                        {error_domain::resources, errc::invalid_argument},
                        resource_config.source, resource_config.type, resource_config.guid,
                        "resource 配置缺少 guid 或 location");
      return result;
    }

    resources::source* source = resolve_resource_source(config, resource_config);
    if (source == nullptr) {
      set_build_failure(result, asset_build_stage::resource, index,
                        {error_domain::resources, errc::not_found}, resource_config.source,
                        resource_config.type, resource_config.guid,
                        "resource 配置无法解析 source");
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

error asset_manager::ensure_source_from_config(const project_source_config& config,
                                               core::string* reason) {
  if (!config.name) {
    assign_reason(reason, "source 配置缺少 name");
    return {error_domain::resources, errc::invalid_argument};
  }

  if (find_source(config.name) != nullptr) {
    return {};
  }

  const auto position = source_factories_.find(config.type);
  if (position == source_factories_.end()) {
    assign_reason(reason, "source 配置没有匹配的 source factory");
    return {error_domain::resources, errc::not_found};
  }

  error status = validate_options(position->second.schema, config.options, reason);
  if (status) {
    return status;
  }

  const bool previous = building_config_runtime_;
  building_config_runtime_ = true;
  status = position->second(*this, config);
  building_config_runtime_ = previous;
  if (status && reason != nullptr && reason->empty()) {
    assign_reason(reason, "source factory 返回错误");
  }
  return status;
}

error asset_manager::ensure_type_from_config(const project_resource_type_config& config,
                                             core::string* reason) {
  if (!config.type) {
    assign_reason(reason, "resource type 配置缺少 type");
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = type_factories_.find(config.type);
  if (position == type_factories_.end()) {
    return {};
  }

  error status = validate_options(position->second.schema, config.options, reason);
  if (status) {
    return status;
  }

  const bool previous = building_config_runtime_;
  building_config_runtime_ = true;
  status = position->second(*this, config);
  building_config_runtime_ = previous;
  if (status && reason != nullptr && reason->empty()) {
    assign_reason(reason, "resource type factory 返回错误");
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

  return register_source_factory(core::string_name{"native"},
                                 source_factory_ops{&asset_manager::make_native_file_source});
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
  for (auto position = sources_.begin(); position != sources_.end();) {
    if (position->second.config_created) {
      position = sources_.erase(position);
    } else {
      ++position;
    }
  }

  database_.clear_registered_types();
  owned_types_.clear();

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

} // namespace caors::resources
