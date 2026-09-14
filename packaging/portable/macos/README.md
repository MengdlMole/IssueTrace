# macOS 便携 ZIP

开发压缩包名为 `IssueTrace-<version>-macos-<arch>-dev.zip`，包含完整的 `IssueTrace.app`。用户完整解压后即可运行，不需要 Homebrew 或 Qt。当前开发包不使用 Apple Developer ID 签名或公证，因此只适合受控测试；首次运行时可能需要在 Finder 中右键打开。

面向公众的稳定包不得带 `-dev`，必须在目标架构原生构建，明确最低 macOS 版本，并完成 Developer ID 签名、公证和真机验收。归档方式必须保留应用包中的符号链接与元数据。

升级时退出旧版本，把新版解压到新目录并打开原工作区。应用内压缩包升级完成后，帮助程序负责暂存、替换 `.app`、健康检查和回滚。
