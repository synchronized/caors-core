<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 资源、资产与 IO 移除记录

## 范围

依据用户 2026-11-27 明确删除要求，停止宿主资源核心及动态依赖开发。
分支 feature/remove-asset-resource-io；不调整版本、不创建标签、不合并 main。
迁移契约由[退出指南](../guides/application-modules-removal.md)维护。

删除三个公共头目录、实现目录及聚合头，清理资源引用 codec、来源装配、资源及文件专用测试、
示例和基准。序列化保留内存转换，移除文件/流 API；插件保留动态加载与通用对象能力，
删除资源/文件测试扩展及清单文件读取便利入口。

## 验证

Windows Clang 23.1.6、Debug 动态库、Ninja 并发 2：剩余库、公共头、全部测试、示例及基准构建完成。
最终完整 CTest 82/82 通过（build/removal-final-ctest.log）。
原安装与重定位后的独立消费各 5 项通过（build/removal-package-final.log）；新安装确认不存在
assets/resources/io 目录及三个聚合头，消费端同时编译旧头不存在的断言。
本机 Emscripten Release 静态库独立构建通过（build/removal-web.log）。
活动源码及构建引用扫描仅保留旧头不存在的负向检查；665 个本地文档链接存在性检查通过。
格式及 git diff --check 通过。日志保存在本机忽略目录，不作为公开制品。

## 失败与修正

第一次全量编译发现插件协议测试残留资源弱句柄扩展，已改为通用插件扩展和模块租约的排队回调测试。
不是移除插件协议测试，也没有跳过该目标。修复为 a1fcedaa；功能删除提交为 d25fe36c。

首次完整 CTest 有两项失败：插件边界混用 Clang 23.1.1/23.1.6 旧产物，被现有构建兼容检查拒绝；
清理并重编译边界对象后通过，未放宽版本检查。依赖缓存测试通过 execute_process 读取中文错误时
使用了错误编码，已显式使用 UTF-8；离线缺失与并发锁超时检查仍保留并通过。
两项专项复验通过后，再次完整运行 82 项，全部通过。

## 平台 CI 与范围

- [Web 45919841654](https://github.com/synchronized/caors-core/actions/runs/45919841654)：d25fe36c 全部通过，包含公共头、Node、浏览器及安装/重定位消费。
- [Linux 45920160524](https://github.com/synchronized/caors-core/actions/runs/45920160524)：a1fcedaa，执行中。
- [Windows 45920164711](https://github.com/synchronized/caors-core/actions/runs/45920164711)：a1fcedaa，执行中。

原生首次 d25fe36c 的工作流在发现插件测试残留后取消，不能记为通过。
后续增量为原生测试诊断、测试进程输出编码和文档清理，没有修改 Web 或剩余库实现；
上述平台结果分别关联其实际提交，不能将旧平台绿色结果当作新增测试全部已经跨平台验收。

[PR #59](https://github.com/synchronized/caors-core/pull/59) 保持草稿，取代已关闭的 #58；
代码删除及本机验证已完成，原生矩阵仍待结束，未合并或发布。
