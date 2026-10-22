// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_PROJECT_BUILDER_H_
#define CAORS_ASSETS_PROJECT_BUILDER_H_

#include "caors/assets/asset_manager.h"

namespace caors::assets {
/**
 * @brief 按工程配置装配资源环境；不参与日常资源请求。
 * @note 借用 manager，必须在其所属线程调用且 manager 须覆盖本对象寿命。
 * 配置创建的对象暂由 manager 持有，builder 析构不清理它们；实际操作未结束时构建返回 frozen。
 * 构建失败可能留下部分配置，沿用既有逐阶段构建语义，不提供事务回滚。
 */
class CAORS_API project_builder final {
public:
  /** @brief 来源工厂上下文。 */
  using source_factory_context = asset_manager::source_factory_context;
  /** @brief 资源类型工厂上下文。 */
  using type_factory_context = asset_manager::type_factory_context;
  /** @brief 文件系统工厂上下文。 */
  using file_system_factory_context = asset_manager::file_system_factory_context;
  /** @brief 来源配置工厂。 */
  using source_factory = asset_manager::source_factory;
  /** @brief 资源类型配置工厂。 */
  using type_factory = asset_manager::type_factory;
  /** @brief 文件系统配置工厂。 */
  using file_system_factory = asset_manager::file_system_factory;
  /** @brief 强类型工程工厂函数签名。 */
  template <typename Options>
  using typed_file_system_factory_fn = asset_manager::typed_file_system_factory_fn<Options>;
  /** @brief 强类型工程工厂函数签名。 */
  template <typename Options>
  using typed_source_factory_fn = asset_manager::typed_source_factory_fn<Options>;
  /** @brief 强类型工程工厂函数签名。 */
  template <typename Options>
  using typed_type_factory_fn = asset_manager::typed_type_factory_fn<Options>;
  /** @brief 绑定待装配的资源环境，不转移所有权。 */
  explicit project_builder(asset_manager& manager) noexcept : manager_(manager) {}
  /** @brief 按配置重建环境，返回首个错误；手工绑定按既有规则保留。 */
  [[nodiscard]] error build(const project_config& config);
  /** @brief 执行相同装配流程，并返回失败阶段及诊断。 */
  [[nodiscard]] asset_build_result build_detailed(const project_config& config);

  /** @brief 查询装配使用的内存资源；由宿主保证其寿命。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return manager_.resource(); }
  /** @brief 借用绑定文件系统，宿主须维持后端寿命。 */
  [[nodiscard]] error bind_file_system(core::string_name name, io::file_system& file_system) {
    return manager_.bind_file_system(name, file_system);
  }
  /** @brief 持有后端和可选模块凭据；allocator 寿命仍由宿主保证。 */
  [[nodiscard]] error bind_file_system(core::string_name name,
                                       std::shared_ptr<io::file_system> file_system,
                                       std::shared_ptr<void> lifetime = {}) {
    return manager_.bind_file_system(name, std::move(file_system), std::move(lifetime));
  }
  /** @brief 查询借用后端，缺失时返回空。 */
  [[nodiscard]] io::file_system* find_file_system(core::string_name name) const {
    return manager_.find_file_system(name);
  }
  /** @brief 查询拥有式后端；借用绑定返回空。 */
  [[nodiscard]] std::shared_ptr<io::file_system>
  find_owned_file_system(core::string_name name) const {
    return manager_.find_owned_file_system(name);
  }
  /** @brief 解除名称绑定，不取消已有来源快照。 */
  [[nodiscard]] bool unbind_file_system(core::string_name name) {
    return manager_.unbind_file_system(name);
  }
  /** @brief 清空文件系统名称绑定。 */
  void clear_file_systems() noexcept { manager_.clear_file_systems(); }
  /** @brief 查询文件系统绑定数量。 */
  [[nodiscard]] std::size_t file_system_count() const noexcept {
    return manager_.file_system_count();
  }
  /** @brief 借用绑定资源来源；宿主须维持来源寿命。 */
  [[nodiscard]] error bind_source(core::string_name name, resources::source& source) {
    return manager_.bind_source(name, source);
  }
  /** @brief 查询已绑定来源。 */
  [[nodiscard]] resources::source* find_source(core::string_name name) const {
    return manager_.find_source(name);
  }
  /** @brief 解除来源绑定。 */
  [[nodiscard]] bool unbind_source(core::string_name name) { return manager_.unbind_source(name); }
  /** @brief 清空来源名称绑定。 */
  void clear_sources() noexcept { manager_.clear_sources(); }
  /** @brief 查询来源绑定数量。 */
  [[nodiscard]] std::size_t source_count() const noexcept { return manager_.source_count(); }
  /** @brief 注册文件系统配置工厂。 */
  [[nodiscard]] error register_file_system_factory(core::string_name type,
                                                   file_system_factory factory) {
    return manager_.register_file_system_factory(type, std::move(factory));
  }
  /** @brief 注册来源配置工厂。 */
  [[nodiscard]] error register_source_factory(core::string_name type, source_factory factory) {
    return manager_.register_source_factory(type, std::move(factory));
  }
  /** @brief 注册资源类型配置工厂。 */
  [[nodiscard]] error register_type_factory(core::string_name type, type_factory factory) {
    return manager_.register_type_factory(type, std::move(factory));
  }
  /** @brief 校验强类型配置后调用文件系统工厂。 */
  template <typename Options, asset_manager::typed_file_system_factory_fn<Options> Create>
  [[nodiscard]] error register_typed_file_system_factory(core::string_name type,
                                                         void* data = nullptr) {
    return manager_.register_typed_file_system_factory<Options, Create>(type, data);
  }
  /** @brief 校验强类型配置后调用来源工厂。 */
  template <typename Options, asset_manager::typed_source_factory_fn<Options> Create>
  [[nodiscard]] error register_typed_source_factory(core::string_name type, void* data = nullptr) {
    return manager_.register_typed_source_factory<Options, Create>(type, data);
  }
  /** @brief 匹配策略并校验强类型配置后调用资源类型工厂。 */
  template <typename Options, asset_manager::typed_type_factory_fn<Options> Create>
  [[nodiscard]] error
  register_typed_type_factory(core::string_name type, core::string_name cache = {},
                              core::string_name loader = {}, void* data = nullptr) {
    return manager_.register_typed_type_factory<Options, Create>(type, cache, loader, data);
  }
  /** @brief 创建 manager 保活的装配对象；不得手动释放，builder 析构不销毁对象。 */
  template <typename T, typename... Args> [[nodiscard]] T* create_owned(Args&&... args) {
    return manager_.create_owned<T>(std::forward<Args>(args)...);
  }

private:
  asset_manager& manager_;
};
} // namespace caors::assets

#endif
