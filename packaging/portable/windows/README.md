# Windows 11 便携 ZIP

完整解压 `IssueTrace-<version>-windows-x86_64.zip`，进入解压出的版本目录并运行 `IssueTrace.exe`。无需安装、管理员权限或额外 Qt 运行环境。

包内的 DLL、`plugins/`、`qml/`、`qt.conf`、清单和许可文件必须与 EXE 一起保留。

## Windows 11 原生构建（推荐）

构建机要求：

- Windows 11 x86_64；
- Git、CMake 3.24+、Ninja；
- Qt Online Installer 安装的 Qt 6.11.1 MinGW 64-bit 和对应 MinGW 工具链；
- Windows 自带的 `tar.exe`，用于离线升级烟测；
- PowerShell 5.1+ 或 PowerShell 7。

在 PowerShell 中从仓库根目录执行：

```powershell
.\packaging\portable\windows\build-native.ps1 `
  -QtRoot C:\Qt\6.11.1\mingw_64
```

`-QtRoot` 可通过 `QTDIR` 提供。`-MinGwRoot` 省略时脚本会先检查 `PATH`，再尝试从 `C:\Qt\Tools` 自动发现；如果安装了多个 MinGW 版本，脚本会停止并要求显式传入与 Qt 6.11.1 配套的目录，例如 `-MinGwRoot C:\Qt\Tools\mingwXXXX_64`，不会冒险选错 ABI。

脚本依次完成：

1. 固定使用内置 SQLite、zlib 和依赖锁指定的 Qt 6.11.1；
2. Release 配置、编译和全部可在 Windows 运行的测试；
3. 使用 `windeployqt` 收集 Qt DLL、插件及 QML 模块；
4. 排除软件 OpenGL、系统 D3D 编译器和 Qt WebEngine；
5. 收集许可证，生成 CycloneDX SBOM 和逐文件发布清单；
6. 生成完整便携 ZIP 及 `.sha256`；
7. 从 ZIP 模拟原地升级，并运行启动和滚动布局烟测。

输出位置：

```text
build\artifacts\IssueTrace-<version>-windows-x86_64-dev.zip
build\artifacts\IssueTrace-<version>-windows-x86_64-dev.zip.sha256
```

原生脚本默认保留 `-dev` 标记。只有完成 `docs/release-checklist.md` 中的真实 Windows 11 人工验收、安全软件检查和发布者身份方案后，才能将其作为稳定发布候选物。

## macOS 交叉构建（仅开发验证）

交叉构建脚本为：

```sh
./packaging/portable/windows/build-cross-macos.sh \
  ./build/toolchains/Qt \
  /opt/homebrew \
  0.5.0
```

版本参数可省略，脚本会读取 `CMakeLists.txt` 中的项目版本。交叉构建输出带 `-dev` 标记的完整 ZIP 和配套 `.sha256` 文件；包中不包含静态库、开发头文件或旧版映射样例。正式发布包必须由原生平台流水线生成。

交叉构建只能验证 PE 格式和依赖闭包。发布前必须在真实 Windows 11 x86_64 上验证中文路径、首次启动、工作区读写、剪贴板、文件选择、压缩包升级、失败回滚和安全软件提示。
