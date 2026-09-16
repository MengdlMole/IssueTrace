# IssueTrace

> 本地事件轨迹与停滞提醒：快速收下事件，随手留下排查过程，在该继续处理时提醒你。

IssueTrace 是一个本地优先的轻量事件收件箱，面向需要排查数据、日志和代码的程序员。它聚焦四件高频的事：几秒钟记下事件、连续追加处理记录、记录实际投入时间、提醒长时间没有推进的事件。完成后可以导出 Markdown 总结或 XLSX 清单。

IssueTrace 不需要账号、服务端和网络，不接管 Obsidian，也不与团队 XLSX 双向同步。SQLite 工作区是唯一数据源；Markdown 与 XLSX 只是可携带的导出结果。

## 当前发布状态

当前版本达到**内部预览版**标准，尚未达到面向公众的稳定发布标准。macOS Apple Silicon 包已完成原生构建、归档启动、滚动布局和离线升级回滚测试；Windows 包是交叉构建开发包，只验证了 PE 依赖闭包；Linux 尚未生成和验收发布物。

所有未签名、未公证或未经目标系统原生验收的压缩包均带 `-dev`。稳定发布前必须完成 [发布检查清单](docs/release-checklist.md)，不得仅删除文件名中的 `-dev`。

## 0.5.0 的核心能力

- 按 `Ctrl/Cmd + N`，只输入事件即可创建，时间自动填写；快速记录复用事件编辑表单，也可直接补充事件描述、优先级、分组、标签、提出人、处理人、版本号、服务和问题单。
- 事件管理页左侧是可折叠、可拖动的分组树，右侧展示当前分组事件；选择父分组会同时包含全部子分组。按住分组行中的 `⋮⋮` 手柄，拖到目标行上方或下方可调整同级顺序，拖到中部可连同全部子分组一起移入目标分组。未分组事件进入“默认分组”，多级分组用 `/` 分隔。
- 管理页把全文搜索、状态和优先级作为常驻快捷条件，服务、版本、问题单和累计处理时长收纳到可展开的“更多筛选”；所有生效条件都会显示为可单独移除的摘要标签，也可一键重置。事件卡片上的标签可以直接加入或取消筛选。
- 管理页可按最后修改时间、创建时间、优先级、事件名称或累计处理时间排序，并可直接打开“编辑信息”。
- 事件记录页保持左侧事件列表、右侧当前事件编辑区；左侧分为“需要关注”和“全部事件”，两块高度可拖动调整，并共用标题、描述、进展和标签搜索。默认按最近修改倒序，也可按创建时间或优先级排序并筛选优先级。
- 四个明确状态：待处理、处理中、等待、已完成；界面显示事件总年龄、当前状态停留时间和未更新时间。
- 当前事件可开始/暂停计时，累计时长与运行状态写入工作区，重启应用后仍保持；误计时可在暂停后从“更多信息”直接修正小时和分钟，后续计时从修正后的累计值继续增加。
- 可为事件设置多级分组、英文逗号分隔的多个标签和优先级；分组、提出人、处理人、服务和版本号都可复用历史输入，也允许录入新值。
- 事件日历按提出日期汇总每天收到的事件；选择日期即可回顾当天列表，点击事件直接进入记录页。
- 待处理超过 30 分钟、处理中超过 60 分钟没有活动，自动进入“需要关注”。
- 可为当前事件设置 30 分钟、2 小时、明天此时或任意未来本地时间提醒；到时使用桌面通知，关闭窗口后可由系统托盘继续运行。
- 时间线支持普通记录、进展和结论；每条自动带时间，倒序显示并可编辑、删除。
- `Ctrl/Cmd + Enter` 提交，`Ctrl/Cmd + Shift + Enter` 直接记为进展；未提交文字按问题自动保存。
- 可直接粘贴剪贴板截图，或拖放/多选日志与附件；单文件最大 100 MB。
- 最新一条“进展”自动成为事件列表中的当前进展，不必重复填写。
- “更多信息”将属性整理为两列：左侧是提出人、处理人、版本号和服务，右侧是优先级、分组、标签和问题单；事件描述支持文字、粘贴图片、选择图片和拖放图片，修改自动保存。
- 单个事件导出为自包含 Markdown 目录；当前筛选结果导出为 XLSX。
- 工作区支持完整性验证、备份和恢复；较低版本的 IssueTrace schema 会在迁移前自动备份。
- 完整压缩包解压即用，也可在应用内选择新版本压缩包完成离线升级和失败回滚。

有意不做：团队协作、云 XLSX 导入/回写、双向 Markdown 同步、内置 AI/OCR、日志解析、复杂大盘、任意工作流和在线更新。这样可以让日常主流程始终停留在一个窗口内。

## 三分钟上手

假设刚收到“支付回调偶发超时，张三反馈”：

1. 按 `Ctrl/Cmd + N`，输入事件；按需补充分组、标签、人员、版本、服务和优先级。尚未开始排查时点“先记下”；立即处理时点“保存并开始处理”。
2. 在右侧输入“先查 14:20 前后的网关日志”，按 `Ctrl/Cmd + Shift + Enter` 记录进展。
3. 截图后直接在输入框粘贴；完整日志可以拖到输入框，文字与文件会保存为同一条记录。
4. 暂时等待外部反馈时点击“等待”，并设置“2 小时后”提醒。
5. 有新发现就继续在同一个输入框追加，不需要整理格式；记录页左侧会把到期提醒、久未处理和久未更新的事件置于“需要关注”，并突出关注时间。
6. 需要查找或整理事件时返回“事件管理”；点击任意事件进入独立记录页继续处理。
7. 需要按日期复盘时打开“事件日历”，选择日期查看当天收到的事件。
8. 处理完点击“完成”，补充结论，然后点击“导出事件”选择目录。

“记录”按钮接受任意随笔；“记录进展”用于值得出现在列表摘要中的阶段结果；“结论”适合最终根因或解决办法。即使应用意外退出，输入框草稿仍会按问题恢复。

## 便携版使用

### Windows 11

1. 校验 `IssueTrace-0.5.0-windows-x86_64-dev.zip.sha256`。
2. 完整解压 ZIP 到任意可写目录，不要在压缩软件预览窗口中运行。
3. 双击 `IssueTrace.exe`。EXE、DLL、`plugins/` 和 `qml/` 必须保持原相对位置。

交叉构建包已检查 PE 依赖闭包，但正式分发前仍需在真实 Windows 11 x86_64 上做 GUI 验收。

### macOS

1. 完整解压 `IssueTrace-0.5.0-macos-<架构>-dev.zip`。
2. 打开 `IssueTrace.app`。开源开发包没有 Apple 商业签名；如被拦截，请在 Finder 中右键“打开”。

### Linux

1. 解压 `IssueTrace-0.5.0-linux-<架构>.tar.gz`。
2. 运行目录中的 `./IssueTrace` 启动脚本。正式分发前应分别在 X11 和 Wayland 验证通知、托盘、剪贴板和文件对话框。

首次启动默认使用“文档/IssueTraceWorkspace”，也可以从“工具 → 切换工作区”选择其他目录。工作区应放在程序解压目录之外。本项目尚未发布，因此不提供其他产品名称、数据库文件名或工作区目录的兼容入口。

## 数据与导出

工作区结构：

```text
IssueTraceWorkspace/
├── issuetrace.db
├── attachments/<issue-uuid>/<attachment-uuid>.<ext>
├── backups/
├── exports/
└── workspace.json
```

不要手工编辑 `issuetrace.db`。附件以问题为目录归档，数据库只保存相对路径；删除采用软删除，原文件保留。工具菜单可以验证数据库完整性和附件大小，也可以创建或恢复完整备份。

Markdown 导出结构：

```text
YYYYMMDDHH <跟踪单>-<事件标题>/
├── 事件记录.md
├── 事件总结.md
├── 图片/
└── 附件/
```

`事件记录.md` 按时间写入全部记录，并在对应记录下面插入图片或附件链接。`事件总结.md` 不复制时间线或自动列出附件，只提供事件概述、事件处理、事件解决、验证和遗留问题、后续建议和相关附件等提炼位置。目录名前十位是事件创建时间（本地时间，`YYYYMMDDHH`）。目标目录已存在时 IssueTrace 会拒绝静默覆盖。

XLSX 一行一个事件，导出当前筛选结果；时间使用真正的 Excel 日期，首行冻结并启用筛选。文本始终按字符串写入，避免公式注入。

## 使用压缩包升级

从“工具 → 从新版压缩包升级”选择同平台、同架构且版本更高的完整 ZIP 或 tar.gz：

> 包内清单和哈希只能发现文件损坏或清单外文件，不能证明压缩包来自 IssueTrace 维护者。升级前必须从可信发布渠道获取压缩包，并单独核对该渠道发布的 SHA-256。

1. IssueTrace 验证并备份外部工作区。
2. 独立升级程序验证归档路径、平台、架构、版本及逐文件 SHA-256。
3. 主程序退出后，新版在当前解压目录就地切换并启动。
4. 新版健康检查失败会恢复旧版；成功后旧版保留为同级 `.previous` 目录。

升级不需要 HTTPS、更新服务器或安装器。也可以退出旧版，把新版解压到新目录，再选择原工作区。不要直接把新文件覆盖到运行中的旧目录。

## 技术栈与许可

- C++20：领域逻辑、SQLite 仓储、导出、备份和升级程序；
- Qt 6 Community（Core、Gui、Widgets、QML、Quick、Quick Controls 2）：跨平台桌面 UI、托盘与通知；
- SQLite + FTS5：本地数据和中文全文检索；
- libxlsxwriter + zlib：XLSX 导出；
- CMake、Ninja、LLVM/GCC/MinGW-w64：构建与测试。

IssueTrace 使用 `GPL-3.0-or-later`，运行时代码只使用开源许可的组件，不包含闭源 SDK、遥测、商业 Qt 模块或在线更新服务。构建 macOS/Windows 原生程序仍不可避免地使用目标操作系统提供的 SDK 和签名设施，详见 [开源策略](docs/open-source-policy.md)。发布包包含许可证、第三方声明、依赖锁、CycloneDX SBOM、逐文件清单和 SHA-256；正式分发时还必须同时提供对应源码包。

## 构建与测试

纯核心构建：

```sh
cmake --preset core-debug
cmake --build --preset core-debug
ctest --preset core-debug --output-on-failure
```

macOS 桌面开发构建：

```sh
brew install cmake ninja qtbase qtdeclarative qtshadertools
cmake --preset macos-homebrew-debug
cmake --build --preset macos-homebrew-debug
ctest --preset macos-homebrew-debug --output-on-failure
```

便携包：

```sh
./packaging/portable/macos/build-native.sh /opt/homebrew
./packaging/portable/linux/build-native.sh /usr
./packaging/portable/windows/build-cross-macos.sh ./build/toolchains/Qt /opt/homebrew
./packaging/build-source-archive.sh
```

Windows 11 原生构建推荐使用 PowerShell：

```powershell
.\packaging\portable\windows\build-native.ps1 `
  -QtRoot C:\Qt\6.11.1\mingw_64
```

具体依赖、自动发现规则、输出文件和验证步骤见 [Windows 便携包说明](packaging/portable/windows/README.md)。macOS 交叉构建脚本只生成带 `-dev` 标记的开发包；正式发布必须使用目标平台原生流水线。构建脚本会运行测试、部署实际需要的 Qt 模块，并生成发布清单、SBOM 和校验和。

应用图标以 `resources/icons/issuetrace.svg` 为源；安装 Qt SVG 和 `pkg-config` 后可运行 `./packaging/generate-icons.sh`，重建 macOS、Windows 和通用 PNG 图标。

设计细节见 [产品边界](docs/product-spec.md)、[架构](docs/architecture.md)、[数据模型](docs/issue-schema.md)、[实施状态](docs/implementation-plan.md) 和 [发布检查清单](docs/release-checklist.md)。
