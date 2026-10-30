// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io.h"
// 普通文件访问头不应把旧同步文件系统和路由器带入调用方接口。
#if defined(CAORS_IO_FILE_SYSTEM_H_) || defined(CAORS_IO_FILE_SYSTEM_RESOLVER_H_) ||               \
    defined(CAORS_IO_ASYNC_FILE_H_) || defined(CAORS_IO_ROUTE_SNAPSHOT_H_)
#error "VFS context must not expose legacy file_system headers"
#endif
static_assert(!std::is_base_of_v<caors::io::backend, caors::io::file_service>);
static_assert(!std::is_constructible_v<caors::io::file_batch, const caors::io::file_service&>);
#if __has_include("caors/io/context.h") || __has_include("caors/io/vfs.h")
#error "Replaced public file access headers must not remain installed"
#endif

#include "caors/resources/byte_reader.h"
#include "caors/resources/source.h"
#include "caors/resources/vfs_source.h"
#if defined(CAORS_IO_FILE_SYSTEM_H_) || defined(CAORS_IO_ROUTE_SNAPSHOT_H_)
#error "Resource source and byte reading must not expose legacy route definitions"
#endif
static_assert(
    std::is_same_v<decltype(caors::resources::read_context::source), caors::io::backend_location>);

static_assert(!std::is_convertible_v<const caors::io::file_service&, const caors::io::file_batch&>);

static_assert(!std::is_assignable_v<caors::io::file_batch&, const caors::io::file_service&>);
namespace {
// 公共值类型须支持空值复位，避免删除重载干扰 GCC 的花括号赋值解析。
[[maybe_unused]] void reset_batch(caors::io::file_batch& batch) {
  batch = {};
  batch = caors::io::file_batch{};
}
}
