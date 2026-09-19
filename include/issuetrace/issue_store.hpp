#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <span>
#include <vector>

namespace issuetrace {

struct StoredIssue {
    std::string id;
    std::string title;
    std::string originalProblem;
    std::string reporter;
    std::string assignee;
    std::string service;
    std::string version;
    std::string ticket;
    std::string status{"pending"};
    std::string priority{"normal"};
    std::string groupName;
    std::string tags;
    std::string conclusion;
    std::int64_t reportedAt{};
    std::optional<std::int64_t> resolvedAt;
    std::int64_t createdAt{};
    std::int64_t updatedAt{};
    std::int64_t statusChangedAt{};
    std::optional<std::int64_t> remindAt;
    std::int64_t trackedMilliseconds{};
    std::optional<std::int64_t> timerStartedAt;
    std::optional<std::int64_t> deletedAt;
};

struct TimelineEntry {
    std::string id;
    std::string issueId;
    std::string type{"note"};
    std::string contentMarkdown;
    std::int64_t occurredAt{};
    std::int64_t createdAt{};
    std::int64_t updatedAt{};
    std::optional<std::int64_t> deletedAt;
};

struct Attachment {
    std::string id;
    std::string timelineEntryId;
    std::string relativePath;
    std::string originalName;
    std::string mimeType;
    std::int64_t byteSize{};
    std::string sha256;
    std::int64_t createdAt{};
};

struct IssueQuery {
    std::string text;
    std::string status;
    std::string service;
    std::string assignee;
    std::string sort{"updated_desc"};
    int staleDays{};
    std::string priority;
    std::string tag;
    std::string progress;
    std::string titleText;
    int minimumTrackedMinutes{};
    int maximumTrackedMinutes{};
    std::string groupPath;
    std::string version;
    std::string ticket;
};

struct SummaryDraft {
    std::string id;
    std::string issueId;
    std::string contentMarkdown;
    std::int64_t createdAt{};
    std::int64_t updatedAt{};
};

struct WorkspaceVerification {
    bool ok{};
    std::int64_t issueCount{};
    std::int64_t timelineCount{};
    std::int64_t attachmentCount{};
    std::vector<std::string> errors;
};

class IssueStore final {
public:
    explicit IssueStore(const std::filesystem::path& workspaceRoot);
    ~IssueStore();
    IssueStore(IssueStore&&) noexcept;
    IssueStore& operator=(IssueStore&&) noexcept;
    IssueStore(const IssueStore&) = delete;
    IssueStore& operator=(const IssueStore&) = delete;

    [[nodiscard]] const std::filesystem::path& workspaceRoot() const;
    [[nodiscard]] StoredIssue createIssue(std::string title,
                                          std::string reporter = {});
    [[nodiscard]] std::vector<StoredIssue> listIssues() const;
    [[nodiscard]] std::vector<StoredIssue> listDeletedIssues() const;
    [[nodiscard]] std::vector<StoredIssue> searchIssues(
        const IssueQuery& query) const;
    [[nodiscard]] std::optional<StoredIssue> findIssue(
        const std::string& id) const;
    void updateIssue(const StoredIssue& issue);
    void renameIssueGroupPrefix(const std::string& sourcePrefix,
                                const std::string& destinationPrefix);
    void setIssueReminder(const std::string& id,
                          std::optional<std::int64_t> remindAt);
    void setIssueTrackedMilliseconds(const std::string& id,
                                     std::int64_t trackedMilliseconds);
    void startIssueTimer(const std::string& id);
    void pauseIssueTimer(const std::string& id);
    [[nodiscard]] std::vector<std::string> distinctServices() const;
    [[nodiscard]] std::vector<std::string> distinctVersions() const;
    [[nodiscard]] std::vector<std::string> distinctReporters() const;
    [[nodiscard]] std::vector<std::string> distinctAssignees() const;
    [[nodiscard]] std::vector<std::string> distinctGroups() const;
    void softDeleteIssue(const std::string& id);
    void restoreIssue(const std::string& id);
    void permanentlyDeleteIssue(const std::string& id);
    [[nodiscard]] TimelineEntry createTimelineEntry(
        const std::string& issueId, std::string type, std::string contentMarkdown);
    [[nodiscard]] std::vector<TimelineEntry> listTimelineEntries(
        const std::string& issueId) const;
    [[nodiscard]] std::vector<TimelineEntry> listDeletedTimelineEntries() const;
    void updateTimelineEntry(const std::string& id, std::string type,
                             std::string contentMarkdown,
                             std::int64_t occurredAt);
    void softDeleteTimelineEntry(const std::string& id);
    void restoreTimelineEntry(const std::string& id);
    void permanentlyDeleteTimelineEntry(const std::string& id);
    [[nodiscard]] std::string currentProgress(const std::string& issueId) const;
    [[nodiscard]] Attachment addAttachment(
        const std::string& timelineEntryId, const std::string& originalName,
        const std::string& mimeType, const std::string& sha256,
        std::span<const unsigned char> content);
    [[nodiscard]] std::vector<Attachment> listAttachments(
        const std::string& timelineEntryId) const;
    [[nodiscard]] std::vector<Attachment> listDescriptionAttachments(
        const std::string& issueId) const;
    void softDeleteAttachment(const std::string& id);
    [[nodiscard]] std::optional<std::string> workspaceValue(
        const std::string& key) const;
    void setWorkspaceValue(const std::string& key, const std::string& value);
    [[nodiscard]] std::optional<std::string> activeFormTemplateJson() const;
    void publishFormTemplate(const std::string& templateId, int version,
                             const std::string& contentJson);
    [[nodiscard]] std::optional<SummaryDraft> findSummaryDraft(
        const std::string& issueId) const;
    [[nodiscard]] SummaryDraft saveSummaryDraft(
        const std::string& issueId, const std::string& contentMarkdown);
    [[nodiscard]] WorkspaceVerification verifyWorkspace() const;
    [[nodiscard]] std::filesystem::path createBackup(
        const std::filesystem::path& destinationRoot) const;

    [[nodiscard]] static WorkspaceVerification verifyBackup(
        const std::filesystem::path& backupRoot);
    static void restoreBackup(const std::filesystem::path& backupRoot,
                              const std::filesystem::path& workspaceRoot);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace issuetrace
