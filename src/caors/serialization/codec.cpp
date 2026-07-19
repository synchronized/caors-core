// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/codec.h"

#include "caors/meta/type.h"

#include <type_traits>

namespace caors::serialization {

bool codec::valid() const noexcept { return ops_ != nullptr && ops_->valid(); }

type_id codec::type() const noexcept { return valid() ? type_.id() : type_id::invalid(); }

bool codec::can_write() const noexcept { return ops_ != nullptr && ops_->write != nullptr; }

bool codec::can_read() const noexcept { return ops_ != nullptr && ops_->read != nullptr; }

bool codec::can_make_value() const noexcept {
  return ops_ != nullptr && ops_->make_value != nullptr;
}

type_id codec_capability::type() const noexcept {
  return valid() ? type_.id() : type_id::invalid();
}

bool codec_capability::can_write() const noexcept {
  return ops_ != nullptr && ops_->write != nullptr;
}

bool codec_capability::can_read() const noexcept {
  return ops_ != nullptr && ops_->read != nullptr;
}

bool codec_capability::can_make_value() const noexcept {
  return ops_ != nullptr && ops_->make_value != nullptr;
}

error codec_capability::write(const serialization::context& context, writer& writer,
                              const any& data) const {
  if (!valid() || ops_->write == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return ops_->write(context, writer, data);
}

error codec_capability::read(const serialization::context& context, reader& reader,
                             const any_ref& target) const {
  if (!valid() || ops_->read == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return ops_->read(context, reader, target);
}

variant_result codec_capability::make_value(const serialization::context& context,
                                            const any& data) const {
  if (!valid() || ops_->make_value == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return ops_->make_value(context, data);
}

codec find_codec(meta::type type) {
  if (!type) {
    return {};
  }

  return codec{type, type.get_raw_ops<codec_ops>()};
}

} // namespace caors::serialization
