// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_JSON_H_
#define CAORS_ASSETS_JSON_H_

#include "caors/assets/asset_manager.h"
#include "caors/assets/project_config.h"
#include "caors/assets/serialization.h"
#include "caors/assets/type_factory.h"
#include "caors/resources/manifest.h"
#include "caors/resources/source.h"
#include "caors/serialization/operations.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::assets::json {

/**
 * @brief 基于 `resources::provider` 的 JSON 资源加载器。
 *
 * `json::loader<T>` 通过 provider 按 GUID 定位字节来源和位置，读取 JSON 文本后复用
 * `assets::load_serialized<T>()` 反序列化为资源对象。它不拥有 provider，调用方需要保证 provider
 * 生命周期。
 */
template <typename T> class loader final : public resources::loader<T> {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit loader(const resources::provider& provider) : provider_(&provider) {}

  loader(const serialization::context& context, const resources::provider& provider)
      : provider_(&provider), context_(context) {}

  [[nodiscard]] resources::load_result<resource_type>
  load(resources::resource_guid guid, memory_resource* resource = nullptr) override {
    io::input_stream_result stream = provider_->open_read(guid, resource);
    if (stream.failed()) {
      return {stream.status, {}};
    }

    serialization::variant_result parsed =
        serialization::parse_json(*stream.value, context_.config.json, resource);
    if (parsed.failed()) {
      return {parsed.status, {}};
    }

    serialization::variant_reader reader{parsed.value};
    return assets::load_serialized<resource_type>(context_, guid, reader, resource);
  }

private:
  const resources::provider* provider_ = nullptr;
  serialization::context context_{};
};

/**
 * @brief 基于 `resources::provider` 的 JSON 资源保存器。
 *
 * `json::saver<T>` 通过 `assets::save_serialized<T>()` 把资源对象写成 JSON 文本，再按资源 GUID 通过
 * provider 写回已绑定位置。它不拥有 provider，调用方需要保证 provider 和底层 source 生命周期。
 */
template <typename T> class saver final : public resources::saver<T> {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit saver(const resources::provider& provider) : provider_(&provider) {}

  saver(const serialization::context& context, const resources::provider& provider)
      : provider_(&provider), context_(context) {}

  [[nodiscard]] error save(const resource_type& value) override {
    io::output_stream_result stream = provider_->open_write(value.guid(), provider_->resource());
    if (stream.failed()) {
      return stream.status;
    }

    core::string output{allocator<char>{provider_->resource()}};
    serialization::json_writer writer{output, context_.config.json};
    const error status = assets::save_serialized(context_, value, writer);
    if (status) {
      return status;
    }

    error written = stream.value->write_all(std::string_view{output.data(), output.size()});
    if (written) {
      return written;
    }

    return stream.value->close();
  }

private:
  const resources::provider* provider_ = nullptr;
  serialization::context context_{};
};

/** @brief 从序列化 DOM 导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target, const variant& value);

/** @brief 从 JSON 文本导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target, std::string_view text);

/** @brief 从资源字节来源读取 JSON 文本并导入资源清单。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target,
                                            resources::source& resource_source,
                                            io::path_view location,
                                            memory_resource* resource = nullptr);

/** @brief 从 JSON 文本读入资源工程描述。 */
[[nodiscard]] CAORS_API error load_project_config(project_config& target, std::string_view text);

/** @brief 从资源字节来源读取 JSON 文本并读入资源工程描述。 */
[[nodiscard]] CAORS_API error load_project_config(project_config& target,
                                                  resources::source& resource_source,
                                                  io::path_view location,
                                                  memory_resource* resource = nullptr);

/**
 * @brief 注册默认 JSON 资源类型工厂。
 *
 * 该工厂支持 `cache = "memory"` 和 `loader = "json"`。它属于 JSON 适配层，因此不放在
 * `asset_manager` 本体中。
 */
template <typename T>
[[nodiscard]] error register_type_factory(asset_manager& manager, core::string_name resource_type) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");
  return assets::register_cached_type_factory<T, loader<T>>(
      manager, resource_type, core::string_name{"memory"}, core::string_name{"json"});
}

/** @brief 注册带 option schema 的默认 JSON 资源类型工厂。 */
template <typename T>
[[nodiscard]] error register_type_factory(asset_manager& manager, core::string_name resource_type,
                                          factory_option_schema schema) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");
  return assets::register_cached_type_factory<T, loader<T>>(
      manager, resource_type, core::string_name{"memory"}, core::string_name{"json"},
      std::move(schema));
}

} // namespace caors::assets::json

#endif
