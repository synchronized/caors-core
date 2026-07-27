// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_EXTENSION_DATA_H_
#define CAORS_META_DETAIL_EXTENSION_DATA_H_

#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/capability_traits.h"
#include "caors/meta/identity_id.h"

#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename Extension> [[nodiscard]] extension_key extension_key_of() noexcept {
  using extension_type = std::remove_cvref_t<Extension>;
  constexpr std::string_view canonical_name = canonical_cpp_type_name<extension_type>();
  static const extension_key key{get_or_create_identity_id(
      identity_domain::extension, canonical_name.data(), canonical_name.size())};
  return key;
}

class extension_storage_base {
public:
  virtual ~extension_storage_base() = default;
};

template <typename Extension> class extension_storage final : public extension_storage_base {
public:
  template <typename... Args>
  explicit extension_storage(Args&&... args) : value(std::forward<Args>(args)...) {}

  Extension value;
};

struct extension_data {
  [[nodiscard]] bool valid() const noexcept { return key && storage != nullptr; }

  extension_key key{};
  memory::unique_ptr<extension_storage_base> storage{};
};

template <typename Extension, typename... Args>
[[nodiscard]] extension_data make_extension_data(memory::memory_resource* resource,
                                                 Args&&... args) {
  using extension_type = std::decay_t<Extension>;
  static_assert(std::is_copy_constructible_v<extension_type>,
                "Meta type extensions must be copy constructible");

  memory::memory_resource* resolved = resource != nullptr ? resource : &memory::library_resource();
  using storage_type = extension_storage<extension_type>;
  auto* storage = memory::make<storage_type>(resolved, std::forward<Args>(args)...);
  memory::unique_ptr<extension_storage_base> erased{storage};
  erased.get_deleter().reset(resolved, [](void* user_data, void* value) noexcept {
    memory::destroy(static_cast<storage_type*>(value),
                    static_cast<memory::memory_resource*>(user_data));
  });

  return extension_data{extension_key_of<extension_type>(), std::move(erased)};
}

template <typename Extension>
[[nodiscard]] const Extension* extension_value(const extension_data& data) noexcept {
  using extension_type = std::decay_t<Extension>;
  if (data.key != extension_key_of<extension_type>() || data.storage == nullptr) {
    return nullptr;
  }

  const auto* storage = static_cast<const extension_storage<extension_type>*>(data.storage.get());
  return &storage->value;
}

template <typename Extension>
[[nodiscard]] Extension* mutable_extension_value(extension_data& data) noexcept {
  using extension_type = std::decay_t<Extension>;
  if (data.key != extension_key_of<extension_type>() || data.storage == nullptr) {
    return nullptr;
  }

  auto* storage = static_cast<extension_storage<extension_type>*>(data.storage.get());
  return &storage->value;
}

} // namespace caors::meta::detail

#endif
