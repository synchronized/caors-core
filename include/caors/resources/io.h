// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_IO_H_
#define CAORS_RESOURCES_IO_H_

#include "caors/resources/cache.h"
#include "caors/resources/manifest.h"
#include "caors/resources/source.h"
#include "caors/result.h"
#include "caors/serialization/operations.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::resources {

template <typename T> using load_result = result<memory::unique_ptr<T>>;

/**
 * @brief 指定资源类型的加载器接口。
 *
 * `loader<T>` 不绑定文件路径、资源包或异步系统。实现类只需要按 GUID 构造资源并返回所有权；
 * 调用方可选择直接持有结果，或通过 `load_into()` 转入 `resources::cache<T>`。
 */
template <typename T> class loader {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  loader() = default;
  virtual ~loader() = default;

  loader(const loader&) = delete;
  loader& operator=(const loader&) = delete;
  loader(loader&&) = delete;
  loader& operator=(loader&&) = delete;

  /** @brief 按 GUID 加载资源，返回资源所有权。 */
  [[nodiscard]] virtual load_result<resource_type> load(resource_guid guid,
                                                        memory_resource* resource = nullptr) = 0;
};

/**
 * @brief 指定资源类型的保存器接口。
 *
 * `saver<T>` 只表达“保存一个已有资源”的动作，不规定目标位置、格式或同步策略。
 */
template <typename T> class saver {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  saver() = default;
  virtual ~saver() = default;

  saver(const saver&) = delete;
  saver& operator=(const saver&) = delete;
  saver(saver&&) = delete;
  saver& operator=(saver&&) = delete;

  /** @brief 保存资源。资源应携带非 nil GUID。 */
  [[nodiscard]] virtual error save(const resource_type& value) = 0;
};

/**
 * @brief 使用加载器把资源加载进 cache。
 *
 * 如果 cache 已包含该 GUID，直接返回已有对象。否则调用 loader 加载资源并把所有权转入 cache。
 */
template <typename T>
[[nodiscard]] result<T*> load_into(cache<T>& target, loader<T>& source, resource_guid guid) {
  if (!guid) {
    return {{error_domain::resources, errc::invalid_argument}, nullptr};
  }

  if (T* existing = target.find(guid)) {
    return {{}, existing};
  }

  load_result<T> loaded = source.load(guid, target.resource());
  if (loaded.failed()) {
    return {loaded.status, nullptr};
  }

  if (loaded.value == nullptr) {
    return {{error_domain::resources, errc::backend_failure}, nullptr};
  }

  if (!loaded.value->has_guid()) {
    loaded.value->set_guid(guid);
  }
  if (loaded.value->guid() != guid) {
    return {{error_domain::resources, errc::inconsistent_type}, nullptr};
  }

  T* stored = loaded.value.get();
  if (!target.insert(std::move(loaded.value))) {
    return {{error_domain::resources, errc::already_exists}, nullptr};
  }

  return {{}, stored};
}

/**
 * @brief 从序列化 reader 构造资源。
 *
 * 该 helper 面向简单资源类型：先用指定内存资源默认构造 `T`，写入 GUID，再通过现有
 * `serialization::deserialize()` 读回资源字段。非默认可构造或有自定义加载流程的资源应实现
 * `loader<T>`。
 */
template <typename T>
[[nodiscard]] load_result<T> load_serialized(resource_guid guid, serialization::reader& reader,
                                             const serialization::context& context = {},
                                             memory_resource* resource = nullptr) {
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

  const error status = serialization::deserialize(*value, reader, context);
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
template <typename T> class json_loader final : public loader<T> {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit json_loader(const provider& provider,
                       serialization::context context = serialization::context{})
      : provider_(&provider), context_(context) {}

  [[nodiscard]] load_result<resource_type> load(resource_guid guid,
                                                memory_resource* resource = nullptr) override {
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
    return load_serialized<resource_type>(guid, reader, context_, resource);
  }

private:
  const provider* provider_ = nullptr;
  serialization::context context_{};
};

/**
 * @brief 使用序列化 writer 保存资源字段。
 *
 * 资源必须携带非 nil GUID。当前 helper 只写资源对象本体字段，资源身份由调用方的数据库、路径或包索引
 * 维护。
 */
template <typename T>
[[nodiscard]] error save_serialized(const T& value, serialization::writer& writer,
                                    const serialization::context& context = {}) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

  if (!value.has_guid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  return serialization::serialize(value, writer, context);
}

/**
 * @brief 基于 `resources::provider` 的 JSON 资源保存器。
 *
 * `json_saver<T>` 通过 `save_serialized<T>()` 把资源对象写成 JSON 文本，再按资源 GUID 通过 provider
 * 写回已绑定位置。它不拥有 provider，调用方需要保证 provider 和底层 source 生命周期。
 */
template <typename T> class json_saver final : public saver<T> {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit json_saver(const provider& provider,
                      serialization::context context = serialization::context{})
      : provider_(&provider), context_(context) {}

  [[nodiscard]] error save(const resource_type& value) override {
    core::string output{allocator<char>{provider_->resource()}};
    serialization::json_writer writer{output};
    const error status = save_serialized(value, writer, context_);
    if (status) {
      return status;
    }

    return provider_->write(value.guid(), std::string_view{output.data(), output.size()});
  }

private:
  const provider* provider_ = nullptr;
  serialization::context context_{};
};

/** @brief 从序列化 DOM 导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest(manifest& target, const variant& value);

/** @brief 从 JSON 文本导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest_json(manifest& target, std::string_view text);

/** @brief 从资源字节来源读取 JSON 文本并导入资源清单。 */
[[nodiscard]] CAORS_API error load_manifest(manifest& target, source& resource_source,
                                            io::path_view location,
                                            memory_resource* resource = nullptr);

} // namespace caors::resources

namespace caors {

template <typename T> using resource_load_result = resources::load_result<T>;
template <typename T> using resource_json_loader = resources::json_loader<T>;
template <typename T> using resource_json_saver = resources::json_saver<T>;
template <typename T> using resource_loader = resources::loader<T>;
template <typename T> using resource_saver = resources::saver<T>;

using resources::load_into;
using resources::load_manifest;
using resources::load_manifest_json;
using resources::load_serialized;
using resources::save_serialized;

} // namespace caors

#endif
