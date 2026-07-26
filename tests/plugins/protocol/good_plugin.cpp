// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/adapters/stl.h"
#include "caors/plugins/abi.h"
#include "caors/plugins/meta_registration.h"
#include "caors/registration.h"
#include "caors/serialization/codec.h"
#include "plugin_protocol_test_extension.h"

#include <atomic>
#include <cstring>
#include <optional>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace {

struct plugin_reflected_type {
  int value = 42;
  std::vector<int> values{1, 2, 3};
  std::unordered_set<int> tags{4, 5};
  std::unordered_map<int, int> lookup{{6, 60}};
  std::pair<int, int> pair_value{7, 70};
  std::tuple<int, int> tuple_value{8, 80};
  std::variant<int, float> variant_value{9};
  std::optional<int> optional_value{10};

  plugin_reflected_type() = default;
  explicit plugin_reflected_type(int initial_value) : value(initial_value) {}
  ~plugin_reflected_type();
  void block() const;
};

struct plugin_instance {
  int lifecycle_state = 0;
  caors_plugin_host host{};
  std::atomic<bool> method_entered{false};
  std::atomic<bool> method_release{false};
  std::atomic<int> destructions{0};
};

plugin_instance instance{};
constexpr int extension_value = 42;

plugin_reflected_type::~plugin_reflected_type() {
  instance.destructions.fetch_add(1, std::memory_order_relaxed);
}

void plugin_reflected_type::block() const {
  instance.method_entered.store(true, std::memory_order_release);
  while (!instance.method_release.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}

void CAORS_PLUGIN_CALL reset_control(void* context) {
  auto& self = *static_cast<plugin_instance*>(context);
  self.method_entered.store(false, std::memory_order_release);
  self.method_release.store(false, std::memory_order_release);
  self.destructions.store(0, std::memory_order_release);
}

int32_t CAORS_PLUGIN_CALL method_entered(void* context) {
  return static_cast<plugin_instance*>(context)->method_entered.load(std::memory_order_acquire) ? 1
                                                                                                : 0;
}

void CAORS_PLUGIN_CALL release_method(void* context) {
  static_cast<plugin_instance*>(context)->method_release.store(true, std::memory_order_release);
}

int32_t CAORS_PLUGIN_CALL destruction_count(void* context) {
  return static_cast<plugin_instance*>(context)->destructions.load(std::memory_order_acquire);
}

const caors_plugin_protocol_test_extension test_extension{
    sizeof(caors_plugin_protocol_test_extension),
    CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_VERSION,
    &instance,
    &reset_control,
    &method_entered,
    &release_method,
    &destruction_count,
};
const caors_plugin_protocol_capability test_capability{73};

caors::serialization::variant_result make_codec_value(const caors::serialization::context&,
                                                      const caors::meta::any& source) {
  const auto* value = source.try_get<plugin_reflected_type>();
  return value != nullptr ? caors::ok(caors::variant::from(static_cast<std::int64_t>(value->value)))
                          : caors::fail<caors::variant>(
                                {caors::error_domain::serialization, caors::errc::invalid_type});
}

caors::serialization::variant_result make_codec_value_ref(const caors::serialization::context&,
                                                          const caors::meta::any_ref& source) {
  const auto* value = source.try_get<plugin_reflected_type>();
  return value != nullptr ? caors::ok(caors::variant::from(static_cast<std::int64_t>(value->value)))
                          : caors::fail<caors::variant>(
                                {caors::error_domain::serialization, caors::errc::invalid_type});
}

const caors::serialization::codec_ops test_codec{
    nullptr, nullptr, nullptr, &make_codec_value, &make_codec_value_ref,
};

int32_t CAORS_PLUGIN_CALL initialize(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 0) {
    return 1;
  }
  if (caors::meta::registry* registry =
          caors::plugins::find_registration_registry(&self.host, data);
      registry != nullptr) {
    caors::meta::context context{*registry};
    auto reflected =
        caors::registration::type_<plugin_reflected_type>(context, "caors.test.plugin.reflected");
    if (reflected.meta()
            .property("value", &plugin_reflected_type::value)
            .property("values", &plugin_reflected_type::values)
            .property("tags", &plugin_reflected_type::tags)
            .property("lookup", &plugin_reflected_type::lookup)
            .property("pair", &plugin_reflected_type::pair_value)
            .property("tuple", &plugin_reflected_type::tuple_value)
            .property("variant", &plugin_reflected_type::variant_value)
            .property("optional", &plugin_reflected_type::optional_value)
            .method("block", &plugin_reflected_type::block)
            .capability(&test_capability)
            .capability(test_codec)
            .factory()
            .constructor<int>()
            .commit() ||
        caors::meta::detail::emplace_extension<caors_plugin_protocol_meta_extension>(
            *registry, reflected.get().id(), caors_plugin_protocol_meta_extension{91})) {
      return 1;
    }
  }
  self.lifecycle_state = 1;
  return 0;
}

int32_t CAORS_PLUGIN_CALL start(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 1) {
    return 1;
  }
  self.lifecycle_state = 2;
  return 0;
}

int32_t CAORS_PLUGIN_CALL stop(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 2) {
    return 1;
  }
  self.lifecycle_state = 3;
  return 0;
}

int32_t CAORS_PLUGIN_CALL shutdown(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  if (self.lifecycle_state != 1 && self.lifecycle_state != 3) {
    return 1;
  }
  self.lifecycle_state = 0;
  return 0;
}

const void* CAORS_PLUGIN_CALL get_extension(void*, const char* id, size_t id_size,
                                            uint32_t version) {
  constexpr char extension_id[] = "caors.test.answer";
  if (version == 1 && id_size == sizeof(extension_id) - 1 &&
      std::memcmp(id, extension_id, id_size) == 0) {
    return &extension_value;
  }
  return version == CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_VERSION &&
                 id_size == sizeof(CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_ID) - 1 &&
                 std::memcmp(id, CAORS_PLUGIN_PROTOCOL_TEST_EXTENSION_ID, id_size) == 0
             ? &test_extension
             : nullptr;
}

constexpr char plugin_name[] = "caors.test.good";
constexpr char plugin_display_name[] = "CAORS 测试插件";

const caors_plugin_descriptor descriptor{
    sizeof(caors_plugin_descriptor),
    {{0x2a, 0x6d, 0x59, 0x92, 0xff, 0x3c, 0x45, 0xae, 0xa1, 0x67, 0xd8, 0x61, 0xe4, 0x69, 0xaa,
      0x90}},
    {plugin_name, sizeof(plugin_name) - 1},
    {plugin_display_name, sizeof(plugin_display_name) - 1},
    {1, 2, 3},
};

const caors_plugin_entry entry{
    sizeof(caors_plugin_entry),
    CAORS_PLUGIN_ABI_VERSION,
    &descriptor,
    &instance,
    &initialize,
    &start,
    &stop,
    &shutdown,
    &get_extension,
};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host* host, uint32_t host_abi_version) {
  if (host == nullptr || host->struct_size < sizeof(caors_plugin_host) ||
      host_abi_version != CAORS_PLUGIN_ABI_VERSION || host->get_extension == nullptr) {
    return nullptr;
  }
  instance.host = *host;
  const auto* runtime = static_cast<const caors_plugin_runtime_extension*>(host->get_extension(
      host->context, CAORS_PLUGIN_RUNTIME_EXTENSION_ID,
      sizeof(CAORS_PLUGIN_RUNTIME_EXTENSION_ID) - 1, CAORS_PLUGIN_RUNTIME_EXTENSION_VERSION));
  if (runtime == nullptr || runtime->struct_size < sizeof(caors_plugin_runtime_extension) ||
      host->get_extension(host->context, CAORS_PLUGIN_RUNTIME_EXTENSION_ID,
                          sizeof(CAORS_PLUGIN_RUNTIME_EXTENSION_ID) - 1, 2) != nullptr) {
    return nullptr;
  }
  if (host->log != nullptr) {
    constexpr char message[] = "插件已协商宿主扩展";
    host->log(host->context, 1, message, sizeof(message) - 1);
  }
  return &entry;
}
