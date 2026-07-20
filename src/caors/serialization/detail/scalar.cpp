// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "internal.h"

#include <string_view>

namespace caors::serialization::detail {

namespace {

[[nodiscard]] const meta::scalar_ops* scalar_ops_of(const any_ref& value) noexcept {
  return value.valid() ? value.get_type().get_raw_ops<meta::scalar_ops>() : nullptr;
}

[[nodiscard]] bool write_scalar_string(writer& writer, const meta::scalar_ops& ops,
                                       const any_ref& data) {
  std::string_view value;
  return ops.to_string != nullptr && ops.to_string(data.data(), value) &&
         writer.write_string(value).succeeded();
}

[[nodiscard]] bool read_scalar_string(reader& reader, const meta::scalar_ops& ops,
                                      const any_ref& target,
                                      const serialization::context& context) {
  core::string value;
  if (reader.read_string(value).failed() || ops.from_string == nullptr) {
    return false;
  }

  std::string_view view{value.data(), value.size()};
  if (ops.string_borrows_input) {
    string_storage* storage = context.effective_string_view_storage();
    if (storage == nullptr) {
      return false;
    }
    view = storage->store(std::move(value));
  }

  return ops.from_string(target.mutable_data(), view);
}

} // namespace

bool try_write_scalar_value(writer& writer, const any& data) {
  return try_write_scalar_value(writer, data.ref());
}

bool try_write_scalar_value(writer& writer, const any_ref& data) {
  const meta::scalar_ops* ops = scalar_ops_of(data);
  if (ops == nullptr) {
    return false;
  }

  switch (ops->kind) {
  case meta::scalar_kind::boolean: {
    bool value = false;
    return ops->to_bool != nullptr && ops->to_bool(data.data(), value) &&
           writer.write_bool(value).succeeded();
  }
  case meta::scalar_kind::character:
  case meta::scalar_kind::string:
    return write_scalar_string(writer, *ops, data);
  case meta::scalar_kind::signed_integer: {
    std::int64_t value = 0;
    return ops->to_i64 != nullptr && ops->to_i64(data.data(), value) &&
           writer.write_i64(value).succeeded();
  }
  case meta::scalar_kind::unsigned_integer: {
    std::uint64_t value = 0;
    return ops->to_u64 != nullptr && ops->to_u64(data.data(), value) &&
           writer.write_u64(value).succeeded();
  }
  case meta::scalar_kind::floating_point: {
    double value = 0.0;
    return ops->to_f64 != nullptr && ops->to_f64(data.data(), value) &&
           writer.write_f64(value).succeeded();
  }
  case meta::scalar_kind::none:
    break;
  }

  return false;
}

variant_result make_scalar_value(const any_ref& data) {
  const meta::scalar_ops* ops = scalar_ops_of(data);
  if (ops == nullptr) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  switch (ops->kind) {
  case meta::scalar_kind::boolean: {
    bool value = false;
    if (ops->to_bool != nullptr && ops->to_bool(data.data(), value)) {
      return {{}, variant::from(value)};
    }
    break;
  }
  case meta::scalar_kind::character:
  case meta::scalar_kind::string: {
    std::string_view value;
    if (ops->to_string != nullptr && ops->to_string(data.data(), value)) {
      return {{}, variant::from(value)};
    }
    break;
  }
  case meta::scalar_kind::signed_integer: {
    std::int64_t value = 0;
    if (ops->to_i64 != nullptr && ops->to_i64(data.data(), value)) {
      return {{}, variant::from(value)};
    }
    break;
  }
  case meta::scalar_kind::unsigned_integer: {
    std::uint64_t value = 0;
    if (ops->to_u64 != nullptr && ops->to_u64(data.data(), value)) {
      return {{}, variant::from(value)};
    }
    break;
  }
  case meta::scalar_kind::floating_point: {
    double value = 0.0;
    if (ops->to_f64 != nullptr && ops->to_f64(data.data(), value)) {
      return {{}, variant::from(value)};
    }
    break;
  }
  case meta::scalar_kind::none:
    break;
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

error read_scalar_value(reader& reader, const any_ref& target,
                        const serialization::context& context) {
  const meta::scalar_ops* ops = scalar_ops_of(target);
  if (ops == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  bool written = false;
  switch (ops->kind) {
  case meta::scalar_kind::boolean: {
    bool value = false;
    written = reader.read_bool(value).succeeded() && ops->from_bool != nullptr &&
              ops->from_bool(target.mutable_data(), value);
    break;
  }
  case meta::scalar_kind::character:
  case meta::scalar_kind::string:
    written = read_scalar_string(reader, *ops, target, context);
    break;
  case meta::scalar_kind::signed_integer: {
    std::int64_t value = 0;
    if (reader.read_i64(value).succeeded()) {
      written = ops->from_i64 != nullptr && ops->from_i64(target.mutable_data(), value);
      break;
    }

    std::uint64_t unsigned_value = 0;
    written = reader.read_u64(unsigned_value).succeeded() && ops->from_u64 != nullptr &&
              ops->from_u64(target.mutable_data(), unsigned_value);
    break;
  }
  case meta::scalar_kind::unsigned_integer: {
    std::uint64_t value = 0;
    if (reader.read_u64(value).succeeded()) {
      written = ops->from_u64 != nullptr && ops->from_u64(target.mutable_data(), value);
      break;
    }

    std::int64_t signed_value = 0;
    written = reader.read_i64(signed_value).succeeded() && ops->from_i64 != nullptr &&
              ops->from_i64(target.mutable_data(), signed_value);
    break;
  }
  case meta::scalar_kind::floating_point: {
    double value = 0.0;
    if (reader.read_f64(value).succeeded()) {
      written = ops->from_f64 != nullptr && ops->from_f64(target.mutable_data(), value);
      break;
    }

    std::int64_t signed_value = 0;
    if (reader.read_i64(signed_value).succeeded()) {
      written = ops->from_i64 != nullptr && ops->from_i64(target.mutable_data(), signed_value);
      break;
    }

    std::uint64_t unsigned_value = 0;
    written = reader.read_u64(unsigned_value).succeeded() && ops->from_u64 != nullptr &&
              ops->from_u64(target.mutable_data(), unsigned_value);
    break;
  }
  case meta::scalar_kind::none:
    break;
  }

  return written ? error{} : error{error_domain::serialization, errc::unsupported};
}

} // namespace caors::serialization::detail
