#pragma once

#include "issuetrace/issue_store.hpp"
#include "form_template.hpp"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>

#include <memory>

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList issues READ issues NOTIFY issuesChanged)
    Q_PROPERTY(QVariantList attentionIssues READ attentionIssues NOTIFY attentionChanged)
    Q_PROPERTY(QVariantList editorIssues READ editorIssues NOTIFY editorIssuesChanged)
    Q_PROPERTY(QVariantList calendarIssues READ calendarIssues NOTIFY calendarIssuesChanged)
    Q_PROPERTY(QVariantList issueGroups READ issueGroups NOTIFY issueGroupsChanged)
    Q_PROPERTY(QVariantMap selectedIssue READ selectedIssue NOTIFY selectedIssueChanged)
    Q_PROPERTY(QString selectedIssueStatus READ selectedIssueStatus NOTIFY selectedIssueChanged)
    Q_PROPERTY(QVariantList timeline READ timeline NOTIFY timelineChanged)
    Q_PROPERTY(QVariantList descriptionAttachments READ descriptionAttachments NOTIFY descriptionAttachmentsChanged)
    Q_PROPERTY(QVariantList formFields READ formFields NOTIFY formFieldsChanged)
    Q_PROPERTY(QStringList serviceOptions READ serviceOptions NOTIFY fieldOptionsChanged)
    Q_PROPERTY(QStringList versionOptions READ versionOptions NOTIFY fieldOptionsChanged)
    Q_PROPERTY(QStringList reporterOptions READ reporterOptions NOTIFY fieldOptionsChanged)
    Q_PROPERTY(QStringList assigneeOptions READ assigneeOptions NOTIFY fieldOptionsChanged)
    Q_PROPERTY(QStringList groupOptions READ groupOptions NOTIFY fieldOptionsChanged)
    Q_PROPERTY(QString workspacePath READ workspacePath NOTIFY workspacePathChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList issues() const { return issues_; }
    [[nodiscard]] QVariantList attentionIssues() const { return attentionIssues_; }
    [[nodiscard]] QVariantList editorIssues() const { return editorIssues_; }
    [[nodiscard]] QVariantList calendarIssues() const { return calendarIssues_; }
    [[nodiscard]] QVariantList issueGroups() const { return issueGroups_; }
    [[nodiscard]] QVariantMap selectedIssue() const { return selectedIssue_; }
    [[nodiscard]] QString selectedIssueStatus() const {
        return selectedIssue_.value(QStringLiteral("status")).toString();
    }
    [[nodiscard]] QVariantList timeline() const { return timeline_; }
    [[nodiscard]] QVariantList descriptionAttachments() const { return descriptionAttachments_; }
    [[nodiscard]] QVariantList formFields() const { return formFields_; }
    [[nodiscard]] QStringList serviceOptions() const { return serviceOptions_; }
    [[nodiscard]] QStringList versionOptions() const { return versionOptions_; }
    [[nodiscard]] QStringList reporterOptions() const { return reporterOptions_; }
    [[nodiscard]] QStringList assigneeOptions() const { return assigneeOptions_; }
    [[nodiscard]] QStringList groupOptions() const { return groupOptions_; }
    [[nodiscard]] QString workspacePath() const { return workspacePath_; }
    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString status() const { return status_; }

    Q_INVOKABLE bool createQuickIssue(const QString& title,
                                      const QString& reporter,
                                      const QString& assignee = {},
                                      const QString& group = {},
                                      const QString& tags = {},
                                      const QString& version = {},
                                      const QString& service = {},
                                      const QString& priority = QStringLiteral("normal"),
                                      const QString& originalProblem = {},
                                      const QString& ticket = {});
    Q_INVOKABLE void selectIssue(const QString& id);
    Q_INVOKABLE bool saveIssue(const QVariantMap& values);
    Q_INVOKABLE bool moveIssueGroup(const QString& sourcePath,
                                    const QString& targetPath,
                                    const QString& placement);
    Q_INVOKABLE bool addTimelineEntry(const QString& type, const QString& content);
    Q_INVOKABLE bool addTimelineEntryWithClipboardImage(const QString& type,
                                                        const QString& content);
    Q_INVOKABLE bool addTimelineEntryWithFiles(const QString& type,
                                               const QString& content,
                                               const QVariantList& files);
    Q_INVOKABLE bool saveTimelineEntry(const QString& id, const QString& type,
                                       const QString& content);
    Q_INVOKABLE void deleteTimelineEntry(const QString& id);
    Q_INVOKABLE void pasteScreenshot(const QString& timelineEntryId);
    Q_INVOKABLE void attachFile(const QString& timelineEntryId,
                                const QUrl& sourceFile);
    Q_INVOKABLE void openAttachment(const QUrl& file);
    Q_INVOKABLE void deleteAttachment(const QString& id);
    Q_INVOKABLE bool addDescriptionClipboardImage();
    Q_INVOKABLE bool addDescriptionImage(const QUrl& sourceFile);
    Q_INVOKABLE void chooseWorkspace(const QUrl& folder);
    Q_INVOKABLE void refreshIssues();
    Q_INVOKABLE bool setSelectedIssueStatus(const QString& status);
    Q_INVOKABLE bool remindSelectedIssueIn(int minutes);
    Q_INVOKABLE bool remindSelectedIssueAt(const QString& localDateTime);
    Q_INVOKABLE bool clearSelectedIssueReminder();
    Q_INVOKABLE bool startSelectedIssueTimer();
    Q_INVOKABLE bool pauseSelectedIssueTimer();
    Q_INVOKABLE bool setSelectedIssueTrackedDuration(int hours, int minutes);
    Q_INVOKABLE void checkReminders();
    Q_INVOKABLE bool clipboardHasImage() const;
    Q_INVOKABLE QString loadTimelineDraft(const QString& issueId) const;
    Q_INVOKABLE QString loadTimelineDraftType(const QString& issueId) const;
    Q_INVOKABLE void saveTimelineDraft(const QString& issueId,
                                       const QString& type,
                                       const QString& content);
    Q_INVOKABLE void filterIssues(const QString& text, const QString& status,
                                  const QString& service,
                                  const QString& assignee,
                                  const QString& sort = QStringLiteral("updated_desc"),
                                  int staleDays = 0,
                                  const QString& priority = {},
                                  const QString& tag = {},
                                  const QString& progress = {},
                                  int minimumTrackedMinutes = 0,
                                  int maximumTrackedMinutes = 0,
                                  const QString& groupPath = {},
                                  const QString& version = {},
                                  const QString& ticket = {});
    Q_INVOKABLE void filterEditorIssues(const QString& priority,
                                        const QString& sort,
                                        const QString& text = {});
    Q_INVOKABLE void exportMarkdown(const QUrl& destination);
    Q_INVOKABLE void exportXlsx(const QUrl& destination);
    Q_INVOKABLE void verifyWorkspace();
    Q_INVOKABLE void backupWorkspace(const QUrl& destination);
    Q_INVOKABLE void restoreWorkspace(const QUrl& backupFolder);
    Q_INVOKABLE void applyUpdate(const QUrl& archiveFile);

signals:
    void issuesChanged();
    void attentionChanged();
    void editorIssuesChanged();
    void calendarIssuesChanged();
    void issueGroupsChanged();
    void issueGroupMoved(const QString& sourcePath, const QString& destinationPath);
    void selectedIssueChanged();
    void timelineChanged();
    void descriptionAttachmentsChanged();
    void formFieldsChanged();
    void fieldOptionsChanged();
    void workspacePathChanged();
    void statusChanged();
    void reminderDue(const QString& issueId, const QString& title,
                     const QString& message);

private:
    void openWorkspace(const QString& path);
    void loadDefaultFormTemplate();
    void rebuildVisibleFormFields();
    void activateFormTemplate(FormTemplateDefinition definition);
    void setStatus(QString value);
    void setSelected(const issuetrace::StoredIssue& issue);
    void refreshTimeline();
    void refreshDescriptionAttachments();
    void refreshAttention();
    void refreshEditorIssues();
    void refreshIssueGroups();
    void refreshFieldOptions();
    QVariantMap toVariantMap(const issuetrace::StoredIssue& issue) const;

    std::unique_ptr<issuetrace::IssueStore> store_;
    QVariantList issues_;
    QVariantList attentionIssues_;
    QVariantList editorIssues_;
    QVariantList calendarIssues_;
    QVariantList issueGroups_;
    QVariantMap selectedIssue_;
    QVariantList timeline_;
    QVariantList descriptionAttachments_;
    QVariantList formFields_;
    QStringList serviceOptions_;
    QStringList versionOptions_;
    QStringList reporterOptions_;
    QStringList assigneeOptions_;
    QStringList groupOptions_;
    FormTemplateDefinition defaultTemplate_;
    FormTemplateDefinition activeTemplate_;
    issuetrace::IssueQuery activeQuery_;
    issuetrace::IssueQuery editorQuery_;
    QString workspacePath_;
    QString status_{QStringLiteral("正在打开工作区…")};
    QSet<QString> notifiedReminders_;
};
