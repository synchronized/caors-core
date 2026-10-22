// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_JSON_H_
#define CAORS_ASSETS_JSON_H_

#include "caors/assets/asset_manager.h"
#include "caors/assets/project_config.h"
#include "caors/assets/project_config_loader.h"
#include "caors/assets/serialization.h"
#include "caors/assets/type_factory.h"
#include "caors/resources/byte_reader.h"
#include "caors/resources/manifest.h"
#include "caors/resources/source.h"
#include "caors/serialization/operations.h"

#include <string_view>
#include <type_traits>

namespace caors::assets::json {

/** @brief JSON 格式的工程资源配置读取器。 */
class CAORS_API project_config_loader final : public assets::project_config_loader {
public:
  [[nodiscard]] error load(project_config& target, resources::source& source,
                           io::location_view location,
                           memory_resource* scratch_resource = nullptr) override;
};

/**
 * @brief 基于 `resources::provider` 的 JSON 资源加载器。
 *
 * `json::loader<T>` 通过 provider 按 GUID 定位字节来源和位置，读取 JSON 文本后复用
 * `assets::load_serialized<T>()` 反序列化为资源对象。它不拥有 provider，调用方需要保证 provider
 * 生命周期。
 */
template <typename T> class loader final : public resources::sync_loader_adapter<T> {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit loader(const resources::provider& provider, serialization::json_options options = {})
      : provider_(&provider), options_(options) {}

  loader(const serialization::context& context, const resources::provider& provider,
         serialization::json_options options = {})
      : provider_(&provider), context_(context), options_(options) {}

  [[nodiscard]] resources::load_result<resource_type>
  load(resources::resource_guid guid, memory_resource* result_resource = nullptr) override {
    serialization::detail::scratch_memory_scope scratch_scope{
        context_.effective_scratch_resource()};
    io::input_stream_result stream = provider_->open_read(guid, scratch_scope.resource());
    if (stream.failed()) {
      return {stream.status, {}};
    }

    serialization::variant_result parsed =
        serialization::parse_json(*stream.value, options_, scratch_scope.resource());
    if (parsed.failed()) {
      return {parsed.status, {}};
    }

    serialization::variant_reader reader{parsed.value, scratch_scope.resource()};
    return assets::load_serialized<resource_type>(context_, guid, reader, result_resource);
  }

private:
  const resources::provider* provider_ = nullptr;
  serialization::context context_{};
  serialization::json_options options_{};
};

/**
 * @brief 从统一 byte_reader 异步读取并构造 JSON 资源；来源取自本次请求的 provider_snapshot。
 * @note 借用 reader，须覆盖实际操作。解析经 context.worker，资源提交经 manager 的 completion。
 * 不捕获实时 provider，不在 worker 上阻塞等待读取；max_bytes 为单次结果预算。
 */
template <typename T> class byte_loader final : public resources::loader<T> {
  struct operation {
    resources::async_load_context context;
    resources::load_completion<T> completion;
    io::io_request<io::byte_buffer> reading;
    using stop_callback = std::stop_callback<std::function<void()>>;
    std::unique_ptr<stop_callback> stop;
  };

public:
  explicit byte_loader(resources::byte_reader& reader, std::uint64_t max_bytes = 64 * 1024 * 1024,
                       serialization::json_options options = {})
      : reader_(&reader), max_bytes_(max_bytes), options_(options) {}
  [[nodiscard]] error start(resources::async_load_context context,
                            resources::load_completion<T> completion) override {
    auto snapshot = context.template snapshot_as<resources::provider_snapshot>();
    if (!snapshot)
      return {error_domain::resources, errc::unsupported};
    auto source = snapshot->resolve(context.guid);
    if (!source)
      return source.error();
    auto op = std::make_shared<operation>();
    op->context = std::move(context);
    op->completion = std::move(completion);
    auto reading = resources::request_bytes(
        *reader_, {std::move(source.value), op->context.worker, max_bytes_, op->context.resource},
        op->context.worker, [op, options = options_](const auto& bytes) {
          resources::load_result<T> result;
#if CAORS_HAS_EXCEPTIONS
          try {
#endif
            if (op->context.stop.stop_requested()) {
              result.status = {error_domain::resources, errc::cancelled};
            } else if (!bytes) {
              result.status = resources::read_error(bytes.error());
            } else {
              const auto* data =
                  bytes.value.empty() ? "" : reinterpret_cast<const char*>(bytes.value.data());
              serialization::context context;
              context.resolvers.memory.result_resource = op->context.resource;
              serialization::detail::scratch_memory_scope scratch{
                  context.effective_scratch_resource()};
              serialization::json_reader reader{std::string_view{data, bytes.value.size()}, options,
                                                scratch.resource()};
              result = assets::load_serialized<T>(context, op->context.guid, reader,
                                                  op->context.resource);
            }
#if CAORS_HAS_EXCEPTIONS
          } catch (const std::bad_alloc&) {
            result.status = {error_domain::resources, errc::out_of_memory};
          } catch (...) {
            result.status = {error_domain::resources, errc::backend_failure};
          }
#endif
          auto completion = std::move(op->completion);
          (void)completion.complete(std::move(result));
        });
    if (!reading)
      return resources::read_error(reading.error());
    op->reading = std::move(reading.value);
    std::weak_ptr<operation> weak = op;
    op->stop = std::make_unique<typename operation::stop_callback>(op->context.stop, [weak] {
      if (auto op = weak.lock())
        op->reading.cancel();
    });
    return {};
  }

private:
  resources::byte_reader* reader_;
  std::uint64_t max_bytes_;
  serialization::json_options options_;
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

  explicit saver(const resources::provider& provider, serialization::json_options options = {})
      : provider_(&provider), options_(options) {}

  saver(const serialization::context& context, const resources::provider& provider,
        serialization::json_options options = {})
      : provider_(&provider), context_(context), options_(options) {}

  [[nodiscard]] error save(const resource_type& value) override {
    io::output_stream_result stream = provider_->open_write(value.guid(), provider_->resource());
    if (stream.failed()) {
      return stream.status;
    }

    serialization::json_writer writer{*stream.value, options_};
    const error status = assets::save_serialized(context_, value, writer);
    if (status) {
      return status;
    }

    return stream.value->close();
  }

private:
  const resources::provider* provider_ = nullptr;
  serialization::context context_{};
  serialization::json_options options_{};
};

/** @brief 从序列化 DOM 导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target, const variant& value);

/** @brief 从 JSON 文本导入资源清单。导入会新增或替换 target 中的条目，不会清空已有条目。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target, std::string_view text);

/** @brief 从资源字节来源读取 JSON 文本并导入资源清单。 */
[[nodiscard]] CAORS_API error load_manifest(resources::manifest& target,
                                            resources::source& resource_source,
                                            io::location_view location,
                                            memory_resource* scratch_resource = nullptr);

/** @brief 从 JSON 文本读入资源工程描述。 */
[[nodiscard]] CAORS_API error load_project_config(project_config& target, std::string_view text);

/** @brief 从资源字节来源读取 JSON 文本并读入资源工程描述。 */
[[nodiscard]] CAORS_API error load_project_config(project_config& target,
                                                  resources::source& resource_source,
                                                  io::location_view location,
                                                  memory_resource* scratch_resource = nullptr);

/** @brief 把资源工程描述写入 JSON 字符串。 */
[[nodiscard]] CAORS_API error save_project_config_to(const project_config& source, string& output,
                                                     serialization::json_options options = {});

/** @brief 把资源工程描述写入资源字节来源。 */
[[nodiscard]] CAORS_API error save_project_config_to(const project_config& source,
                                                     resources::source& resource_source,
                                                     io::location_view location,
                                                     serialization::json_options options = {},
                                                     memory_resource* scratch_resource = nullptr);

/** @brief 把资源工程描述保存为 JSON 字符串。 */
[[nodiscard]] CAORS_API serialization::serialization_text_result
save_project_config(const project_config& source, serialization::json_options options = {});

/**
 * @brief 注册默认 JSON 资源类型工厂。
 *
 * 该工厂支持 `cache = "memory"` 和 `loader = "json"`。它属于 JSON 适配层，因此不放在
 * `asset_manager` 本体中。
 */
template <typename T>
[[nodiscard]] error register_type_factory(project_builder project,
                                          core::string_name resource_type) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");
  return assets::register_cached_type_factory<T, loader<T>>(
      project, resource_type, core::string_name{"memory"}, core::string_name{"json"});
}

} // namespace caors::assets::json

#endif
