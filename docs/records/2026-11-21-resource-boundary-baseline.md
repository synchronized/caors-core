<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 宿主资源核心编译边界基线

## 范围

对应[宿主驱动资源加载计划](../plans/host-driven-resource-loading.md)的 P0，分支
feature/resource-source-boundary，基于 69653b6f。新增 tests/resource_boundary 独立 CMake 工程，
也通过现有测试树自动接入常规构建，不需要 Catch2 或新增第三方依赖。

## 方法与实际结果

在构建目录生成与所有现有 IO 公共头同路径的禁止头，优先于库头搜索；
命中即输出 CAORS_RESOURCE_BOUNDARY_FORBIDDEN_IO 编译错误。
正向 OBJECT 目标实例化继承资源类型的宿主异步 loader、缓存及 request_load_into 完整调用路径，
只编译 core.cpp，不链接单体 caors，也不编译 IO 或 assets 源码。
独立 configure 时分别探测 database 和 asset_manager，要求它们编译失败且包含上述指定诊断，
避免把缺失工具链或任意语法错误误当成有效依赖证据。

Windows、Clang 23.1.1、Visual Studio 开发环境、Ninja 并发 2：

- 独立 configure 与正向对象编译通过。
- database 经 manifest 引入 IO，反向探针按预期命中禁止头。
- asset_manager 的项目与来源装配仍依赖 IO，反向探针按预期命中禁止头。

复现：在初始化开发环境后执行
`cmake -S tests/resource_boundary -B build/resource-boundary -G Ninja`，
再执行 `cmake --build build/resource-boundary`。也可在本机使用 Visual Studio generator。
负向详细诊断保存在该构建目录的 database-baseline.log 和 assets-baseline.log。

## 限制及后续出口

这里只证明公开头与模板实例化边界，没有可执行程序，不证明独立链接、网络执行、
动态依赖、无 GUID 对象或管理器清理已实现。当前 cache、handle 和 loader 仍强制资源基类及 GUID。
现有 src/CMakeLists.txt 仍把全部源码加入单体 caors，不能称为独立资源模块。
核心脱离 manifest 位置及项目状态后，须把对应负向探针改为正向探针；不得为维持旧基线阻止解耦。
P1 验证宿主 Key 与对象身份拆分；P2 才以独立链接及运行作为出口。
本批未运行完整平台矩阵，不改运行时协议，未删除 IO 或合并 main。
