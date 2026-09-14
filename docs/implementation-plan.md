# IssueTrace 0.4 实施状态

## 已完成

- [x] 产品收敛为问题收件箱、时间线和停留提醒；移除复杂页面入口。
- [x] 单页双栏 UI，固定时间线输入，倒序虚拟列表和可滚动长记录。
- [x] 收件箱、时间线输入、时间线列表和问题详情组件化，控制器通过显式 QML 单例注册。
- [x] 问题切换、状态修改、详情关闭和应用退出前刷新待保存内容。
- [x] 四状态模型，以及旧 `verifying/resolved/closed` 状态迁移。
- [x] 持久化状态变更时间和显式提醒时间。
- [x] 问题年龄、状态停留、未活动和“需要关注”投影。
- [x] 30 分钟、2 小时、明天此时提醒，桌面通知和系统托盘。
- [x] 快捷提交、按问题保存草稿、粘贴截图、拖放和批量附件。
- [x] Markdown 自包含导出、当前筛选 XLSX 导出。
- [x] Markdown/XLSX 导出从 UI 控制器拆分为可独立测试的应用服务。
- [x] schema 7 自动迁移、迁移前快照、工作区验证/备份/恢复。
- [x] 完整压缩包离线升级、健康检查和回滚。
- [x] Windows 11 原生 PowerShell 便携构建、部署、归档和升级烟测脚本。

## 发布验证

- `issuetrace_core_tests`：CRUD、状态/提醒持久化、附件、搜索、备份、迁移和 XLSX；
- `issuetrace_form_template_tests`：默认补充信息表单的解析与校验；
- `issuetrace_issue_export_service_tests`：中文路径、附件链接、状态映射、Markdown/XLSX 和覆盖保护；
- `issuetrace_update_package_tests`：清单、哈希、版本和归档边界；
- `issuetrace_updater_integration`：升级成功与失败回滚；
- `issuetrace_ui_scroll_smoke`：创建长时间线并验证视口可滚动；
- sanitizer 核心测试；
- macOS 原生便携包烟测和 Windows 交叉构建依赖闭包。
- 应用图标、macOS bundle 元数据与 Windows 文件版本资源。

## 暂缓

0.4.0 的定位是内部预览版。真实 Windows 11、Linux X11/Wayland 的人工 GUI 验收，以及 macOS 签名/公证，必须在对应发布环境执行；未完成前发布物保持 `-dev` 后缀。产品名称、域名和应用标识符也要在公开发布前最终确认。

团队 XLSX、双向 Obsidian、AI 总结和复杂大盘不在当前路线图；先用 0.4 的日常使用反馈判断是否确有必要。
