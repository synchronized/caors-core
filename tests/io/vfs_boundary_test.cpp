// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#include "caors/io.h"
// 普通文件访问头不应把旧同步文件系统和路由器带入调用方接口。
#if defined(CAORS_IO_FILE_SYSTEM_H_) || defined(CAORS_IO_FILE_SYSTEM_RESOLVER_H_) ||               \
    defined(CAORS_IO_ASYNC_FILE_H_) || defined(CAORS_IO_ROUTE_SNAPSHOT_H_)
#error "VFS context must not expose legacy file_system headers"
#endif
static_assert(!std::is_base_of_v<caors::io::backend, caors::io::vfs>);

static_assert(!std::is_constructible_v<caors::io::io_context, caors::io::vfs&&,
                                       caors::io::io_context_options>);
