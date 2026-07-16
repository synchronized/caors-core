// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_IO_H_
#define CAORS_ASSETS_IO_H_

#include "caors/resources/io.h"
#include "caors/resources/manifest.h"
#include "caors/resources/source.h"
#include "caors/serialization/operations.h"

#include <string_view>
#include <type_traits>

namespace caors::assets {

/**
 * @brief 从序列化 reader 构造资源。
 *
 * 该 helper 面向简单资源类型：先用指定内存资源默认构造 `T`，写入 GUID，再通过现有
 * `serialization::deserialize()` 读回资源字段。非默认可构造或有自定义加载流程的资源应实现
 * `resources::loader<T>`。
 */
template <typename T>
[[nodiscard]] resources::load_result<T>
load_serialized(const serialization::context& context, resources::resource_guid guid,
                serialization::reader& reader, memory_resource* resource = nullptr);

template <typename T>
[[nodiscard]] resources::load_result<T> load_serialized(resources::resource_guid guid,
                                                        serialization::reader& reader,
                                                        memory_resource* resource = nullptr) {
  return load_serialized<T>(serialization::context{}, guid, reader, resource);
}

template <typename T>
[[nodiscard]] resources::load_result<T>
load_serialized(const serialization::context& context, resources::resource_guid guid,
                serialization::reader& reader, memory_resource* resource) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");
  static_assert(std::is_default_constructible_v<T>,
                "load_serialized<T>() requires default constructible resources");

  if (!guid) {
    return {{error_domain::resources, errc::invalid_argument}, {}};
  }

  memory::unique_ptr<T> value = memory::make_unique<T>(resource);
  if (value == nullptr) {
    return {{error_domain::resources, errc::out_of_memory}, {}};
  }

  if (value->has_guid() && value->guid() != guid) {
    return {{error_domain::resources, errc::inconsistent_type}, {}};
  }
  if (!value->has_guid()) {
    value->set_guid(guid);
  }

  const error status = serialization::deserialize(context, *value, reader);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(value)};
}

/**
 * @brief 基于 `resources::provider` 的 JSON 资源加载器。
 *
 * `json_loader<T>` 通过 provider 按 GUID 定位字节来源和位置，读取 JSON 文本后复用
 * `load_serialized<T>()` 反序列化为资源对象。它不拥有 provider，调用方需要保证 provider 生命周期。
 */
template <typename T> class json_loader final : public resources::loader<T> {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit json_loader(const resources::provider& provider) : provider_(&provider) {}

  json_loader(const serialization::context& context, const resources::provider& provider)
      : provider_(&provider), context_(context) {}

  [[nodiscard]] resources::load_result<resource_type>
  load(resources::resource_guid guid, memory_resource* resource = nullptr) override {
    io::byte_buffer_result bytes = provider_->read(guid, resource);
    if (bytes.failed()) {
      return {bytes.status, {}};
    }

    const std::string_view text{reinterpret_cast<const char*>(bytes.value.data()),
                                bytes.value.size()};
    serialization::variant_result parsed = serialization::parse_json(text);
    if (parsed.failed()) {
      return {parsed.status, {}};
    }

    serialization::variant_reader reader{parsed.value};
    return load_serialized<resource_type>(context_, guid, reader, resource);
  }

private:
  const resources::provider* provider_ = nullptr;
  serialization::context context_{};
};

/**
 * @brief 使用序列化 writer 保存资源字段。
 *
 * 资源必须携带非 nil GUID。当前 helper 只写资源对象本体字段，资源身份由调用方的数据库、路径或包索引
 * 维护。
 */
template <typename T>
[[nodiscard]] error save_serialized(const serialization::context& context, const T& value,
                                    serialization::writer& writer);

template <typename T>
[[nodiscard]] error save_serialized(const T& value, serialization::writer& writer) {
  return save_serialized(serialization::context{}, value, writer);
}

template <typename T>
[[nodiscard]] error save_serialized(const serialization::context& context, const T& value,
                                    serialization::writer& writer) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

  if (!value.has_guid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  return serialization::serialize(context, value, writer);
}

/**
 * @brief 基于 `resources::provider` 的 JSON 资源保存器。
 *
 * `json_saver<T>` 通过 `save_serialized<T>()` 把资源对象写成 JSON 文本，再按资源 GUID 通过 provider
 * 写回已绑定位置。它不拥有 provider，调用方需要保证 provider 和底层 source 生命周期。
 */
template <typename T> class json_saver final : public resources::saver<T> {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit json_saver(const resources::provider& provider) : provider_(&provider) {}

  json_saver(const serialization::context& context, const resources::provider& provider)
      : provider_(&provider), context_(context) {}

  [[nodiscard]] error save(const resource_type& value) override {
    core::string output{allocator<char>{provider_->resource()}};
    serialization::json_writer writer{output};
    const error status = save_serialized(context_, value, writer);
    if (status) {
      return status;
    }

    return provider_->write(value.guid(), std::string_view{output.data(), output.size()});
  }

private:
  const resources::provider* provider_ = nullptr;
  serialization::context context_{};
};

/** @brief 从序列化 DOM 导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target, const variant& value);

/** @brief 从 JSON 文本导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest_json(resources::manifest& target,
                                                 std::string_view text);

/** @brief 从资源字节来源读取 JSON 文本并导入资源清单。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target,
                                            resources::source& resource_source,
                                            io::path_view location,
                                            memory_resource* resource = nullptr);

} // namespace caors::assets

#endif
