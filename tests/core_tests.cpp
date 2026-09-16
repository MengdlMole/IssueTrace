#include "issuetrace/issue_store.hpp"
#include "issuetrace/summary_renderer.hpp"
#include "issuetrace/xlsx_exporter.hpp"

#include <sqlite3.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void testSummaryRenderingPreservesManualContent() {
    const std::string templateText =
        "---\ntitle: \"{{issue.title}}\"\n---\n"
        "# {{issue.title}}\n"
        "<!-- issuetrace:auto:timeline:start -->\n{{timeline}}\n"
        "<!-- issuetrace:auto:timeline:end -->\n"
        "手工结论\n"
        "<!-- issuetrace:auto:attachments:start -->\n{{attachments}}\n"
        "<!-- issuetrace:auto:attachments:end -->\n";
    issuetrace::SummaryRenderContext first;
    first.issueFields["title"] = "支付 \"超时\"";
    first.timelineMarkdown = "旧时间线";
    first.attachmentsMarkdown = "旧附件";
    const auto rendered = issuetrace::renderSummaryMarkdown(templateText, first);
    assert(rendered.find("title: \"支付 \\\"超时\\\"\"") != std::string::npos);

    auto manuallyEdited = rendered;
    manuallyEdited.replace(manuallyEdited.find("手工结论"),
                           std::string("手工结论").size(), "用户补充的根因");
    auto second = first;
    second.timelineMarkdown = "新时间线";
    second.attachmentsMarkdown = "新附件";
    const auto refreshed = issuetrace::refreshSummaryAutoSections(
        manuallyEdited, issuetrace::renderSummaryMarkdown(templateText, second));
    assert(refreshed.find("用户补充的根因") != std::string::npos);
    assert(refreshed.find("新时间线") != std::string::npos);
    assert(refreshed.find("旧时间线") == std::string::npos);
    assert(refreshed.find("新附件") != std::string::npos);

    bool unknownRejected = false;
    try {
        static_cast<void>(issuetrace::renderSummaryMarkdown("{{issue.missing}}", first));
    } catch (const std::exception&) {
        unknownRejected = true;
    }
    assert(unknownRejected);
}

void testXlsxExport() {
    issuetrace::XlsxTable table;
    table.headers = {"服务", "问题", "提出时间", "状态", "问题进展"};
    table.rows = {
        {{"支付服务", std::nullopt}, {"支付接口偶发超时", std::nullopt},
         {"", 1789089300000LL}, {"处理中", std::nullopt},
         {"已定位到连接池耗尽", std::nullopt}},
        {{"账户服务", std::nullopt}, {"登录失败", std::nullopt},
         {"", 1789177500000LL}, {"待验证", std::nullopt},
         {"修复已提交", std::nullopt}}};
    const auto bytes = issuetrace::buildXlsx(table);
    assert(bytes.size() > 1000);
    assert(bytes[0] == 'P' && bytes[1] == 'K');
    const auto headerOnly = issuetrace::buildXlsx({{"问题"}, {}});
    assert(headerOnly.size() > 1000);
    const auto longText = issuetrace::buildXlsx(
        {{"问题进展"}, {{{std::string(40000, 'x'), {}}}}});
    assert(longText.size() > 1000);
    bool malformedRejected = false;
    try {
        static_cast<void>(issuetrace::buildXlsx({{"问题", "状态"}, {{{"少一列", {}}}}}));
    } catch (const std::invalid_argument&) {
        malformedRejected = true;
    }
    assert(malformedRejected);
    if (const auto* output = std::getenv("ISSUETRACE_XLSX_VERIFICATION_PATH")) {
        std::ofstream file(output, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        assert(file.good());
    }
}

void testIssueStoreCrudAndPersistence() {
    const auto root = std::filesystem::temp_directory_path() /
                      "issuetrace-issue-store-tests-中文";
    const auto backupDestination = std::filesystem::temp_directory_path() /
                                   "issuetrace-backup-tests-中文";
    const auto restoredRoot = std::filesystem::temp_directory_path() /
                              "issuetrace-restored-tests-中文";
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(backupDestination);
    std::filesystem::remove_all(restoredRoot);
    std::string id;
    std::string attachmentId;
    std::string attachmentRelativePath;
    std::filesystem::path backupPath;
    std::int64_t expectedStatusChangedAt{};
    std::int64_t expectedRemindAt{};
    {
        issuetrace::IssueStore store(root);
        const auto created = store.createIssue("支付接口偶发超时", "张三");
        id = created.id;
        assert(!id.empty());
        assert(created.status == "pending");
        assert(created.statusChangedAt > 0);
        assert(std::filesystem::exists(root / "issuetrace.db"));
        assert(std::filesystem::exists(root / "workspace.json"));
        assert(std::filesystem::is_directory(root / "attachments"));
        assert(!store.workspaceValue("test_setting").has_value());
        store.setWorkspaceValue("test_setting", "中文值");
        assert(store.workspaceValue("test_setting") == "中文值");
        assert(!store.activeFormTemplateJson().has_value());
        store.publishFormTemplate("default", 1, "{\"version\":1}");
        store.publishFormTemplate("default", 2, "{\"version\":2}");
        assert(store.activeFormTemplateJson() == "{\"version\":2}");
        bool duplicateTemplateRejected = false;
        try {
            store.publishFormTemplate("default", 2, "duplicate");
        } catch (const std::exception&) {
            duplicateTemplateRejected = true;
        }
        assert(duplicateTemplateRejected);

        auto issue = store.findIssue(id);
        assert(issue.has_value());
        issue->service = "支付服务";
        issue->assignee = "李四";
        issue->status = "investigating";
        expectedStatusChangedAt = created.statusChangedAt + 1;
        expectedRemindAt = created.statusChangedAt + 3600000;
        issue->statusChangedAt = expectedStatusChangedAt;
        issue->remindAt = expectedRemindAt;
        issue->priority = "urgent";
        issue->groupName = "支付域";
        issue->tags = "线上,超时";
        issue->conclusion = "连接池耗尽";
        store.updateIssue(*issue);
        const auto updatedBeforeReminder = store.findIssue(id)->updatedAt;
        expectedRemindAt += 60000;
        store.setIssueReminder(id, expectedRemindAt);
        store.startIssueTimer(id);
        assert(store.findIssue(id)->timerStartedAt.has_value());
        store.pauseIssueTimer(id);
        assert(!store.findIssue(id)->timerStartedAt.has_value());
        store.setIssueTrackedMilliseconds(id, 5'400'000);
        assert(store.findIssue(id)->trackedMilliseconds == 5'400'000);
        bool negativeTrackedTimeRejected = false;
        try {
            store.setIssueTrackedMilliseconds(id, -1);
        } catch (const std::invalid_argument&) {
            negativeTrackedTimeRejected = true;
        }
        assert(negativeTrackedTimeRejected);
        assert(store.distinctServices() == std::vector<std::string>{"支付服务"});
        assert(store.distinctReporters() == std::vector<std::string>{"张三"});
        assert(store.distinctAssignees() == std::vector<std::string>{"李四"});
        assert(store.distinctGroups() == std::vector<std::string>{"支付域"});
        assert(store.findIssue(id)->updatedAt == updatedBeforeReminder);

        const auto note = store.createTimelineEntry(id, "note", "先检查数据库连接数");
        const auto progress = store.createTimelineEntry(
            id, "progress", "已定位到连接池耗尽");
        assert(store.listTimelineEntries(id).size() == 2);
        assert(store.currentProgress(id) == "已定位到连接池耗尽");
        store.updateTimelineEntry(note.id, "progress", "正在检查连接池");
        store.softDeleteTimelineEntry(progress.id);
        assert(store.currentProgress(id) == "正在检查连接池");

        const std::array<unsigned char, 8> pngHeader{
            0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
        const auto attachment = store.addAttachment(
            note.id, "排查截图.PNG", "image/png", "test-sha256", pngHeader);
        attachmentId = attachment.id;
        attachmentRelativePath = attachment.relativePath;
        assert(std::filesystem::exists(root / attachment.relativePath));
        assert(store.listAttachments(note.id).size() == 1);

        assert(store.searchIssues({"支付接口", "", "", ""}).size() == 1);
        assert(store.searchIssues({"正在检查连接池", "", "", ""}).size() == 1);
        assert(store.searchIssues({"排查截图", "", "", ""}).size() == 1);
        assert(store.searchIssues({"", "investigating", "支付", "李四"}).size() == 1);
        assert(store.searchIssues({"", "resolved", "", ""}).empty());
        const auto activityBeforeReminder = store.findIssue(id)->updatedAt;
        store.setIssueReminder(id, expectedRemindAt + 1);
        const auto reminderOnlyUpdate = store.findIssue(id);
        assert(reminderOnlyUpdate->remindAt == expectedRemindAt + 1);
        assert(reminderOnlyUpdate->updatedAt == activityBeforeReminder);
        store.setIssueReminder(id, expectedRemindAt);
        issuetrace::IssueQuery priorityQuery;
        priorityQuery.sort = "priority_desc";
        assert(store.searchIssues(priorityQuery).front().id == id);
        priorityQuery.priority = "urgent";
        priorityQuery.tag = "超时";
        priorityQuery.progress = "检查连接池";
        assert(store.searchIssues(priorityQuery).size() == 1);
        priorityQuery.tag = "线上";
        priorityQuery.titleText = "接口偶发";
        assert(store.searchIssues(priorityQuery).size() == 1);
        priorityQuery.titleText = "不存在";
        assert(store.searchIssues(priorityQuery).empty());
        priorityQuery.titleText.clear();

        sqlite3* timerDatabase = nullptr;
        assert(sqlite3_open((root / "issuetrace.db").string().c_str(),
                            &timerDatabase) == SQLITE_OK);
        assert(sqlite3_exec(timerDatabase,
            "UPDATE issues SET tracked_milliseconds=5400000", nullptr, nullptr,
            nullptr) == SQLITE_OK);
        sqlite3_close(timerDatabase);
        priorityQuery.minimumTrackedMinutes = 80;
        priorityQuery.maximumTrackedMinutes = 100;
        assert(store.searchIssues(priorityQuery).size() == 1);
        priorityQuery.minimumTrackedMinutes = 91;
        assert(store.searchIssues(priorityQuery).empty());

        const auto summary = store.saveSummaryDraft(id, "用户总结：线程池配置错误");
        assert(!summary.id.empty());
        assert(store.searchIssues({"线程池配置", "", "", ""}).size() == 1);
        const auto updatedSummary = store.saveSummaryDraft(id, "更新后的总结");
        assert(updatedSummary.id == summary.id);
        assert(store.searchIssues({"线程池配置", "", "", ""}).empty());
        assert(store.searchIssues({"更新后的总结", "", "", ""}).size() == 1);

        bool rolledBack = false;
        try {
            static_cast<void>(store.createTimelineEntry(
                "missing-issue", "note", "不应保存"));
        } catch (const std::exception&) {
            rolledBack = true;
        }
        assert(rolledBack);
        assert(store.listIssues().size() == 1);
        const auto health = store.verifyWorkspace();
        assert(health.ok);
        assert(health.issueCount == 1);
        assert(health.timelineCount == 1);
        assert(health.attachmentCount == 1);
        backupPath = store.createBackup(backupDestination);
        assert(issuetrace::IssueStore::verifyBackup(backupPath).ok);
        bool rejectedNestedBackup = false;
        try {
            static_cast<void>(store.createBackup(root / "attachments" / "backup"));
        } catch (const std::invalid_argument&) {
            rejectedNestedBackup = true;
        }
        assert(rejectedNestedBackup);
    }
    {
        issuetrace::IssueStore reopened(root);
        const auto restored = reopened.findIssue(id);
        assert(restored.has_value());
        assert(restored->assignee == "李四");
        assert(restored->conclusion == "连接池耗尽");
        assert(restored->statusChangedAt == expectedStatusChangedAt);
        assert(restored->remindAt == expectedRemindAt);
        assert(reopened.workspaceValue("test_setting") == "中文值");
        assert(reopened.activeFormTemplateJson() == "{\"version\":2}");
        assert(reopened.findSummaryDraft(id)->contentMarkdown == "更新后的总结");
        assert(reopened.listTimelineEntries(id).size() == 1);
        const auto entry = reopened.listTimelineEntries(id).front();
        assert(reopened.listAttachments(entry.id).size() == 1);
        reopened.softDeleteAttachment(attachmentId);
        assert(reopened.listAttachments(entry.id).empty());
        assert(reopened.searchIssues({"排查截图", "", "", ""}).empty());
        reopened.softDeleteIssue(id);
        assert(reopened.listIssues().empty());
        assert(reopened.listDeletedIssues().size() == 1);
        assert(!reopened.findIssue(id).has_value());
        reopened.restoreIssue(id);
        assert(reopened.findIssue(id).has_value());
        assert(reopened.listDeletedIssues().empty());
        reopened.softDeleteIssue(id);
    }
    {
        issuetrace::IssueStore reopened(root);
        bool rejected = false;
        try {
            static_cast<void>(reopened.createIssue("  \n", ""));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    issuetrace::IssueStore::restoreBackup(backupPath, restoredRoot);
    {
        issuetrace::IssueStore restored(restoredRoot);
        assert(restored.findIssue(id).has_value());
        assert(restored.currentProgress(id) == "正在检查连接池");
        assert(restored.verifyWorkspace().ok);
    }
    std::filesystem::remove(backupPath / attachmentRelativePath);
    assert(!issuetrace::IssueStore::verifyBackup(backupPath).ok);
    std::filesystem::remove_all(root);
    std::filesystem::remove_all(backupDestination);
    std::filesystem::remove_all(restoredRoot);
}

void testHierarchicalGroupFiltering() {
    const auto root = std::filesystem::temp_directory_path() /
                      "issuetrace-group-filter-tests";
    std::filesystem::remove_all(root);
    {
        issuetrace::IssueStore store(root);
        auto parent = store.createIssue("支付域公共事项", "");
        parent.groupName = "支付域";
        parent.tags = "线上,超时";
        parent.version = "v2.3";
        parent.ticket = "INC-100";
        store.updateIssue(parent);
        auto child = store.createIssue("支付回调事项", "");
        child.groupName = "支付域/回调";
        child.tags = "线上";
        store.updateIssue(child);
        static_cast<void>(store.createIssue("未分组事项", ""));

        issuetrace::IssueQuery query;
        query.groupPath = "支付域";
        assert(store.searchIssues(query).size() == 2);
        query.groupPath = "支付域/回调";
        assert(store.searchIssues(query).size() == 1);
        query.groupPath = "__default__";
        assert(store.searchIssues(query).size() == 1);
        query.groupPath = "不存在";
        assert(store.searchIssues(query).empty());
        query = {};
        query.tag = "线上";
        assert(store.searchIssues(query).size() == 2);
        query.tag = "线上,超时";
        assert(store.searchIssues(query).size() == 1);
        query = {};
        query.version = "2.3";
        query.ticket = "INC-1";
        assert(store.searchIssues(query).size() == 1);

        const auto updatedAt = store.findIssue(parent.id)->updatedAt;
        store.renameIssueGroupPrefix("支付域", "核心系统/支付域");
        assert(store.findIssue(parent.id)->groupName == "核心系统/支付域");
        assert(store.findIssue(child.id)->groupName == "核心系统/支付域/回调");
        assert(store.findIssue(parent.id)->updatedAt == updatedAt);
        query = {};
        query.groupPath = "核心系统";
        assert(store.searchIssues(query).size() == 2);
    }
    std::filesystem::remove_all(root);
}

void testSchemaMigrationSafety() {
    const auto migrationRoot = std::filesystem::temp_directory_path() /
                               "issuetrace-migration-tests-中文";
    std::filesystem::remove_all(migrationRoot);
    {
        issuetrace::IssueStore store(migrationRoot);
        store.setWorkspaceValue("schema_version", "5");
    }
    {
        issuetrace::IssueStore migrated(migrationRoot);
        assert(migrated.workspaceValue("schema_version") == "8");
        bool foundSnapshot = false;
        for (const auto& entry :
             std::filesystem::directory_iterator(migrationRoot / "backups")) {
            if (entry.is_regular_file() &&
                entry.path().filename().string().starts_with("PreMigration-v5-")) {
                foundSnapshot = true;
            }
        }
        assert(foundSnapshot);
        migrated.setWorkspaceValue("schema_version", "99");
    }
    bool rejectedFutureWorkspace = false;
    try {
        issuetrace::IssueStore unsupported(migrationRoot);
    } catch (const std::runtime_error&) {
        rejectedFutureWorkspace = true;
    }
    assert(rejectedFutureWorkspace);
    std::filesystem::remove_all(migrationRoot);
}

void testUnversionedWorkspaceMigration() {
    const auto root = std::filesystem::temp_directory_path() /
                      "issuetrace-unversioned-migration-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    sqlite3* database = nullptr;
    assert(sqlite3_open((root / "issuetrace.db").string().c_str(), &database) == SQLITE_OK);
    const char* sql =
        "CREATE TABLE issues("
        "id TEXT PRIMARY KEY,title TEXT NOT NULL,original_problem TEXT NOT NULL DEFAULT '',"
        "reporter TEXT NOT NULL DEFAULT '',assignee TEXT NOT NULL DEFAULT '',"
        "service TEXT NOT NULL DEFAULT '',version TEXT NOT NULL DEFAULT '',"
        "ticket TEXT NOT NULL DEFAULT '',status TEXT NOT NULL DEFAULT 'pending',"
        "priority TEXT NOT NULL DEFAULT 'normal',conclusion TEXT NOT NULL DEFAULT '',"
        "reported_at INTEGER NOT NULL,resolved_at INTEGER,created_at INTEGER NOT NULL,"
        "updated_at INTEGER NOT NULL,deleted_at INTEGER);"
        "INSERT INTO issues(id,title,status,reported_at,created_at,updated_at) "
        "VALUES('legacy','旧问题','resolved',1000,1000,2000);";
    assert(sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_close(database);
    {
        issuetrace::IssueStore migrated(root);
        const auto issue = migrated.findIssue("legacy");
        assert(issue.has_value());
        assert(issue->status == "completed");
        assert(issue->statusChangedAt == 2000);
        assert(!issue->remindAt.has_value());
        assert(migrated.workspaceValue("schema_version") == "8");
    }
    std::filesystem::remove_all(root);
}

}  // namespace

int main() {
    testSummaryRenderingPreservesManualContent();
    testXlsxExport();
    testIssueStoreCrudAndPersistence();
    testHierarchicalGroupFiltering();
    testSchemaMigrationSafety();
    testUnversionedWorkspaceMigration();
    std::cout << "All IssueTrace core tests passed.\n";
}
