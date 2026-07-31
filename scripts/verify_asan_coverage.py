# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

"""验证 CAORS、测试程序和 C++ 测试插件统一启用了 AddressSanitizer。"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ASAN_FLAG = "-fsanitize=address"


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path, help="CMake Ninja 构建目录")
    return parser.parse_args()


def is_project_cpp_source(path: Path) -> bool:
    normalized = path.as_posix()
    return path.suffix in {".cc", ".cpp", ".cxx"} and (
        "/src/" in normalized or "/tests/" in normalized
    )


def verify_compile_commands(build_dir: Path) -> list[str]:
    compile_commands_path = build_dir / "compile_commands.json"
    entries = json.loads(compile_commands_path.read_text(encoding="utf-8"))
    missing: list[str] = []

    for entry in entries:
        source = Path(entry["file"]).resolve()
        if is_project_cpp_source(source) and ASAN_FLAG not in entry["command"]:
            missing.append(f"编译命令缺少 ASan：{source}")

    return missing


def verify_link_commands(build_dir: Path) -> list[str]:
    ninja_path = build_dir / "build.ninja"
    content = ninja_path.read_text(encoding="utf-8")
    missing: list[str] = []

    for block in re.split(r"(?m)(?=^build )", content):
        first_line = block.partition("\n")[0]
        if not re.search(r": CXX_(?:EXECUTABLE|SHARED_LIBRARY)_LINKER", first_line):
            continue

        output = first_line.removeprefix("build ").partition(":")[0]
        if not (output.startswith("src/") or output.startswith("tests/")):
            continue

        if ASAN_FLAG not in block:
            missing.append(f"链接命令缺少 ASan：{output}")

    return missing


def main() -> int:
    build_dir = parse_arguments().build_dir.resolve()
    problems = verify_compile_commands(build_dir)
    problems.extend(verify_link_commands(build_dir))

    if problems:
        print("\n".join(problems))
        return 1

    print("ASan 覆盖校验通过：CAORS、测试程序和 C++ 测试插件均已启用。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
