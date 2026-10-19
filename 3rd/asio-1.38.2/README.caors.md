<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# standalone Asio

本目录固定 standalone Asio `asio-1.38.2` 的公共头文件，来源为：

- 仓库：<https://github.com/chriskohlhoff/asio>
- 标签：`asio-1-38-2`
- 提交：`8806a6803cde7054c3049d3666d3ec36786568c5`
- 许可证：Boost Software License 1.0，见 `LICENSE_1_0.txt`

仅保留构建 CAORS 所需的 `include/asio`、`include/asio.hpp` 与许可证文件。第三方源码保持上游内容，
CAORS 的编译宏统一由 `caors_asio_config` CMake 目标声明。
