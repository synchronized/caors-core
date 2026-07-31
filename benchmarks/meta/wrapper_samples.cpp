// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/config.h"
#include "caors/meta/context.h"
#include "caors/meta/detail/method_record.h"
#include "caors/meta/detail/property_record.h"
#include "caors/meta/detail/static_type_record_model.h"
#include "caors/meta/property_accessor.h"
#include "caors/registration.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace caors::benchmarks {
namespace {

constexpr std::size_t registration_sample_count = 32;

template <std::size_t Index> struct property_registration_sample {
  int member = 0;
  int accessor = 0;

  [[nodiscard]] int get_accessor() const noexcept { return accessor; }
  void set_accessor(int value) noexcept { accessor = value; }
  [[nodiscard]] int get_readonly() const noexcept { return member; }
};

template <std::size_t Index> struct method_registration_sample {
  int value = 0;

  [[nodiscard]] int add(int argument) noexcept { return value + argument; }
  [[nodiscard]] int add(int left, int right) const noexcept { return value + left + right; }
  void reset() noexcept { value = 0; }
};

struct default_resource_scope final {
  explicit default_resource_scope(memory_resource* resource) noexcept
      : previous(set_default_memory_resource(resource)) {}

  default_resource_scope(const default_resource_scope&) = delete;
  default_resource_scope& operator=(const default_resource_scope&) = delete;
  ~default_resource_scope() { (void)set_default_memory_resource(previous); }

  memory_resource* previous = nullptr;
};

[[nodiscard]] std::string_view make_type_name(std::span<char> buffer, std::string_view prefix,
                                              std::size_t index) {
  if (buffer.size() <= prefix.size()) {
    return {};
  }

  std::copy(prefix.begin(), prefix.end(), buffer.begin());
  const std::to_chars_result result =
      std::to_chars(buffer.data() + prefix.size(), buffer.data() + buffer.size(), index);
  return result.ec == std::errc{}
             ? std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())}
             : std::string_view{};
}

template <typename T> void warm_registration_type(std::string_view name) {
  (void)meta::detail::static_type_record_ptr_of<T>();
  (void)string_name{name};
}

template <std::size_t... Index>
void warm_property_registration_types(std::index_sequence<Index...>) {
  std::array<char, 64> name{};
  (warm_registration_type<property_registration_sample<Index>>(
       make_type_name(name, "caors.benchmark.wrapper.property.", Index)),
   ...);
}

template <std::size_t... Index> void warm_method_registration_types(std::index_sequence<Index...>) {
  std::array<char, 64> name{};
  (warm_registration_type<method_registration_sample<Index>>(
       make_type_name(name, "caors.benchmark.wrapper.method.", Index)),
   ...);
}

template <std::size_t Index> [[nodiscard]] error register_property_sample(meta::context& context) {
  using sample_type = property_registration_sample<Index>;
  std::array<char, 64> name{};
  const std::string_view type_name =
      make_type_name(name, "caors.benchmark.wrapper.property.", Index);
  return registration::type_<sample_type>(context, type_name)
      .meta()
      .property("member", &sample_type::member)
      .property("accessor", &sample_type::get_accessor, &sample_type::set_accessor)
      .property_readonly("readonly", &sample_type::get_readonly)
      .commit();
}

template <std::size_t Index> [[nodiscard]] error register_method_sample(meta::context& context) {
  using sample_type = method_registration_sample<Index>;
  using unary_method = int (sample_type::*)(int) noexcept;
  using binary_const_method = int (sample_type::*)(int, int) const noexcept;
  std::array<char, 64> name{};
  const std::string_view type_name = make_type_name(name, "caors.benchmark.wrapper.method.", Index);
  return registration::type_<sample_type>(context, type_name)
      .meta()
      .method("add", static_cast<unary_method>(&sample_type::add))
      .method("add", static_cast<binary_const_method>(&sample_type::add))
      .method("reset", &sample_type::reset)
      .commit();
}

template <std::size_t... Index>
[[nodiscard]] error register_property_samples(meta::context& context,
                                              std::index_sequence<Index...>) {
  error status{};
  ((status = status.failed() ? status : register_property_sample<Index>(context)), ...);
  return status;
}

template <std::size_t... Index>
[[nodiscard]] error register_method_samples(meta::context& context, std::index_sequence<Index...>) {
  error status{};
  ((status = status.failed() ? status : register_method_sample<Index>(context)), ...);
  return status;
}

template <typename Register>
[[nodiscard]] memory_statistics measure_registration_allocations(Register&& register_samples) {
  tracking_resource tracking{&get_system_memory_resource()};
  {
    default_resource_scope resource_scope{&tracking};
    meta::registry registry{};
    meta::context context{registry};
    const error status = std::forward<Register>(register_samples)(context);
    if (status.failed()) {
      std::cerr << "统计 meta wrapper 注册分配失败，错误码: " << status.value() << '\n';
      std::exit(EXIT_FAILURE);
    }
  }

  const memory_statistics result = tracking.statistics();
  if (result.bytes_in_use != 0) {
    std::cerr << "统计 meta wrapper 注册分配后仍有存活内存: " << result.bytes_in_use << '\n';
    std::exit(EXIT_FAILURE);
  }
  return result;
}

void print_layout(std::string_view name, std::size_t size, std::size_t alignment) {
  std::cout << name << ": size=" << size << ", alignment=" << alignment << '\n';
}

void print_registration_statistics(std::string_view name, const memory_statistics& statistics,
                                   const memory_statistics& baseline, std::size_t record_count,
                                   std::size_t node_count) {
  const std::size_t allocation_count = statistics.allocation_count >= baseline.allocation_count
                                           ? statistics.allocation_count - baseline.allocation_count
                                           : 0;
  const std::size_t bytes_allocated = statistics.bytes_allocated >= baseline.bytes_allocated
                                          ? statistics.bytes_allocated - baseline.bytes_allocated
                                          : 0;
  const double allocations_per_record =
      static_cast<double>(allocation_count) / static_cast<double>(record_count);
  const double bytes_per_record =
      static_cast<double>(bytes_allocated) / static_cast<double>(record_count);
  const double allocations_per_node =
      static_cast<double>(allocation_count) / static_cast<double>(node_count);
  const double bytes_per_node =
      static_cast<double>(bytes_allocated) / static_cast<double>(node_count);

  std::cout << name << ": records=" << record_count << ", nodes=" << node_count
            << ", net_allocations=" << allocation_count << " (" << allocations_per_record
            << "/record, " << allocations_per_node << "/node), net_bytes=" << bytes_allocated
            << " (" << bytes_per_record << "/record, " << bytes_per_node
            << "/node), gross_peak=" << statistics.peak_bytes_in_use
            << ", live=" << statistics.bytes_in_use << '\n';
}

} // namespace

void print_meta_wrapper_layout_samples() {
  std::cout << "\nMeta wrapper layout samples\n";
  print_layout("meta.detail.property_wrapper", sizeof(meta::detail::property_wrapper),
               alignof(meta::detail::property_wrapper));
  print_layout("meta.detail.property_record", sizeof(meta::detail::property_record),
               alignof(meta::detail::property_record));
  print_layout("meta.detail.method_wrapper", sizeof(meta::detail::method_wrapper),
               alignof(meta::detail::method_wrapper));
  print_layout("meta.detail.method_wrapper_state", sizeof(meta::detail::method_wrapper_state),
               alignof(meta::detail::method_wrapper_state));
  print_layout("meta.detail.method_record", sizeof(meta::detail::method_record),
               alignof(meta::detail::method_record));
  print_layout("meta.property", sizeof(meta::property), alignof(meta::property));
  print_layout("meta.method", sizeof(meta::method), alignof(meta::method));
  print_layout("meta.property_accessor", sizeof(meta::property_accessor),
               alignof(meta::property_accessor));
}

void print_meta_wrapper_registration_samples() {
  std::cout << "\nMeta wrapper registration allocation samples\n";
  if constexpr (!config::internal_allocations_use_default_resource) {
    std::cout
        << "disabled: configure with "
           "CAORS_INTERNAL_ALLOCATIONS_USE_DEFAULT_RESOURCE=ON to collect registration samples\n";
    return;
  }

  (void)string_name{"member"};
  (void)string_name{"accessor"};
  (void)string_name{"readonly"};
  (void)string_name{"add"};
  (void)string_name{"reset"};
  warm_property_registration_types(std::make_index_sequence<registration_sample_count>{});
  warm_method_registration_types(std::make_index_sequence<registration_sample_count>{});

  const memory_statistics empty =
      measure_registration_allocations([](meta::context&) { return error{}; });
  const memory_statistics properties = measure_registration_allocations([](meta::context& context) {
    return register_property_samples(context,
                                     std::make_index_sequence<registration_sample_count>{});
  });
  const memory_statistics methods = measure_registration_allocations([](meta::context& context) {
    return register_method_samples(context, std::make_index_sequence<registration_sample_count>{});
  });

  std::cout << "meta.registration.empty_registry"
            << ": allocations=" << empty.allocation_count << ", bytes=" << empty.bytes_allocated
            << ", peak=" << empty.peak_bytes_in_use << ", live=" << empty.bytes_in_use << '\n';
  print_registration_statistics("meta.registration.property_mixed", properties, empty,
                                registration_sample_count, registration_sample_count * 3);
  print_registration_statistics("meta.registration.method_mixed", methods, empty,
                                registration_sample_count, registration_sample_count * 3);
}

} // namespace caors::benchmarks
