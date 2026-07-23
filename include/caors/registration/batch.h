// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REGISTRATION_BATCH_H_
#define CAORS_REGISTRATION_BATCH_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/context.h"
#include "caors/meta/object.h"
#include "caors/meta/type_id.h"
#include "caors/registration/enum_builder.h"
#include "caors/registration/type_builder.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::registration {

/** @brief 批量注册项类型。 */
enum class batch_item_kind {
  type,
  enumeration,
};

/** @brief 批量注册失败诊断。 */
struct batch_diagnostic {
  batch_item_kind kind = batch_item_kind::type;
  std::size_t index = static_cast<std::size_t>(-1);
  type_id id{};
  core::string_name name{};
  error status{};
};

/** @brief 返回批量注册项类型的调试文本。 */
[[nodiscard]] inline std::string_view to_string(batch_item_kind kind) noexcept {
  switch (kind) {
  case batch_item_kind::type:
    return "type";
  case batch_item_kind::enumeration:
    return "enum";
  }

  return "unknown";
}

/** @brief 按顺序提交多个 registration builder 的批量注册器。 */
class batch_builder final {
  class item_base;
  template <typename Builder> class item_model;

public:
  explicit batch_builder(meta::context& ctx) noexcept : context_(&ctx) {}

  batch_builder(const batch_builder&) = delete;
  batch_builder& operator=(const batch_builder&) = delete;
  batch_builder(batch_builder&&) noexcept = default;
  batch_builder& operator=(batch_builder&&) noexcept = default;

  /** @brief 追加普通类型注册项。 */
  template <typename T> type_builder<T>& type_(std::string_view name) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    auto builder = registration::type_<value_type>(*context_, name);
    auto item = make_item<type_builder<value_type>>(batch_item_kind::type,
                                                    meta::detail::static_type_id<value_type>(),
                                                    core::string_name{name}, std::move(builder));
    auto* model = static_cast<item_model<type_builder<value_type>>*>(item.get());
    type_builder<value_type>& result = model->builder;
    items_.push_back(std::move(item));
    return result;
  }

  /** @brief 追加使用对象静态身份名称的注册项。 */
  template <typename T>
    requires(std::is_base_of_v<meta::object, T>)
  type_builder<T>& type_() {
    return type_<T>(traits::type_identity<T>::name());
  }

  /** @brief 追加枚举类型注册项。 */
  template <typename T> enum_builder<T>& enum_(std::string_view name) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    auto builder = registration::enum_<value_type>(*context_, name);
    auto item = make_item<enum_builder<value_type>>(batch_item_kind::enumeration,
                                                    meta::detail::static_type_id<value_type>(),
                                                    core::string_name{name}, std::move(builder));
    auto* model = static_cast<item_model<enum_builder<value_type>>*>(item.get());
    enum_builder<value_type>& result = model->builder;
    items_.push_back(std::move(item));
    return result;
  }

  /** @brief 顺序提交注册项，遇到第一处失败时停止。 */
  [[nodiscard]] error commit() {
    if (committed_) {
      return status_;
    }

    committed_ = true;
    failure_ = {};
    status_ = {};

    for (std::size_t index = 0; index < items_.size(); ++index) {
      item_base& item = *items_[index];
      const error item_status = item.commit();
      if (item_status) {
        status_ = item_status;
        failure_ = batch_diagnostic{item.kind(), index, item.id(), item.name(), item_status};
        return status_;
      }
    }

    return status_;
  }

  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept { return !status_; }
  [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
  [[nodiscard]] bool has_diagnostic() const noexcept { return static_cast<bool>(failure_.status); }
  [[nodiscard]] const batch_diagnostic* diagnostic() const noexcept {
    return has_diagnostic() ? &failure_ : nullptr;
  }
  [[nodiscard]] batch_item_kind failed_kind() const noexcept { return failure_.kind; }
  [[nodiscard]] std::size_t failed_index() const noexcept { return failure_.index; }
  [[nodiscard]] type_id failed_type_id() const noexcept { return failure_.id; }
  [[nodiscard]] core::string_name failed_name() const noexcept { return failure_.name; }

  /** @brief 返回首个失败项的可读诊断文本；没有失败时返回空字符串。 */
  [[nodiscard]] core::string diagnostic_message() const {
    if (!has_diagnostic()) {
      return core::make_string(&memory::library_resource());
    }

    core::string message = core::make_string(&memory::library_resource());
    message += "registration batch failed at item ";
    message += std::to_string(failure_.index).c_str();
    message += ": ";
    message += to_string(failure_.kind);
    if (failure_.name) {
      message += " ";
      message += failure_.name.view();
    }
    message += " ";
    message += failure_.status.code().message().c_str();
    return message;
  }

private:
  class item_base {
  public:
    virtual ~item_base() = default;

    [[nodiscard]] virtual error commit() = 0;
    [[nodiscard]] virtual batch_item_kind kind() const noexcept = 0;
    [[nodiscard]] virtual type_id id() const noexcept = 0;
    [[nodiscard]] virtual core::string_name name() const noexcept = 0;
  };

  template <typename Builder> class item_model final : public item_base {
  public:
    item_model(batch_item_kind kind_value, type_id id_value, core::string_name name_value,
               Builder&& builder_value)
        : builder(std::move(builder_value)), kind_(kind_value), id_(id_value), name_(name_value) {}

    [[nodiscard]] error commit() override { return builder.commit(); }
    [[nodiscard]] batch_item_kind kind() const noexcept override { return kind_; }
    [[nodiscard]] type_id id() const noexcept override { return id_; }
    [[nodiscard]] core::string_name name() const noexcept override { return name_; }

    Builder builder;

  private:
    batch_item_kind kind_ = batch_item_kind::type;
    type_id id_{};
    core::string_name name_{};
  };

  template <typename Builder>
  [[nodiscard]] memory::unique_ptr<item_base> make_item(batch_item_kind kind, type_id id,
                                                        core::string_name name, Builder&& builder) {
    using item_type = item_model<Builder>;
    auto* raw =
        memory::make<item_type>(&memory::library_resource(), kind, id, name, std::move(builder));
    memory::unique_ptr<item_base> result{raw};
    result.get_deleter().reset(
        &memory::library_resource(), [](void* user_data, void* value) noexcept {
          memory::destroy(static_cast<item_type*>(value), static_cast<memory_resource*>(user_data));
        });
    return result;
  }

  using erased_item_ptr = memory::unique_ptr<item_base>;
  using item_list = core::vector<erased_item_ptr>;

  meta::context* context_ = nullptr;
  item_list items_{item_list::allocator_type{&memory::library_resource()}};
  error status_{};
  batch_diagnostic failure_{};
  bool committed_ = false;
};

/** @brief 在指定上下文中创建批量注册器。 */
[[nodiscard]] inline batch_builder batch(meta::context& ctx) noexcept { return batch_builder{ctx}; }

/** @brief 使用默认上下文创建批量注册器。 */
[[nodiscard]] inline batch_builder batch() noexcept { return batch(meta::default_context()); }

} // namespace caors::registration

#endif
