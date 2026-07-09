// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/codec.h"

#include "caors/meta/capability.h"
#include "caors/meta/type.h"

#include <type_traits>

namespace caors::serialization {

bool codec::valid() const noexcept { return ops_ != nullptr && ops_->valid(); }

type_id codec::type() const noexcept { return ops_ != nullptr ? ops_->type : type_id::invalid(); }

bool codec::can_write() const noexcept {
  return ops_ != nullptr && (ops_->write != nullptr || ops_->write_context != nullptr);
}

bool codec::can_read() const noexcept {
  return ops_ != nullptr && (ops_->read != nullptr || ops_->read_context != nullptr ||
                             ops_->read_value != nullptr || ops_->read_value_context != nullptr);
}

bool codec::can_make_value() const noexcept {
  return ops_ != nullptr && (ops_->make_value != nullptr || ops_->make_value_context != nullptr);
}

codec find_codec(type_id id) {
  if (!id) {
    return {};
  }

  return detail::get_codec_registry().find_codec(id);
}

codec find_codec(meta::type type) {
  if (!type) {
    return {};
  }

  return codec{type.get_capability<detail::codec_ops>()};
}

} // namespace caors::serialization

namespace caors::serialization::detail {

const codec_ops* ops_of(codec view) noexcept { return view.ops_; }

codec_registry& codec_registry::instance() noexcept {
  static codec_registry registry;
  return registry;
}

error codec_registry::register_codec(type_id id, const codec_ops& codec) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!codec.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return rethrow_as(meta::detail::register_capability_for<codec_ops>(id, &codec),
                    error_domain::serialization);
}

codec codec_registry::find_codec(type_id id) const {
  if (!id) {
    return {};
  }

  return codec{meta::capabilities::find<codec_ops>(id)};
}

bool codec_registry::contains(type_id id) const {
  if (!id) {
    return false;
  }

  return meta::capabilities::contains<codec_ops>(id);
}

std::size_t codec_registry::size() const noexcept { return meta::capabilities::size<codec_ops>(); }

codec_registry& get_codec_registry() noexcept { return codec_registry::instance(); }

error register_serialization_codec(type_id id, const codec_ops& codec) {
  return get_codec_registry().register_codec(id, codec);
}

} // namespace caors::serialization::detail
