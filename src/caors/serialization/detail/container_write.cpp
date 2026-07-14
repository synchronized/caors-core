// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "helpers.h"

namespace caors::serialization::detail {

[[nodiscard]] error write_sequence_value(writer& writer, const meta::sequence_view& view,
                                         const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(*size);
  if (status) {
    return status;
  }

  for (std::size_t index = 0; index < *size; ++index) {
    status = writer.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    status = write_serialized_value(writer, element, nested_context.value);
    if (status) {
      return status;
    }

    status = writer.end_element();
    if (status) {
      return status;
    }
  }

  return writer.end_array();
}

[[nodiscard]] error write_set_value(writer& output, const meta::set_view& view,
                                    const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = output.begin_array(*size);
  if (status) {
    return status;
  }

  struct visitor_context {
    writer* writer = nullptr;
    const serialization::context* context = nullptr;
    std::size_t index = 0;
    error status{};
  } visitor{&output, &context, 0, {}};

  const bool completed = view.for_each_ref(
      [](any_ref element, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        visitor->status = visitor->writer->begin_element(visitor->index);
        if (visitor->status) {
          return false;
        }

        result<serialization::context> nested_context = visitor->context->enter_nested_value();
        if (nested_context.failed()) {
          visitor->status = nested_context.status;
          return false;
        }

        visitor->status = write_serialized_value(*visitor->writer, element, nested_context.value);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_element();
        if (visitor->status) {
          return false;
        }

        ++visitor->index;
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.status ? visitor.status : error{error_domain::serialization, errc::unsupported};
  }

  return output.end_array();
}

[[nodiscard]] error write_associative_value(writer& output, const meta::associative_view& view,
                                            const serialization::context& context) {
  const std::optional<std::size_t> size = view.size();
  if (!view.valid() || !size.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = output.begin_dictionary(*size);
  if (status) {
    return status;
  }

  struct visitor_context {
    writer* writer = nullptr;
    const serialization::context* context = nullptr;
    std::size_t index = 0;
    error status{};
  } visitor{&output, &context, 0, {}};

  const bool completed = view.for_each_ref(
      [](any_ref key, any_ref value, void* user_data) -> bool {
        auto* visitor = static_cast<visitor_context*>(user_data);
        visitor->status = visitor->writer->begin_entry(visitor->index);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->begin_key();
        if (visitor->status) {
          return false;
        }

        result<serialization::context> nested_context = visitor->context->enter_nested_value();
        if (nested_context.failed()) {
          visitor->status = nested_context.status;
          return false;
        }

        visitor->status = write_serialized_value(*visitor->writer, key, nested_context.value);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_key();
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->begin_value();
        if (visitor->status) {
          return false;
        }

        visitor->status = write_serialized_value(*visitor->writer, value, nested_context.value);
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_value();
        if (visitor->status) {
          return false;
        }

        visitor->status = visitor->writer->end_entry();
        if (visitor->status) {
          return false;
        }

        ++visitor->index;
        return true;
      },
      &visitor);

  if (!completed) {
    return visitor.status ? visitor.status : error{error_domain::serialization, errc::unsupported};
  }

  return output.end_dictionary();
}

[[nodiscard]] error write_pair_value(writer& writer, const meta::pair_view& view,
                                     const serialization::context& context) {
  if (!view.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(2);
  if (status) {
    return status;
  }

  status = writer.begin_element(0);
  if (status) {
    return status;
  }

  any_ref first = view.first_ref();
  if (!first.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return nested_context.status;
  }

  status = write_serialized_value(writer, first, nested_context.value);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  status = writer.begin_element(1);
  if (status) {
    return status;
  }

  any_ref second = view.second_ref();
  if (!second.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  status = write_serialized_value(writer, second, nested_context.value);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  return writer.end_array();
}

[[nodiscard]] error write_tuple_value(writer& writer, const meta::tuple_view& view,
                                      const serialization::context& context) {
  if (!view.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  const std::size_t size = view.size();
  error status = writer.begin_array(size);
  if (status) {
    return status;
  }

  for (std::size_t index = 0; index < size; ++index) {
    status = writer.begin_element(index);
    if (status) {
      return status;
    }

    any_ref element = view.element_ref(index);
    if (!element.valid()) {
      return {error_domain::serialization, errc::unsupported};
    }

    result<serialization::context> nested_context = context.enter_nested_value();
    if (nested_context.failed()) {
      return nested_context.status;
    }

    status = write_serialized_value(writer, element, nested_context.value);
    if (status) {
      return status;
    }

    status = writer.end_element();
    if (status) {
      return status;
    }
  }

  return writer.end_array();
}

[[nodiscard]] error write_variant_value(writer& writer, const meta::variant_view& view,
                                        const serialization::context& context) {
  const std::optional<std::size_t> active = view.index();
  if (!view.valid() || !active.has_value()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = writer.begin_array(2);
  if (status) {
    return status;
  }

  status = writer.begin_element(0);
  if (status) {
    return status;
  }

  status = writer.write_u64(static_cast<std::uint64_t>(*active));
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  status = writer.begin_element(1);
  if (status) {
    return status;
  }

  any_ref value = view.value_ref();
  if (!value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<serialization::context> nested_context = context.enter_nested_value();
  if (nested_context.failed()) {
    return nested_context.status;
  }

  status = write_serialized_value(writer, value, nested_context.value);
  if (status) {
    return status;
  }

  status = writer.end_element();
  if (status) {
    return status;
  }

  return writer.end_array();
}

} // namespace caors::serialization::detail
