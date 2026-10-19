# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

"""检查 Linux 动态库的私有 Asio 导出，并保存实际依赖与符号证据。"""

import pathlib
import subprocess
import sys

library = pathlib.Path(sys.argv[1]).resolve(strict=True)
output = pathlib.Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)

def run(*args):
    return subprocess.check_output(args, text=True)

symbols = run("nm", "-D", "--defined-only", "-C", str(library))
dynamic = run("readelf", "-d", str(library))
dependencies = run("ldd", str(library))
(output / "exports.txt").write_text(symbols, encoding="utf-8")
(output / "dynamic.txt").write_text(dynamic, encoding="utf-8")
(output / "dependencies.txt").write_text(dependencies, encoding="utf-8")
if "asio::" in symbols:
    raise SystemExit("错误：动态库导出了私有 Asio 符号")
if "caors::get_version()" not in symbols or "(SONAME)" not in dynamic:
    raise SystemExit("错误：缺少公共版本函数或 SONAME")
if "not found" in dependencies:
    raise SystemExit("错误：动态库依赖未找到")
print(f"符号检查通过，已记录 {len(symbols.splitlines())} 个导出及运行依赖")
