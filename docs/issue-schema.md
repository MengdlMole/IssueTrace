# IssueTrace 工作区与 schema 8

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

- `issues`：事件字段、分组、标签、优先级、计时累计值、计时开始点、四状态及各类时间；
- `timeline_entries`：所属问题、类型、Markdown 正文、发生/创建/更新时间和软删除时间；
- `attachments`：所属记录、相对路径、原名、MIME、大小和 SHA-256；
- `issue_search`：FTS5 trigram 全文投影；
- `metadata`：`schema_version=8`、草稿和工作区设置；`group_order.v1` 保存用户调整后的分组路径顺序；
- `summary_drafts`、`form_template_versions`：保留 IssueTrace 开发期 schema 的数据与导出能力，不进入 0.4 主 UI。

`progress` 不单独存储，始终取最新一条有效 `type=progress` 的记录。`status_changed_at` 只在状态改变时更新；`remind_at` 可空，设置提醒不算一次问题活动。

移动分组使用前缀事务批量改写 `group_name`：移动父分组时全部后代路径一起更新，同时重建受影响事件的全文索引，但不修改 `updated_at`，避免整理分组改变“最后修改时间”排序。

## 迁移

打开较低 schema 的 IssueTrace 工作区时，程序先在 `backups/` 创建数据库快照，再迁移到 schema 8。schema 8 新增 `group_name`、`tags`、`tracked_milliseconds` 和 `timer_started_at`；旧事件使用安全默认值。高于当前版本的数据库会被拒绝打开，避免旧程序破坏新数据。
