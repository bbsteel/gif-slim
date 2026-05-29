# GIF Slim

一个基于 **Qt Widgets + giflib** 的本地 GIF 编辑器，支持：
- 拖动范围选择
- 裁剪 / 删除范围
- 抽帧 / 倍速 / 缩放 / 颜色数调整
- GUI 导出与 headless CLI 导出

当前仓库已经补齐了三平台打包脚本，但**只有 Linux AppImage 已在当前环境实际验证打通**。Windows 和 macOS 需要各自的原生环境来出包。

| 平台 | 当前产物 | 当前状态 |
| --- | --- | --- |
| Linux | `dist/GifSlim-1.0.0-linux-x86_64.AppImage` | **已验证** |
| Windows | `Setup.exe` 或 portable zip | 需要原生 Windows / MSYS2 环境 |
| macOS | `.dmg` | 需要原生 macOS 环境 |

## 本地构建

仓库当前可信构建入口是 **qmake**。

```bash
./build.sh
```

运行桌面版：

```bash
./build_and_run.sh
```

CLI 冒烟：

```bash
QT_QPA_PLATFORM=offscreen ./gif-slim --headless --open test.gif --skip 2 --save /tmp/out.gif
```

## 打包入口

仓库已经准备好的打包脚本：

| 脚本 | 作用 | 运行环境 |
| --- | --- | --- |
| `./package-linux.sh` | 生成 Linux AppImage | Linux |
| `./package-windows.sh` | 生成 Windows 安装包或 portable zip | Windows / MSYS2 |
| `./package-macos.sh` | 生成 macOS dmg | macOS |

此外还有 GitHub Actions 工作流：

```text
.github/workflows/package.yml
```

它支持：
- 手工触发 `workflow_dispatch`
- 推送 `v*` tag 后自动打三平台包

## Linux 打包

### 依赖

当前脚本假设系统已经有：
- `qmake6`
- `rsvg-convert`
- `curl`

`package-linux.sh` 会自动下载：
- `linuxdeploy`
- `linuxdeploy-plugin-qt`
- `appimagetool`

### 执行

```bash
./package-linux.sh
```

产物默认输出到：

```text
dist/GifSlim-1.0.0-linux-x86_64.AppImage
```

### 当前实现细节

- 从 `assets/gif-slim.svg` 渲染出 Linux 用 PNG 图标
- 自动收集 Qt 依赖
- 额外带入 `offscreen` / `minimal` 平台插件，保证 headless 用法可用

## Windows 打包

Windows 包需要在 **原生 Windows 环境** 下执行，推荐 **MSYS2 UCRT64**。

### 推荐环境

1. 安装 [MSYS2](https://www.msys2.org/)
2. 打开 **UCRT64** shell
3. 安装依赖：

```bash
pacman -S --needed \
  git \
  make \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-tools \
  mingw-w64-ucrt-x86_64-giflib
```

每个包的用途：

| 包名 | 提供的命令 | 用途 |
| --- | --- | --- |
| `mingw-w64-ucrt-x86_64-qt6-base` | `qmake6` | 生成 Makefile |
| `make` | `make` | 构建 |
| `mingw-w64-ucrt-x86_64-gcc` | `g++` | 编译 |
| `mingw-w64-ucrt-x86_64-qt6-tools` | `windeployqt` | 自动收集 Qt DLL |
| `mingw-w64-ucrt-x86_64-giflib` | `libgif-7.dll` | GIF 编解码运行时库 |

`cp`、`mkdir`、`nproc` 来自 `coreutils`（MSYS2 默认已装）。`zip` 来自 `zip` 包（MSYS2 默认已装）。

如果你要直接生成安装包（而不是 portable zip），还需要安装 **Inno Setup**：

```powershell
# 以管理员身份在 PowerShell 中执行：
# 1. 先装 Chocolatey（https://chocolatey.org/install）
# 2. 再装 Inno Setup
choco install innosetup -y
```

`choco` 安装后 `iscc` 命令会出现在 PATH 中，脚本自动检测到后生成 `.exe` 安装包。否则退化为 portable zip（通过 PowerShell `Compress-Archive` 打包）。

### 直接打包

在 UCRT64 shell 中进入仓库根目录执行：

```bash
./package-windows.sh
```

这个脚本内部做的事情是：

1. `qmake6 GifSlim.pro`
2. `make -j...`
3. 把 `release/gif-slim.exe` 复制到 `dist/windows-portable/`
4. 执行 `windeployqt dist/windows-portable/gif-slim.exe`
5. 如果系统里有 `iscc`（Inno Setup 编译器），则继续用：

```text
packaging/windows/GifSlim.iss
```

生成安装包；否则退化为 portable zip。

### 产物

有 Inno Setup 时：

```text
dist/GifSlim-1.0.0-windows-x64-setup.exe
```

没有 Inno Setup 时：

```text
dist/GifSlim-1.0.0-windows-x64-portable.zip
```

### Windows 常见问题

| 问题 | 原因 | 处理方式 |
| --- | --- | --- |
| 找不到 `qmake6` | `mingw-w64-ucrt-x86_64-qt6-base` 没装，或不在 UCRT64 shell | 确认包已装、shell 正确，`which qmake6` 验证 |
| 找不到 `windeployqt` | `mingw-w64-ucrt-x86_64-qt6-tools` 没装 | `pacman -S mingw-w64-ucrt-x86_64-qt6-tools` |
| `cp: cannot stat 'release/gif-slim.exe'` | 构建失败，产物没生成 | 检查 `make` 输出定位错误（通常是 giflib 没装或 qmake 版本不对） |
| 运行时报缺 `libgif-7.dll` | giflib 运行库没打包进去 | 确保 `mingw-w64-ucrt-x86_64-giflib` 已安装后重新跑脚本 |
| `zip: command not found` | MSYS2 精简安装缺少 zip | `pacman -S zip`，或脚本会自动回退到 PowerShell |
| `iscc` 不存在 | 没装 Inno Setup | 用管理员 PowerShell：`choco install innosetup -y`；不需要安装包则忽略，脚本自动退化为 portable zip |
| `choco` 不存在 | 没装 Chocolatey 包管理器 | https://chocolatey.org/install 先安装，或直接下载 Inno Setup 安装 |

## macOS 打包

macOS 包需要在 **原生 macOS 环境** 下执行。

### 依赖

```bash
brew install qt giflib
```

### 执行

```bash
export PATH="$(brew --prefix qt)/bin:$PATH"
./package-macos.sh
```

产物默认输出到：

```text
dist/GifSlim-1.0.0-macos.dmg
```

脚本内部会：

1. `qmake6 GifSlim.pro`
2. `make`
3. `macdeployqt gif-slim.app -dmg`

## GitHub Actions 自动打包

工作流文件：

```text
.github/workflows/package.yml
```

### 触发方式

1. 手工触发：GitHub Actions 页面点 `package`
2. 推 tag：

```bash
git tag v1.0.0
git push origin v1.0.0
```

### 工作流行为

| Job | 环境 | 产物 |
| --- | --- | --- |
| `linux` | `ubuntu-22.04` | AppImage |
| `windows` | `windows-2022` + MSYS2 UCRT64 | Setup.exe / portable zip |
| `macos` | `macos-14` | dmg |
