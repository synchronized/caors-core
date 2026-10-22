// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/database.h"

// 单独翻译单元验证 资源 database 与 loader 协议不再通过传递包含依赖具体来源或文件系统。
#if defined(CAORS_RESOURCES_SOURCE_H_) || defined(CAORS_IO_FILE_SYSTEM_H_)
#error "resource database must not include VFS source or file_system"
#endif

static_assert(std::is_polymorphic_v<caors::resources::load_snapshot>);
