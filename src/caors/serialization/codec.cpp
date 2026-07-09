// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/codec.h"

#include "caors/meta/type.h"

#include <type_traits>

namespace caors::serialization {

bool codec::valid() const noexcept { return ops_ != nullptr && ops_->valid(); }

type_id codec::type() const noexcept { return ops_ != nullptr ? ops_->type : type_id::invalid(); }

bool codec::can_write() const noexcept { return ops_ != nullptr && ops_->write != nullptr; }

bool codec::can_read() const noexcept {
  return ops_ != nullptr && (ops_->read != nullptr || ops_->read_value != nullptr);
}

bool codec::can_make_value() const noexcept {
  return ops_ != nullptr && ops_->make_value != nullptr;
}

codec find_codec(type_id id) {
  if (!id) {
    return {};
  }

  return find_codec(meta::find(id));
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

} // namespace caors::serialization::detail
