# GIF Editor

一个基于 **Qt Widgets + giflib** 的本地 GIF 编辑器，支持：
- 拖动范围选择
- 裁剪 / 删除范围
- 抽帧 / 倍速 / 缩放 / 颜色数调整
- GUI 导出与 headless CLI 导出

当前仓库已经补齐了三平台打包脚本，但**只有 Linux AppImage 已在当前环境实际验证打通**。Windows 和 macOS 需要各自的原生环境来出包。

| 平台 | 当前产物 | 当前状态 |
| --- | --- | --- |
| Linux | `dist/GifEditor-1.0.0-linux-x86_64.AppImage` | **已验证** |
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
QT_QPA_PLATFORM=offscreen ./gif-editor --headless --open test.gif --skip 2 --save /tmp/out.gif
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
dist/GifEditor-1.0.0-linux-x86_64.AppImage
```

### 当前实现细节

- 从 `assets/gif-editor.svg` 渲染出 Linux 用 PNG 图标
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

如果你要直接生成安装包（而不是 portable zip），还需要安装 **Inno Setup**：

```powershell
choco install innosetup -y
```

### 直接打包

在 UCRT64 shell 中进入仓库根目录执行：

```bash
./package-windows.sh
```

这个脚本内部做的事情是：

1. `qmake6 GifEditor.pro`
2. `make -j...`
3. 把 `gif-editor.exe` 复制到 `dist/windows-portable/`
4. 执行 `windeployqt dist/windows-portable/gif-editor.exe`
5. 如果系统里有 `iscc`（Inno Setup 编译器），则继续用：

```text
packaging/windows/GifEditor.iss
```

生成安装包；否则退化为 portable zip。

### 产物

有 Inno Setup 时：

```text
dist/GifEditor-1.0.0-windows-x64-setup.exe
```

没有 Inno Setup 时：

```text
dist/GifEditor-1.0.0-windows-x64-portable.zip
```

### Windows 常见问题

| 问题 | 原因 | 处理方式 |
| --- | --- | --- |
| 找不到 `windeployqt` | Qt6 Tools 没装或 PATH 不对 | 确认 `mingw-w64-ucrt-x86_64-qt6-tools` 已安装，并且使用的是 UCRT64 shell |
| 打出来只有 exe 没有 Qt DLL | 没执行 `windeployqt` | 不要手工只拷 exe，直接用 `./package-windows.sh` |
| `iscc` 不存在 | 没装 Inno Setup | 安装 Inno Setup，或接受 portable zip 产物 |
| 运行时报缺 `giflib` 相关 DLL | Windows 运行库没被一起带出 | 确保 `mingw-w64-ucrt-x86_64-giflib` 已安装后重新打包 |

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
dist/GifEditor-1.0.0-macos.dmg
```

脚本内部会：

1. `qmake6 GifEditor.pro`
2. `make`
3. `macdeployqt gif-editor.app -dmg`

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

## assets 目录怎么管理

当前仓库里：

```text
assets/gif-editor.svg
```

它应该被当成**单一源图（source of truth）**。

### 开源项目里常见的做法

网上大多数桌面开源项目不会手工维护三套完全独立的图标，而是：

| 资产类型 | 常见做法 |
| --- | --- |
| 源图 | 保留 1 份 SVG，或者 1 份 1024x1024 的高分辨率 PNG |
| Linux 图标 | 从源图导出不同尺寸 PNG，或直接保留 SVG + desktop entry |
| Windows 图标 | 由多尺寸 PNG 打成一个 `.ico` |
| macOS 图标 | 由一组固定命名的 PNG 组成 `.iconset`，再用 `iconutil` 生成 `.icns` |
| 自动化位置 | 放在 `scripts/`、`tools/`、`packaging/`，构建时或发布前统一生成 |

### 常见生成工具

| 目标 | 常用工具 |
| --- | --- |
| SVG -> PNG | `rsvg-convert`、Inkscape、ImageMagick |
| PNG -> ICO | ImageMagick (`magick`)、png-to-ico |
| PNG/iconset -> ICNS | macOS `iconutil`、`png2icns` |

### 为什么很多开源项目仍然把 `.ico` / `.icns` 提交进仓库

因为：

1. Windows 和 macOS 的打包工具通常直接消费 `.ico` / `.icns`
2. 这些文件是平台分发时的稳定输入，不希望每次打包都临时手工生成
3. CI / 本地环境不一定都有完整图像处理链路

所以常见实践是：

- **源图**长期保存在 `assets/icon.svg`
- **派生图**（`icon.ico`、`icon.icns`、若干 PNG）通过脚本生成
- **是否提交派生文件**取决于团队偏好  
  - 小项目常常直接提交，省去每次生成
  - 更自动化的项目则在 CI 或 release 脚本里统一生成

### 这仓库推荐的管理方式

建议保持：

```text
assets/gif-editor.svg
```

作为唯一设计源图，然后增加一个专门的图标生成脚本，例如：

```text
scripts/generate-icons.sh
```

统一生成：

```text
assets/gif-editor-16.png
assets/gif-editor-32.png
assets/gif-editor-256.png
assets/gif-editor.ico
assets/gif-editor.icns
```

这样：
- Linux 打包继续直接用 SVG / PNG
- Windows qmake 的 `RC_ICONS`
- macOS qmake 的 `ICON`

都能有稳定输入。

### 参考生成思路

#### Linux / 通用 PNG

```bash
rsvg-convert -w 256 -h 256 assets/gif-editor.svg -o assets/gif-editor-256.png
```

#### Windows `.ico`

```bash
magick assets/gif-editor.svg \
  -define icon:auto-resize=256,128,64,48,32,16 \
  assets/gif-editor.ico
```

#### macOS `.icns`

在 macOS 上：

```bash
mkdir -p assets/gif-editor.iconset
rsvg-convert -w 16 -h 16 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_16x16.png
rsvg-convert -w 32 -h 32 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_16x16@2x.png
rsvg-convert -w 32 -h 32 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_32x32.png
rsvg-convert -w 64 -h 64 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_32x32@2x.png
rsvg-convert -w 128 -h 128 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_128x128.png
rsvg-convert -w 256 -h 256 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_128x128@2x.png
rsvg-convert -w 256 -h 256 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_256x256.png
rsvg-convert -w 512 -h 512 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_256x256@2x.png
rsvg-convert -w 512 -h 512 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_512x512.png
rsvg-convert -w 1024 -h 1024 assets/gif-editor.svg -o assets/gif-editor.iconset/icon_512x512@2x.png
iconutil -c icns assets/gif-editor.iconset -o assets/gif-editor.icns
```

## 当前建议

如果你的目标是“先让别人能稳定打包”，当前最实用的顺序是：

1. 先按本文档跑通 Linux / Windows / macOS 的原生打包
2. 再补一个统一的图标生成脚本
3. 最后把 `.ico` / `.icns` 是否提交进仓库固定下来
