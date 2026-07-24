// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REQUEST_PAYLOAD_H_
#define CAORS_META_DETAIL_REQUEST_PAYLOAD_H_

#include <memory>
#include <type_traits>

namespace caors::meta::detail {

/** @brief 同步擦除请求的非拥有参数载体。 */
class request_payload final {
public:
  constexpr request_payload() noexcept = default;

  template <typename T>
    requires(!std::is_same_v<std::remove_cvref_t<T>, request_payload>)
  explicit request_payload(T& value) noexcept : data_(std::addressof(value)) {}

  template <typename T>
    requires(!std::is_lvalue_reference_v<T>)
  request_payload(T&&) = delete;

  template <typename T> [[nodiscard]] T& get() const noexcept { return *static_cast<T*>(data_); }

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return data_ != nullptr; }

private:
  void* data_ = nullptr;
};

} // namespace caors::meta::detail

#endif
