<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 第三方依赖与源码缓存

本页维护 CAORS 开发依赖的获取、复用、离线、并发和缓存恢复规则。
版本、上游提交、下载地址、SHA-256、入口和许可证标识唯一维护在
[依赖清单](../../cmake/caors_dependency_catalog.cmake)，目标组装在 [vendor](../../vendor/CMakeLists.txt)。

## 范围与目标复用

Catch2 只用于测试，ctrack 只用于基准；不是核心库运行时或安装包的传递依赖。
父项目已提供对应目标时优先复用；Catch2 必须同时有 Catch2::Catch2 与 Catch2::Catch2WithMain，
并提供匹配的 Catch2_VERSION 或可验证包元数据；ctrack 必须提供 ctrack::ctrack 与匹配的 ctrack_VERSION。
不完整、未知或不匹配的版本会失败，不以下载另一份同名目标掩盖冲突。
关闭测试（含 BUILD_TESTING）或基准后不获取对应依赖；两者都关闭时不创建源码缓存目录。

缓存 Catch2 使用上游聚合源，并在当前 build/vendor/catch2-compat 生成 catch2/catch_all.hpp
适配头；不复制或改写共享源码。ctrack 继续使用单头目标与 CTRACK_DISABLE_EXECUTION_POLICY，
不运行其上游 CMake 项目，不引入可选 TBB、示例或测试。第三方目标不套用 CAORS 的警告即错误策略。

## 配置与布局

| 选项 | 默认值 | 含义 |
| --- | --- | --- |
| CAORS_DEPENDENCY_SOURCE_CACHE_DIR | 本仓库 vendor/cache | 可指定另一源码缓存根目录 |
| CAORS_DEPENDENCY_OFFLINE | OFF | ON 时不下载，缺少可用源码或归档即失败 |
| CAORS_DEPENDENCY_LOCK_TIMEOUT | 300 | 等待当前依赖锁的秒数，0 表示不等待 |

```text
vendor/
  CMakeLists.txt
  README.md
  cache/                       # 忽略，不进入 Git 或安装包
    archives/<name>-<sha256>.tar.gz
    sources/<name>/<sha256>/
    locks/<name>-<sha256>.lock
    staging/<name>-<short-sha>-<nonce>/
```

只共享源码与原始归档。不同 preset 的生成头、静态库和对象文件仍在各自 build 目录，不能复用编译目录。
归档 SHA-256 是缓存身份的一部分；首次解压前验证摘要，完成发布后视为只读。
源码命中检查完成记录与入口，不逐文件重新校验整棵源码树，不支持手动修改缓存内容。

## 首次准备和离线构建

开发依赖首次配置需要访问锁定上游。可以在有网络的 Linux 环境先配置以填充两个依赖：

```sh
cmake --preset linux-clang-release -DCAORS_BUILD_BENCHMARKS=ON
cmake --preset linux-clang-release -DCAORS_DEPENDENCY_OFFLINE=ON
```

其他 preset 指向相同缓存后即可离线配置；Windows 工具链初始化仍遵循[构建指南](build.md)。
离线机器可以预先复制同协议缓存的 archives 和 sources，不复制正在写入的目录、locks 或 staging。
只有归档也可离线解压；只有完整源码也可命中。路径和归档摘要必须匹配清单，不能改名绕过校验。

仅构建核心库和示例、完全不获取测试与基准依赖：

```sh
cmake --preset linux-clang-release -DCAORS_BUILD_TESTING=OFF -DCAORS_BUILD_BENCHMARKS=OFF -DCAORS_DEPENDENCY_OFFLINE=ON
```

源码发布归档不包含被忽略的 vendor/cache；单独源码包不承诺首次离线运行测试。
发布前需验证归档的联网冷配置和预填缓存离线配置；实际交付范围由对应版本及 Release 维护。

## 并发、损坏与恢复

缓存函数按依赖名和归档摘要加锁，持锁覆盖检查、下载、解压和完成发布；不同依赖使用不同锁。
下载进入唯一 staging，校验成功才发布归档；源码入口和完成记录准备好后再重命名发布。
使用操作系统文件锁，函数返回或进程退出释放；锁文件存在不表示仍被占用，不按时间戳强行删锁。
只支持同一主机上遵循此协议的进程，不承诺分布式锁，也不能保护同时操作同一 build 目录的两个 Ninja。

离线缺失、坏摘要、不完整源码、超时或下载失败均明确报错。不会自动删除已有归档或源码；
失败 staging 保留路径用于排查；Windows 路径较深时可配置较短的本地缓存根目录。恢复时使用新缓存根目录，或确认没有活跃写入者后将损坏项移走重试。
跨仓库复用必须统一缓存键、完成标记及锁协议；配置相同物理目录不代表兼容 Granit/Gneiss 的既有格式。
CI 仅缓存 archives/sources，按平台、依赖集合及清单/缓存实现摘要区分；不缓存编译产物、锁或失败暂存。

## 来源与许可证

Catch2 为 BSL-1.0，ctrack 为 MIT；上游许可文本位于对应源码根目录的 LICENSE.txt / LICENSE。
源码来源与固定提交见清单；迁移未升级版本，现有聚合代码与上游内容经比对一致，仅保留构建目录适配。
实际比对、缓存测试、构建及安装证据见[迁移记录](../records/2026-10-20-dependency-source-cache.md)。
