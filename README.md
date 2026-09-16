# IssueTrace

> 本地事件轨迹与停滞提醒：快速收下事件，随手留下排查过程，在该继续处理时提醒你。

IssueTrace 是一个本地优先的轻量事件收件箱，面向需要排查数据、日志和代码的程序员。它聚焦四件高频的事：几秒钟记下事件、连续追加处理记录、记录实际投入时间、提醒长时间没有推进的事件。完成后可以导出 Markdown 总结或 XLSX 清单。

IssueTrace 不需要账号、服务端和网络，不接管 Obsidian，也不与团队 XLSX 双向同步。SQLite 工作区是唯一数据源；Markdown 与 XLSX 只是可携带的导出结果。

## 当前版本与定位

| 项目 | 当前值 |
|---|---|
| 应用版本 | `1.0.0`，由 `CMakeLists.txt` 的 `project(... VERSION ...)` 唯一定义 |
| 工作区 schema | `8`；打开旧 schema 前自动创建迁移快照，拒绝打开未来 schema |
| 产品阶段 | 内部预览版，不是面向公众的稳定版本 |
| 数据模式 | 单用户、本地优先；SQLite 是唯一事实来源，不要求账号或网络 |
| 许可证 | `GPL-3.0-or-later`，运行时仅使用开源许可组件 |
| 目标系统 | macOS 13+ arm64、Windows 11 x86_64、Linux x86_64/arm64 |

当前关键主流程是：

```text
快速收下事件 → 随手追加排查记录和证据 → 状态/提醒/计时 → 检索回顾 → 导出
```

## 能力边界

### 已支持

| 领域 | 当前能力 |
|---|---|
| 快速创建 | `Ctrl/Cmd + N` 打开与详情编辑共用的表单；仅事件标题必填，时间自动生成；可选填文字描述、提出人、处理人、版本号、服务、优先级、多级分组、多个标签和问题单 |
| 事件详情 | 自动保存标题、文字描述和属性；提出人、处理人、版本号、服务、分组可选择历史值或输入新值；属性采用左右两列布局 |
| 事件记录 | 普通记录、进展、结论三种类型；自动记录时间、倒序显示，支持编辑和删除；最新进展自动投影到事件列表 |
| 草稿与快捷键 | 按事件自动保存未提交草稿；`Ctrl/Cmd + Enter` 提交，`Ctrl/Cmd + Shift + Enter` 记录进展 |
| 图片与附件 | 已创建事件的描述支持粘贴、选择或拖放图片；时间线支持粘贴截图、拖放或多选任意日志/附件，单文件最大 100 MB |
| 状态 | 固定为待处理、处理中、等待、已完成；状态变化立即同步到详情、管理列表和记录页列表 |
| 关注与提醒 | 待处理超过 30 分钟、处理中超过 60 分钟未活动会进入“需要关注”；支持 30 分钟、2 小时、明天此时或自定义未来时间提醒，并通过托盘发送桌面通知 |
| 处理计时 | 开始、暂停并持久化累计处理时间；暂停后可手工修正小时和分钟，后续计时从修正值继续累加；完成事件会自动暂停 |
| 事件管理 | 左侧多级分组树，右侧事件结果；分组可折叠、同级排序或连同全部子分组拖入其他分组；空分组显示在“默认分组” |
| 检索筛选 | 全文搜索标题、描述、进展、标签、服务、版本和问题单；组合筛选状态、优先级、标签、服务、版本、问题单、分组和处理时长；条件可逐个移除或一键重置 |
| 排序 | 按最后修改时间、创建时间、优先级、事件名称或累计处理时间排序 |
| 记录页切换 | 左侧“需要关注/全部事件”区域可调整高度；支持标题、描述、进展和标签模糊搜索，以及优先级筛选和三种排序 |
| 事件日历 | 月视图按提出日期统计每天收到的事件；选择日期查看当天列表，点击后进入对应事件记录 |
| 导出 | 单个事件导出为带图片和附件链接的自包含 Markdown 目录；当前管理筛选结果导出为 XLSX；总结文档提供事件概述、事件处理、事件解决、验证和遗留问题等章节 |
| 工作区 | 切换本地工作区、完整性验证、完整备份和恢复；数据库迁移前自动备份 |
| 便携运行与升级 | 完整压缩包解压即用；选择同平台、同架构且版本更高的完整压缩包进行离线升级，失败时回滚；不依赖更新服务器 |

### 有限支持

| 能力 | 当前限制 |
|---|---|
| 快速创建图片/附件 | 快速创建表单只保存文字描述和属性；必须先创建事件，再在“更多信息”添加描述图片，或在时间线添加日志和附件 |
| 事件总结 | 导出时可以一键生成总结模板，但应用内没有独立的富文本总结编辑器，也不会覆盖或刷新已有导出目录；复杂总结应在导出后用 Obsidian 等工具补充 |
| Markdown | 仅支持单向导出，不监听外部 Markdown 修改，也不会从 Obsidian 反向同步 |
| XLSX | 仅支持导出当前筛选结果，不支持从 XLSX 创建事件、字段映射、云表单同步或回写 |
| 日历 | 仅用于按提出日期回顾事件；不支持排期、拖动日期、周视图、工时日历或日程提醒 |
| 提醒规则 | 自动关注阈值目前固定；显式提醒是单次提醒，不支持重复提醒和复杂规则 |
| 删除恢复 | 时间线和附件可软删除；事件仓储具备软删除/恢复能力，但当前 UI 没有事件删除、回收站和恢复入口 |
| 表单模板 | 内部存在固定模板解析与版本存储，但当前 UI 不支持增删字段、调整字段类型或发布自定义模板 |
| 离线升级安全 | 校验清单和 SHA-256 可发现损坏或篡改，但不能证明发布者身份；当前没有在线更新、代码签名验证或可信发布服务 |

### 当前不支持

- 多用户、团队分配、权限、评论、实时协作和云同步；
- 团队云 XLSX 导入、字段映射、定时同步或双向回写；
- Obsidian/Logseq 双向同步、外部 Markdown 监视和冲突合并；
- 内置 AI 总结、OCR、日志解析、代码分析或自动根因定位；
- 自定义状态流、任意自定义字段、用户可视化表单设计器；
- 统计大盘、图表报表、甘特图、项目计划和工单系统集成；
- 事件级删除/回收站 UI；
- 在线自动更新和后台下载；
- Windows ARM64 发布包。

这些边界是刻意的：1.0.0 优先保证个人开发者能快速记录、持续跟踪、避免停滞并完成导出，而不把应用扩展成团队工单平台。

## 发布状态

| 平台 | 当前状态 |
|---|---|
| macOS arm64 | 原生 Release、归档启动、滚动和升级回滚烟测已通过；尚未进行 Developer ID 签名和公证 |
| Windows 11 x86_64 | macOS 交叉构建和 PE 依赖闭包已通过；原生构建脚本已就绪，但尚未完成真实 Windows 11 原生构建和 GUI 人工验收 |
| Linux | 原生打包脚本已具备；尚未生成并完成 X11/Wayland 发布物验收 |

所有未签名、未公证或未经目标系统原生验收的压缩包均带 `-dev`。稳定发布前必须完成 [发布检查清单](docs/release-checklist.md)，不得仅删除文件名中的 `-dev`。

当前 macOS 开发构建的 11 项自动化测试全部通过，覆盖核心 CRUD、搜索筛选、迁移、XLSX/Markdown 导出、更新包边界、升级回滚、长页面滚动、状态即时刷新、自定义提醒、描述图片、计时修正、分组迁移、历史字段、三页面导航和事件日历投影。自动化通过不等同于三个目标系统的人工发布验收。

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

“记录”按钮接受任意随笔；“记录进展”用于值得出现在列表摘要中的阶段结果；“结论”适合最终根因或解决办法。即使应用意外退出，输入框草稿仍会按事件恢复。

## 便携版使用

### Windows 11

1. 校验 `IssueTrace-1.0.0-windows-x86_64-dev.zip.sha256`。
2. 完整解压 ZIP 到任意可写目录，不要在压缩软件预览窗口中运行。
3. 双击 `IssueTrace.exe`。EXE、DLL、`plugins/` 和 `qml/` 必须保持原相对位置。

交叉构建包已检查 PE 依赖闭包，但正式分发前仍需在真实 Windows 11 x86_64 上做 GUI 验收。

### macOS

1. 完整解压 `IssueTrace-1.0.0-macos-<架构>-dev.zip`。
2. 打开 `IssueTrace.app`。开源开发包没有 Apple 商业签名；如被拦截，请在 Finder 中右键“打开”。

### Linux

1. 解压 `IssueTrace-1.0.0-linux-<架构>.tar.gz`。
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

不要手工编辑 `issuetrace.db`。附件以事件为目录归档，数据库只保存相对路径；删除采用软删除，原文件保留。工具菜单可以验证数据库完整性和附件大小，也可以创建或恢复完整备份。

Markdown 导出结构：

```text
YYYYMMDDHH <问题单>-<事件标题>/
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

### 版本号

应用显示版本、离线升级比较、压缩包名称、SBOM 和发布清单必须使用同一个版本号。正式打包前先修改 `CMakeLists.txt`：

```cmake
project(IssueTrace VERSION 1.0.0 LANGUAGES C CXX)
```

版本只支持 `1`、`1.2`、`1.2.3` 或 `1.2.3.4` 这样的纯数字格式。打包脚本虽然提供版本参数，但该参数只覆盖归档名称和发布元数据，不能覆盖已经编译进程序的 `PROJECT_VERSION`；因此不要只靠脚本参数发布新版本。

### 便携包

```sh
# macOS 原生包
./packaging/portable/macos/build-native.sh /opt/homebrew

# Linux 原生包
./packaging/portable/linux/build-native.sh /usr

# 对应源码包
./packaging/build-source-archive.sh
```

输出统一位于 `build/artifacts/`。未经签名、公证或目标系统原生验收的包保留 `-dev` 后缀。

### 在 macOS 交叉构建 Windows 11 包

macOS 可以生成供 Windows 11 x86_64 解压运行的完整便携 ZIP。构建前需要：

- Homebrew 的 CMake、Ninja 和 `mingw-w64`；
- `/opt/homebrew` 下的 macOS Qt 6.11.1，用于运行 `moc`、`rcc` 和 QML 编译工具；
- `build/toolchains/Qt` 下的 Windows x86_64 MinGW Qt 6.11.1 目标包，其中必须包含 `lib/cmake/Qt6/qt.toolchain.cmake` 和 Windows Qt DLL。

```sh
brew install cmake ninja mingw-w64

./packaging/portable/windows/build-cross-macos.sh \
  ./build/toolchains/Qt \
  /opt/homebrew
```

输出为：

```text
build/artifacts/IssueTrace-<version>-windows-x86_64-dev.zip
build/artifacts/IssueTrace-<version>-windows-x86_64-dev.zip.sha256
```

交叉构建会生成 EXE、Qt/MinGW 运行库、插件、QML 模块、许可证、SBOM、逐文件清单和校验和，并检查 PE 依赖闭包。macOS 不能完成 Windows GUI 运行验证；发布前仍必须在真实 Windows 11 x86_64 上验证首次启动、中文路径、工作区读写、剪贴板、文件对话框、通知、托盘、升级和回滚。当前不支持 Windows ARM64。

Windows 11 原生构建推荐使用 PowerShell：

```powershell
.\packaging\portable\windows\build-native.ps1 `
  -QtRoot C:\Qt\6.11.1\mingw_64
```

具体依赖、自动发现规则、输出文件和验证步骤见 [Windows 便携包说明](packaging/portable/windows/README.md)。macOS 交叉构建脚本只生成带 `-dev` 标记的开发包；正式发布必须使用目标平台原生流水线。原生构建脚本会运行目标平台测试、部署实际需要的 Qt 模块，并生成发布清单、SBOM 和校验和。

应用图标以 `resources/icons/issuetrace.svg` 为源；安装 Qt SVG 和 `pkg-config` 后可运行 `./packaging/generate-icons.sh`，重建 macOS、Windows 和通用 PNG 图标。

设计细节见 [产品边界](docs/product-spec.md)、[架构](docs/architecture.md)、[数据模型](docs/issue-schema.md)、[实施状态](docs/implementation-plan.md) 和 [发布检查清单](docs/release-checklist.md)。
