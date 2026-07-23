# SPDX-License-Identifier: MIT
# Copyright (c) 2026 CAORS contributors

[CmdletBinding()]
param(
    [ValidateSet("configure", "build", "test", "all")]
    [string]$Action = "all",

    [string]$Preset = "windows-msvc-debug",

    [ValidateSet("x64", "x86", "arm64")]
    [string]$Architecture = "x64"
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory)]
        [string]$Command,

        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "命令执行失败（退出码 $LASTEXITCODE）：$Command $($Arguments -join ' ')"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "未找到 vswhere.exe，请安装 Visual Studio Installer 和 C++ 桌面开发工作负载。"
}

$installationPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installationPath) {
    throw "未找到包含 MSVC C++ 工具链的 Visual Studio。"
}

$vsDevCmd = Join-Path $installationPath.Trim() "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "未找到 Visual Studio 开发环境脚本：$vsDevCmd"
}

# 批处理脚本无法直接修改父 PowerShell，因此读取其环境并导入当前进程。
$environmentLines = & $env:ComSpec /d /s /c "`"$vsDevCmd`" -no_logo -arch=$Architecture -host_arch=x64 && set"
if ($LASTEXITCODE -ne 0) {
    throw "初始化 Visual Studio 开发环境失败。"
}

foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
    }
}

switch ($Action) {
    "configure" { Invoke-CheckedCommand cmake @("--preset", $Preset) }
    "build" { Invoke-CheckedCommand cmake @("--build", "--preset", $Preset) }
    "test" { Invoke-CheckedCommand ctest @("--preset", $Preset) }
    "all" {
        Invoke-CheckedCommand cmake @("--preset", $Preset)
        Invoke-CheckedCommand cmake @("--build", "--preset", $Preset)
        Invoke-CheckedCommand ctest @("--preset", $Preset)
    }
}
