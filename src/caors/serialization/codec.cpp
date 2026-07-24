// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/codec.h"

#include "caors/meta/type.h"
#include "caors/meta/value/any.h"

#include <type_traits>

namespace caors::serialization {

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

error codec_view::write(const serialization::context& context, writer& writer) const {
  const codec_ops* operations = ops();
  if (operations == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  const any_ref data = source();
  if (operations->write_ref != nullptr) {
    return operations->write_ref(context, writer, data);
  }

  if (operations->write != nullptr) {
    any copied = data.to_any();
    if (!copied.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    return operations->write(context, writer, copied);
  }

  return {error_domain::serialization, errc::unsupported};
}

error codec_view::read(const serialization::context& context, reader& reader) const {
  const codec_ops* operations = ops();
  if (!writable() || operations == nullptr || operations->read == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return operations->read(context, reader, source());
}

variant_result codec_view::make_value(const serialization::context& context) const {
  const codec_ops* operations = ops();
  if (operations == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  const any_ref data = source();
  if (operations->make_value_ref != nullptr) {
    return operations->make_value_ref(context, data);
  }

  if (operations->make_value != nullptr) {
    any copied = data.to_any();
    if (!copied.valid()) {
      return {{error_domain::serialization, errc::unsupported}, {}};
    }

    return operations->make_value(context, copied);
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

codec_capability find_codec(meta::type type) { return codec_capability{type}; }

} // namespace caors::serialization
