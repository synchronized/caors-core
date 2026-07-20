// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/codec.h"

#include "caors/meta/type.h"
#include "caors/meta/value/any.h"

#include <type_traits>

namespace caors::serialization {

bool codec::valid() const noexcept { return ops_ != nullptr && ops_->valid(); }

type_id codec::type() const noexcept { return valid() ? type_.id() : type_id::invalid(); }

bool codec::can_write() const noexcept {
  return ops_ != nullptr && (ops_->write != nullptr || ops_->write_ref != nullptr);
}

bool codec::can_read() const noexcept { return ops_ != nullptr && ops_->read != nullptr; }

bool codec::can_make_value() const noexcept {
  return ops_ != nullptr && (ops_->make_value != nullptr || ops_->make_value_ref != nullptr);
}

type_id codec_capability::type() const noexcept {
  return valid() ? type_.id() : type_id::invalid();
}

bool codec_capability::can_write() const noexcept {
  return ops_ != nullptr && (ops_->write != nullptr || ops_->write_ref != nullptr);
}

bool codec_capability::can_read() const noexcept {
  return ops_ != nullptr && ops_->read != nullptr;
}

bool codec_capability::can_make_value() const noexcept {
  return ops_ != nullptr && (ops_->make_value != nullptr || ops_->make_value_ref != nullptr);
}

error codec_capability::write(const serialization::context& context, writer& writer,
                              const any& data) const {
  if (!valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (ops_->write != nullptr) {
    return ops_->write(context, writer, data);
  }

  if (ops_->write_ref != nullptr) {
    return ops_->write_ref(context, writer, data.ref());
  }

  return {error_domain::serialization, errc::unsupported};
}

error codec_capability::write(const serialization::context& context, writer& writer,
                              const any_ref& data) const {
  if (!valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if (ops_->write_ref != nullptr) {
    return ops_->write_ref(context, writer, data);
  }

  if (ops_->write != nullptr) {
    any copied = data.to_any();
    if (!copied.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return ops_->write(context, writer, copied);
  }

  return {error_domain::serialization, errc::unsupported};
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
  if (!valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (ops_->make_value != nullptr) {
    return ops_->make_value(context, data);
  }

  if (ops_->make_value_ref != nullptr) {
    return ops_->make_value_ref(context, data.ref());
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

variant_result codec_capability::make_value(const serialization::context& context,
                                            const any_ref& data) const {
  if (!valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (ops_->make_value_ref != nullptr) {
    return ops_->make_value_ref(context, data);
  }

  if (ops_->make_value != nullptr) {
    any copied = data.to_any();
    if (!copied.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return ops_->make_value(context, copied);
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

codec find_codec(meta::type type) {
  if (!type) {
    return {};
  }

  return codec{type, type.get_raw_ops<codec_ops>()};
}

} // namespace caors::serialization
