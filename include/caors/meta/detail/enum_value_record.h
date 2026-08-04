// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_ENUM_VALUE_RECORD_H_
#define CAORS_META_DETAIL_ENUM_VALUE_RECORD_H_

#include "caors/memory/unique_ptr.h"
#include "caors/meta/metadata.h"

#include <cstdint>
#include <utility>

namespace caors::meta::detail {

/** @brief 枚举项反射元信息。 */
class enum_value_record final {
public:
  enum_value_record() noexcept = default;

  enum_value_record(string_name enum_name, std::int64_t signed_value, std::uint64_t unsigned_value,
                    memory::memory_resource* resource = nullptr) noexcept
      : name_(enum_name), signed_value_(signed_value), unsigned_value_(unsigned_value),
        resource_(resource != nullptr ? resource : &memory::library_resource()) {}

  enum_value_record(const enum_value_record& other)
      : name_(other.name_), signed_value_(other.signed_value_),
        unsigned_value_(other.unsigned_value_), resource_(other.resource_) {
    if (other.metadata_ != nullptr) {
      metadata_ = memory::make_unique<meta::metadata_map>(resource_, *other.metadata_);
    }
  }

  enum_value_record& operator=(const enum_value_record& other) {
    if (this == &other) {
      return *this;
    }

    name_ = other.name_;
    signed_value_ = other.signed_value_;
    unsigned_value_ = other.unsigned_value_;
    resource_ = other.resource_;
    if (other.metadata_ != nullptr) {
      metadata_ = memory::make_unique<meta::metadata_map>(resource_, *other.metadata_);
    } else {
      metadata_.reset();
    }
    return *this;
  }

  enum_value_record(enum_value_record&&) noexcept = default;
  enum_value_record& operator=(enum_value_record&&) noexcept = default;
  ~enum_value_record() = default;

  [[nodiscard]] bool valid() const noexcept { return !name_.empty(); }
  [[nodiscard]] string_name name() const noexcept { return name_; }
  [[nodiscard]] std::int64_t to_i64() const noexcept { return signed_value_; }
  [[nodiscard]] std::uint64_t to_u64() const noexcept { return unsigned_value_; }

  [[nodiscard]] const meta::metadata_map& metadata() const noexcept {
    return metadata_ != nullptr ? *metadata_ : empty_metadata_map();
  }

  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept {
    return metadata_ != nullptr ? meta::find_metadata(*metadata_, key) : nullptr;
  }

  void rebind_resource(memory::memory_resource* resource) {
    memory::memory_resource* resolved =
        resource != nullptr ? resource : &memory::library_resource();
    if (resolved == resource_) {
      return;
    }
    if (metadata_ != nullptr) {
      meta::metadata_map migrated = meta::make_metadata_map(resolved);
      for (auto& [key, value] : *metadata_) {
        migrated.emplace(key, std::move(value));
      }
      metadata_ = memory::make_unique<meta::metadata_map>(resolved, std::move(migrated));
    }
    resource_ = resolved;
  }

  void set_metadata(meta::metadata_key key, meta::metadata_value value) {
    if (metadata_ == nullptr) {
      metadata_ =
          memory::make_unique<meta::metadata_map>(resource_, meta::make_metadata_map(resource_));
    }
    meta::set_metadata(*metadata_, key, std::move(value));
  }

private:
  [[nodiscard]] static const meta::metadata_map& empty_metadata_map() noexcept {
    static const meta::metadata_map empty{
        typename meta::metadata_map::allocator_type{&memory::library_resource()}};
    return empty;
  }

  string_name name_{};
  std::int64_t signed_value_ = 0;
  std::uint64_t unsigned_value_ = 0;
  memory::unique_ptr<meta::metadata_map> metadata_{};
  memory::memory_resource* resource_ = &memory::library_resource();
};

} // namespace caors::meta::detail

#endif
