// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/assets/asset_manager.h"

#include "caors/io/file_system_resolver.h"
#include "caors/io/memory_file_system.h"
#include "caors/io/mount_file_system.h"
#include "caors/io/native_file_system.h"
#include "caors/io/uri.h"

#include <string_view>
#include <utility>

namespace caors::assets {
namespace {

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

} // namespace

error asset_manager::make_memory_file_system(file_system_factory_context& context,
                                             const project_file_system_config& config,
                                             void* user_data) {
  (void)user_data;
  auto file_system = std::allocate_shared<io::memory_file_system>(
      allocator<io::memory_file_system>{context.resource}, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, file_system);
}

error asset_manager::make_native_file_system(file_system_factory_context& context,
                                             const project_file_system_config& config,
                                             void* user_data) {
  (void)user_data;
  auto file_system =
      config.root.empty()
          ? std::allocate_shared<io::native_file_system>(
                allocator<io::native_file_system>{context.resource}, context.resource)
          : std::allocate_shared<io::native_file_system>(
                allocator<io::native_file_system>{context.resource}, config.root, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, file_system);
}

error asset_manager::make_resolver_file_system(file_system_factory_context& context,
                                               const project_file_system_config& config,
                                               void* user_data) {
  (void)user_data;
  auto file_system = std::allocate_shared<io::file_system_resolver>(
      allocator<io::file_system_resolver>{context.resource}, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, file_system);
}

error asset_manager::make_mount_file_system(file_system_factory_context& context,
                                            const project_file_system_config& config,
                                            void* user_data) {
  (void)user_data;
  auto file_system = std::allocate_shared<io::mount_file_system>(
      allocator<io::mount_file_system>{context.resource}, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_file_system(config.name, file_system);
}

error asset_manager::make_memory_file_source(source_factory_context& context,
                                             const project_source_config& config, void* user_data) {
  (void)user_data;
  auto file_system = std::allocate_shared<io::memory_file_system>(
      allocator<io::memory_file_system>{context.resource}, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = context.manager.create_owned<resources::file_source>(file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

error asset_manager::make_native_file_source(source_factory_context& context,
                                             const project_source_config& config, void* user_data) {
  (void)user_data;
  auto file_system =
      config.root.empty()
          ? std::allocate_shared<io::native_file_system>(
                allocator<io::native_file_system>{context.resource}, context.resource)
          : std::allocate_shared<io::native_file_system>(
                allocator<io::native_file_system>{context.resource}, config.root, context.resource);
  if (file_system == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  auto* source = context.manager.create_owned<resources::file_source>(file_system);
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

  auto owner = context.manager.find_owned_file_system(file_system_name);
  auto* source = owner ? context.manager.create_owned<resources::file_source>(std::move(owner))
                       : context.manager.create_owned<resources::file_source>(*file_system);
  if (source == nullptr) {
    return {error_domain::resources, errc::out_of_memory};
  }

  return context.manager.bind_source(config.name, *source);
}

struct asset_manager::project_state {
  using file_system_factory_map = core::hash_map<core::string_name, file_system_factory>;
  using source_factory_map = core::hash_map<core::string_name, source_factory>;
  using type_factory_map = core::hash_map<core::string_name, type_factory>;

  struct file_system_binding {
    std::shared_ptr<io::file_system> owner;
    io::file_system* file_system = nullptr;
    bool config_created = false;
  };

  struct source_binding {
    resources::source* source = nullptr;
    bool config_created = false;
  };

  using file_system_map = core::hash_map<core::string_name, file_system_binding>;
  using source_map = core::hash_map<core::string_name, source_binding>;

  struct owned_runtime_object {
    using destroy_fn = void (*)(memory_resource* resource, void* value) noexcept;

    owned_runtime_object() noexcept = default;
    owned_runtime_object(memory_resource* resource, void* value, destroy_fn destroy,
                         bool config_created) noexcept;
    ~owned_runtime_object();

    owned_runtime_object(const owned_runtime_object&) = delete;
    owned_runtime_object& operator=(const owned_runtime_object&) = delete;

    owned_runtime_object(owned_runtime_object&& other) noexcept;
    owned_runtime_object& operator=(owned_runtime_object&& other) noexcept;

    void reset() noexcept;

    memory_resource* resource = nullptr;
    void* value = nullptr;
    destroy_fn destroy = nullptr;
    bool config_created = false;
  };

  using owned_object_list = core::vector<owned_runtime_object>;
  using manual_type_map = core::hash_map<core::string_name, manual_type_binding>;

  explicit project_state(memory_resource* resource)
      : provider_(resource), file_systems_(typename file_system_map::allocator_type{resource}),
        sources_(typename source_map::allocator_type{resource}),
        file_system_factories_(typename file_system_factory_map::allocator_type{resource}),
        source_factories_(typename source_factory_map::allocator_type{resource}),
        type_factories_(typename type_factory_map::allocator_type{resource}),
        owned_objects_(typename owned_object_list::allocator_type{resource}),
        manual_type_bindings_(typename manual_type_map::allocator_type{resource}) {}

  resources::provider provider_;
  file_system_map file_systems_;
  source_map sources_;
  file_system_factory_map file_system_factories_;
  source_factory_map source_factories_;
  type_factory_map type_factories_;
  owned_object_list owned_objects_;
  manual_type_map manual_type_bindings_;
  bool building_config_runtime_ = false;
};

asset_manager::project_state::owned_runtime_object::owned_runtime_object(
    memory_resource* resource, void* value, destroy_fn destroy, bool config_created) noexcept
    : resource(resource), value(value), destroy(destroy), config_created(config_created) {}

asset_manager::project_state::owned_runtime_object::~owned_runtime_object() { reset(); }

asset_manager::project_state::owned_runtime_object::owned_runtime_object(
    owned_runtime_object&& other) noexcept
    : resource(other.resource), value(other.value), destroy(other.destroy),
      config_created(other.config_created) {
  other.resource = nullptr;
  other.value = nullptr;
  other.destroy = nullptr;
  other.config_created = false;
}

asset_manager::project_state::owned_runtime_object&
asset_manager::project_state::owned_runtime_object::operator=(
    owned_runtime_object&& other) noexcept {
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

void asset_manager::project_state::owned_runtime_object::reset() noexcept {
  if (value != nullptr && destroy != nullptr) {
    destroy(resource, value);
  }

  resource = nullptr;
  value = nullptr;
  destroy = nullptr;
  config_created = false;
}

void asset_manager::project_deleter::operator()(project_state* state) const noexcept {
  memory::destroy(state, resource);
}

bool asset_manager::is_building_project() const noexcept {
  return project_->building_config_runtime_;
}

void asset_manager::retain_owned(void* value, owned_destroy_fn destroy) {
  project_->owned_objects_.emplace_back(resource_, value, destroy,
                                        project_->building_config_runtime_);
}

error asset_manager::remember_manual_type_binding_erased(core::string_name type,
                                                         manual_type_binding next) {
  auto position = project_->manual_type_bindings_.find(type);
  if (position == project_->manual_type_bindings_.end()) {
    project_->manual_type_bindings_.emplace(type, next);
    return {};
  }
  auto& binding = position->second;
  if (binding.cache != next.cache || binding.bind != next.bind)
    return {error_domain::resources, errc::already_exists};
  void*& slot = next.async_loader ? binding.async_loader : binding.loader;
  void* value = next.async_loader ? next.async_loader : next.loader;
  if (slot && slot != value)
    return {error_domain::resources, errc::already_exists};
  slot = value;
  return {};
}

void asset_manager::clear_file_systems() noexcept { project_->file_systems_.clear(); }
std::size_t asset_manager::file_system_count() const noexcept {
  return project_->file_systems_.size();
}
void asset_manager::clear_sources() noexcept { project_->sources_.clear(); }
std::size_t asset_manager::source_count() const noexcept { return project_->sources_.size(); }

resources::provider& asset_manager::project_provider() noexcept { return project_->provider_; }
const resources::provider& asset_manager::project_provider() const noexcept {
  return project_->provider_;
}
error asset_manager::use_project_provider_snapshot() {
  return database_.set_snapshot_capture([this] { return project_->provider_.snapshot(); });
}

asset_manager::asset_manager(memory_resource* resource)
    : resource_(resource != nullptr ? resource : get_default_memory_resource()),
      manifest_(resource_), dependency_graph_(resource_), database_(resource_),
      project_(memory::make<project_state>(resource_, resource_), project_deleter{resource_}) {
  database_.set_manifest(manifest_);
  (void)use_project_provider_snapshot();
  database_.set_dependency_graph(dependency_graph_);
  (void)register_builtin_file_system_factories();
  (void)register_builtin_source_factories();
}

asset_manager::~asset_manager() {
  database_.clear_registered_types();
  project_->provider_.clear();
  project_->sources_.clear();
  project_->file_systems_.clear();
  project_->type_factories_.clear();
  project_->source_factories_.clear();
  project_->file_system_factories_.clear();
  project_->manual_type_bindings_.clear();
  project_->owned_objects_.clear();
}

error asset_manager::bind_file_system(core::string_name name, io::file_system& file_system) {
  if (!name) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = project_->file_systems_.find(name);
  if (position != project_->file_systems_.end()) {
    if (position->second.file_system != &file_system) {
      return {error_domain::resources, errc::already_exists};
    }

    return {};
  }

  project_->file_systems_.emplace(name, project_state::file_system_binding{
                                            {}, &file_system, project_->building_config_runtime_});
  return {};
}

error asset_manager::bind_file_system(core::string_name name,
                                      std::shared_ptr<io::file_system> file_system,
                                      std::shared_ptr<void> lifetime) {
  if (!name || !file_system)
    return {error_domain::resources, errc::invalid_argument};
  struct held_backend {
    std::shared_ptr<void> lifetime;
    std::shared_ptr<io::file_system> target;
  };
  auto held =
      std::allocate_shared<held_backend>(allocator<held_backend>{resource_},
                                         held_backend{std::move(lifetime), std::move(file_system)});
  auto owner = std::shared_ptr<io::file_system>{held, held->target.get()};
  const auto found = project_->file_systems_.find(name);
  if (found != project_->file_systems_.end()) {
    if (found->second.file_system != owner.get())
      return {error_domain::resources, errc::already_exists};
    found->second.owner = std::move(owner);
    return {};
  }
  auto* backend = owner.get();
  project_->file_systems_.emplace(
      name, project_state::file_system_binding{std::move(owner), backend,
                                               project_->building_config_runtime_});
  return {};
}

std::shared_ptr<io::file_system>
asset_manager::find_owned_file_system(core::string_name name) const {
  const auto found = project_->file_systems_.find(name);
  return found == project_->file_systems_.end() ? std::shared_ptr<io::file_system>{}
                                                : found->second.owner;
}

bool asset_manager::unbind_file_system(core::string_name name) {
  return project_->file_systems_.erase(name) != 0;
}

io::file_system* asset_manager::find_file_system(core::string_name name) const {
  const auto position = project_->file_systems_.find(name);
  return position != project_->file_systems_.end() ? position->second.file_system : nullptr;
}

error asset_manager::register_file_system_factory(core::string_name type,
                                                  file_system_factory factory) {
  if (!type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = project_->file_system_factories_.find(type);
  if (position != project_->file_system_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  project_->file_system_factories_.emplace(type, factory);
  return {};
}

error asset_manager::bind_source(core::string_name name, resources::source& source) {
  if (!name) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = project_->sources_.find(name);
  if (position != project_->sources_.end()) {
    if (position->second.source != &source) {
      return {error_domain::resources, errc::already_exists};
    }

    return {};
  }

  project_->sources_.emplace(
      name, project_state::source_binding{&source, project_->building_config_runtime_});
  return {};
}

bool asset_manager::unbind_source(core::string_name name) {
  return project_->sources_.erase(name) != 0;
}

resources::source* asset_manager::find_source(core::string_name name) const {
  const auto position = project_->sources_.find(name);
  return position != project_->sources_.end() ? position->second.source : nullptr;
}

error asset_manager::register_source_factory(core::string_name type, source_factory factory) {
  if (!type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = project_->source_factories_.find(type);
  if (position != project_->source_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  project_->source_factories_.emplace(type, factory);
  return {};
}

error asset_manager::register_type_factory(core::string_name resource_type, type_factory factory) {
  if (!resource_type || !factory.valid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  const auto position = project_->type_factories_.find(resource_type);
  if (position != project_->type_factories_.end()) {
    return position->second.create == factory.create &&
                   position->second.user_data == factory.user_data &&
                   position->second.cache_strategy == factory.cache_strategy &&
                   position->second.loader_strategy == factory.loader_strategy
               ? error{}
               : error{error_domain::resources, errc::already_exists};
  }

  project_->type_factories_.emplace(resource_type, factory);
  return {};
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

  const auto position = project_->file_system_factories_.find(config.type);
  if (position == project_->file_system_factories_.end()) {
    append_reason_text(reasons, "file_system 配置没有匹配的 file_system factory");
    return {error_domain::resources, errc::not_found};
  }

  error status{};
  const std::size_t reason_count = reasons.size();
  const bool previous = project_->building_config_runtime_;
  project_->building_config_runtime_ = true;
  status = position->second(*this, config, &reasons);
  project_->building_config_runtime_ = previous;
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

      auto owner = find_owned_file_system(route.target);
      const error status =
          owner ? resolver->mount(io::uri_view{route.prefix}, std::move(owner), route.target_prefix)
                : resolver->mount(io::uri_view{route.prefix}, *target, route.target_prefix);
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

      auto owner = find_owned_file_system(route.target);
      const error status = owner ? mount->mount(route.prefix, std::move(owner), route.target_prefix)
                                 : mount->mount(route.prefix, *target, route.target_prefix);
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

  const auto position = project_->source_factories_.find(config.type);
  if (position == project_->source_factories_.end()) {
    append_reason_text(reasons, "source 配置没有匹配的 source factory");
    return {error_domain::resources, errc::not_found};
  }

  error status{};
  const std::size_t reason_count = reasons.size();
  const bool previous = project_->building_config_runtime_;
  project_->building_config_runtime_ = true;
  status = position->second(*this, config, &reasons);
  project_->building_config_runtime_ = previous;
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

  const auto position = project_->type_factories_.find(config.type);
  if (position == project_->type_factories_.end()) {
    return {};
  }

  error status{};
  const std::size_t reason_count = reasons.size();
  const bool previous = project_->building_config_runtime_;
  project_->building_config_runtime_ = true;
  status = position->second(*this, config, &reasons);
  project_->building_config_runtime_ = previous;
  if (status && reasons.size() == reason_count) {
    append_reason_text(reasons, "resource type factory 返回错误");
  }
  return status;
}

error asset_manager::register_builtin_source_factories() {
  error status = register_source_factory(core::string_name{"memory_file_system"},
                                         source_factory{&asset_manager::make_memory_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"memory"},
                                   source_factory{&asset_manager::make_memory_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"native_file_system"},
                                   source_factory{&asset_manager::make_native_file_source});
  if (status) {
    return status;
  }

  status = register_source_factory(core::string_name{"native"},
                                   source_factory{&asset_manager::make_native_file_source});
  if (status) {
    return status;
  }

  return register_source_factory(core::string_name{"file_system"},
                                 source_factory{&asset_manager::make_bound_file_source});
}

error asset_manager::register_builtin_file_system_factories() {
  error status =
      register_file_system_factory(core::string_name{"memory_file_system"},
                                   file_system_factory{&asset_manager::make_memory_file_system});
  if (status) {
    return status;
  }

  status = register_file_system_factory(
      core::string_name{"memory"}, file_system_factory{&asset_manager::make_memory_file_system});
  if (status) {
    return status;
  }

  status =
      register_file_system_factory(core::string_name{"native_file_system"},
                                   file_system_factory{&asset_manager::make_native_file_system});
  if (status) {
    return status;
  }

  status = register_file_system_factory(
      core::string_name{"native"}, file_system_factory{&asset_manager::make_native_file_system});
  if (status) {
    return status;
  }

  status =
      register_file_system_factory(core::string_name{"file_system_resolver"},
                                   file_system_factory{&asset_manager::make_resolver_file_system});
  if (status) {
    return status;
  }

  status =
      register_file_system_factory(core::string_name{"resolver"},
                                   file_system_factory{&asset_manager::make_resolver_file_system});
  if (status) {
    return status;
  }

  status =
      register_file_system_factory(core::string_name{"mount_file_system"},
                                   file_system_factory{&asset_manager::make_mount_file_system});
  if (status) {
    return status;
  }

  return register_file_system_factory(core::string_name{"mount"},
                                      file_system_factory{&asset_manager::make_mount_file_system});
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

  if (project_->sources_.size() == 1) {
    return project_->sources_.begin()->second.source;
  }

  return nullptr;
}

error asset_manager::clear_config_runtime() {
  if (database_.has_active_operations()) {
    return {error_domain::resources, errc::frozen};
  }
  for (auto position = project_->file_systems_.begin();
       position != project_->file_systems_.end();) {
    if (position->second.config_created) {
      position = project_->file_systems_.erase(position);
    } else {
      ++position;
    }
  }

  for (auto position = project_->sources_.begin(); position != project_->sources_.end();) {
    if (position->second.config_created) {
      position = project_->sources_.erase(position);
    } else {
      ++position;
    }
  }

  database_.clear_registered_types();
  const auto remove_config_object = [](const project_state::owned_runtime_object& object) {
    return object.config_created;
  };
  project_->owned_objects_.erase(std::remove_if(project_->owned_objects_.begin(),
                                                project_->owned_objects_.end(),
                                                remove_config_object),
                                 project_->owned_objects_.end());

  return rebuild_manual_type_bindings();
}

error asset_manager::rebuild_manual_type_bindings() {
  for (const auto& pair : project_->manual_type_bindings_) {
    const manual_type_binding& binding = pair.second;
    if (binding.bind == nullptr || binding.cache == nullptr ||
        (binding.loader == nullptr && binding.async_loader == nullptr)) {
      return {error_domain::resources, errc::invalid_argument};
    }

    const error status =
        binding.bind(*this, binding.cache, binding.loader, binding.async_loader, pair.first);
    if (status) {
      return status;
    }
  }

  return {};
}

} // namespace caors::assets
