# IssueTrace 工作区与 schema 7

## 工作区

```text
IssueTraceWorkspace/
├── issuetrace.db
├── attachments/<issue-uuid>/<attachment-uuid>.<ext>
├── backups/
├── exports/
└── workspace.json
```

附件名和 MIME 类型在数据库中保留；磁盘名使用 UUID，避免重名和不安全字符。数据库保存相对路径，工作区可整体移动。

## 核心表

- `issues`：问题字段、四状态、提出/创建/更新/状态变更/解决/提醒时间及软删除时间；
- `timeline_entries`：所属问题、类型、Markdown 正文、发生/创建/更新时间和软删除时间；
- `attachments`：所属记录、相对路径、原名、MIME、大小和 SHA-256；
- `issue_search`：FTS5 trigram 全文投影；
- `metadata`：`schema_version=7`、草稿和工作区设置；
- `summary_drafts`、`form_template_versions`：保留 IssueTrace 开发期 schema 的数据与导出能力，不进入 0.4 主 UI。

`progress` 不单独存储，始终取最新一条有效 `type=progress` 的记录。`status_changed_at` 只在状态改变时更新；`remind_at` 可空，设置提醒不算一次问题活动。

## 迁移

打开较低 schema 的 IssueTrace 工作区时，程序先在 `backups/` 创建数据库快照，再迁移到 schema 7：开发期的 `verifying` 映射为 `investigating`，`resolved/closed` 映射为 `completed`，原 `updated_at` 作为初始状态变更时间。高于当前版本的数据库会被拒绝打开，避免旧程序破坏新数据。其他应用或旧项目名称下的工作区不属于兼容范围。
