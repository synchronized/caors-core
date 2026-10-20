// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/platform/shared_library.h"

#include "caors/config.h"

#if CAORS_HAS_DYNAMIC_LIBRARIES
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace caors::platform {
namespace {

[[nodiscard]] error open_error(const std::filesystem::path& path) noexcept {
  if (path.empty()) {
    return {error_domain::platform, errc::invalid_argument};
  }

  std::error_code status;
  if (!std::filesystem::exists(path, status)) {
    return {error_domain::platform, status ? errc::backend_failure : errc::not_found};
  }

  return {error_domain::platform, errc::backend_failure};
}

} // namespace

shared_library::shared_library(shared_library&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), path_(std::move(other.path_)) {
  other.path_.clear();
}

shared_library& shared_library::operator=(shared_library&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  static_cast<void>(close());
  handle_ = std::exchange(other.handle_, nullptr);
  path_ = std::move(other.path_);
  other.path_.clear();
  return *this;
}

shared_library::~shared_library() { static_cast<void>(close()); }

result<shared_library> shared_library::open(const std::filesystem::path& path,
                                            library_load_mode mode) {
  if (path.empty()) {
    return fail<shared_library>({error_domain::platform, errc::invalid_argument});
  }

#if defined(_WIN32)
  static_cast<void>(mode);
  HMODULE native = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  void* handle = reinterpret_cast<void*>(native);
#else
  const int flags = RTLD_NOW | (mode == library_load_mode::global ? RTLD_GLOBAL : RTLD_LOCAL);
  void* handle = dlopen(path.c_str(), flags);
#endif

  if (handle == nullptr) {
    return fail<shared_library>(open_error(path));
  }

  return ok(shared_library{handle, path});
}

void* shared_library::find_symbol(std::string_view name) const {
  if (!loaded() || name.empty() || name.find('\0') != std::string_view::npos) {
    return nullptr;
  }

  const std::string terminated{name};
#if defined(_WIN32)
  const FARPROC symbol = GetProcAddress(reinterpret_cast<HMODULE>(handle_), terminated.c_str());
  return reinterpret_cast<void*>(symbol);
#else
  return dlsym(handle_, terminated.c_str());
#endif
}

error shared_library::close() noexcept {
  if (!loaded()) {
    path_.clear();
    return {};
  }

#if defined(_WIN32)
  const bool succeeded = FreeLibrary(reinterpret_cast<HMODULE>(handle_)) != 0;
#else
  const bool succeeded = dlclose(handle_) == 0;
#endif
  if (!succeeded) {
    return {error_domain::platform, errc::backend_failure};
  }

  handle_ = nullptr;
  path_.clear();
  return {};
}

} // namespace caors::platform

#else
namespace caors::platform {
shared_library::shared_library(shared_library&& other) noexcept = default;
shared_library& shared_library::operator=(shared_library&& other) noexcept = default;
shared_library::~shared_library() = default;
result<shared_library> shared_library::open(const std::filesystem::path& path, library_load_mode) {
  return fail<shared_library>(
      {error_domain::platform, path.empty() ? errc::invalid_argument : errc::unsupported});
}
void* shared_library::find_symbol(std::string_view) const { return nullptr; }
error shared_library::close() noexcept { return {}; }
} // namespace caors::platform
#endif
