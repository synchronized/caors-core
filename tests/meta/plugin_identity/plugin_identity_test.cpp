// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "plugin_identity_test_support.h"

#include "caors/error.h"
#include "caors/meta/type_id.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

class shared_library final {
public:
  shared_library() = default;
  explicit shared_library(const char* path) { open(path); }
  ~shared_library() { close(); }

  shared_library(const shared_library&) = delete;
  shared_library& operator=(const shared_library&) = delete;

  shared_library(shared_library&& other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}

  shared_library& operator=(shared_library&& other) noexcept {
    if (this != &other) {
      close();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }

  template <typename Function> [[nodiscard]] Function find(const char* name) const noexcept {
#if defined(_WIN32)
    return reinterpret_cast<Function>(GetProcAddress(handle_, name));
#else
    return reinterpret_cast<Function>(dlsym(handle_, name));
#endif
  }

private:
  void open(const char* path) {
#if defined(_WIN32)
    handle_ = LoadLibraryA(path);
#else
    handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
  }

  void close() noexcept {
    if (handle_ == nullptr) {
      return;
    }
#if defined(_WIN32)
    FreeLibrary(handle_);
#else
    dlclose(handle_);
#endif
    handle_ = nullptr;
  }

#if defined(_WIN32)
  HMODULE handle_ = nullptr;
#else
  void* handle_ = nullptr;
#endif
};

[[nodiscard]] int fail(const char* message) {
  std::cerr << message << '\n';
  return 1;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    return fail("插件 identity 测试需要 plugin A 和 plugin B 路径");
  }

  shared_library plugin_a{argv[1]};
  shared_library plugin_b{argv[2]};
  if (!plugin_a.valid() || !plugin_b.valid()) {
    return fail("无法加载 identity 测试插件");
  }

  const auto plugin_a_id =
      plugin_a.find<caors::tests::plugin_identity::type_id_fn>("caors_test_plugin_a_type_id");
  const auto plugin_b_id =
      plugin_b.find<caors::tests::plugin_identity::type_id_fn>("caors_test_plugin_b_type_id");
  if (plugin_a_id == nullptr || plugin_b_id == nullptr) {
    return fail("无法查找 identity 测试插件导出函数");
  }

  std::uint64_t first_id = 0;
  std::uint64_t second_id = 0;
  std::thread first{[&] { first_id = plugin_a_id(); }};
  std::thread second{[&] { second_id = plugin_b_id(); }};
  first.join();
  second.join();

  const std::uint64_t host_id =
      caors::meta::detail::type_id_of_cpp_type<caors::tests::plugin_identity::shared_plugin_type>()
          .value();
  if (first_id == 0 || first_id != second_id || first_id != host_id) {
    return fail("host 与插件没有获得统一的 type_id");
  }

  plugin_a = shared_library{};
  shared_library reloaded_plugin_a{argv[1]};
  const auto reloaded_plugin_a_id =
      reloaded_plugin_a.find<caors::tests::plugin_identity::type_id_fn>(
          "caors_test_plugin_a_type_id");
  if (reloaded_plugin_a_id == nullptr || reloaded_plugin_a_id() != host_id) {
    return fail("插件重新加载后没有复用原 type_id");
  }

  const auto plugin_a_register =
      reloaded_plugin_a.find<caors::tests::plugin_identity::register_type_fn>(
          "caors_test_plugin_a_register_type");
  const auto plugin_b_register = plugin_b.find<caors::tests::plugin_identity::register_type_fn>(
      "caors_test_plugin_b_register_type");
  if (plugin_a_register == nullptr || plugin_b_register == nullptr) {
    return fail("无法查找插件类型注册函数");
  }

  if (plugin_a_register() != static_cast<int>(caors::errc::success)) {
    return fail("plugin A 无法注册基础静态类型模型");
  }
  if (plugin_b_register() != static_cast<int>(caors::errc::inconsistent_type_model)) {
    return fail("不同插件中的冲突静态类型模型没有被诊断");
  }

  return 0;
}
