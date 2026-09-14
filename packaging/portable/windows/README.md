# Windows 11 便携 ZIP

完整解压 `IssueTrace-<version>-windows-x86_64.zip`，进入解压出的版本目录并运行 `IssueTrace.exe`。无需安装、管理员权限或额外 Qt 运行环境。

包内的 DLL、`plugins/`、`qml/`、`qt.conf`、清单和许可文件必须与 EXE 一起保留。当前交叉构建脚本为：

```sh
./packaging/portable/windows/build-cross-macos.sh \
  ./build/toolchains/Qt \
  /opt/homebrew \
  0.4.0
```

版本参数可省略，脚本会读取 `CMakeLists.txt` 中的项目版本。当前交叉构建输出带 `-dev` 标记的完整 ZIP 和配套 `.sha256` 文件；包中不包含静态库、开发头文件或旧版映射样例。正式发布包必须由原生平台流水线生成。

交叉构建只能验证 PE 格式和依赖闭包。发布前必须在真实 Windows 11 x86_64 上验证中文路径、首次启动、工作区读写、剪贴板、文件选择、压缩包升级、失败回滚和安全软件提示。
