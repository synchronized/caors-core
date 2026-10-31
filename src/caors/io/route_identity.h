// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_INTERNAL_IO_ROUTE_IDENTITY_H_
#define CAORS_INTERNAL_IO_ROUTE_IDENTITY_H_
#include "caors/export.h"
#include <cstdint>
namespace caors::io::detail {
// 新旧路由共享标识序列，退出兼容层时不改变来源身份语义。
[[nodiscard]] CAORS_API std::uint64_t next_route_id() noexcept;
}
#endif
