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
    Q_PROPERTY(QVariantMap selectedIssue READ selectedIssue NOTIFY selectedIssueChanged)
    Q_PROPERTY(QVariantList timeline READ timeline NOTIFY timelineChanged)
    Q_PROPERTY(QVariantList formFields READ formFields NOTIFY formFieldsChanged)
    Q_PROPERTY(QString workspacePath READ workspacePath NOTIFY workspacePathChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList issues() const { return issues_; }
    [[nodiscard]] QVariantList attentionIssues() const { return attentionIssues_; }
    [[nodiscard]] QVariantMap selectedIssue() const { return selectedIssue_; }
    [[nodiscard]] QVariantList timeline() const { return timeline_; }
    [[nodiscard]] QVariantList formFields() const { return formFields_; }
    [[nodiscard]] QString workspacePath() const { return workspacePath_; }
    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString status() const { return status_; }

    Q_INVOKABLE bool createQuickIssue(const QString& title,
                                      const QString& reporter);
    Q_INVOKABLE void selectIssue(const QString& id);
    Q_INVOKABLE bool saveIssue(const QVariantMap& values);
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
    Q_INVOKABLE void chooseWorkspace(const QUrl& folder);
    Q_INVOKABLE void refreshIssues();
    Q_INVOKABLE bool setSelectedIssueStatus(const QString& status);
    Q_INVOKABLE bool remindSelectedIssueIn(int minutes);
    Q_INVOKABLE bool clearSelectedIssueReminder();
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
                                  int staleDays = 0);
    Q_INVOKABLE void exportMarkdown(const QUrl& destination);
    Q_INVOKABLE void exportXlsx(const QUrl& destination);
    Q_INVOKABLE void verifyWorkspace();
    Q_INVOKABLE void backupWorkspace(const QUrl& destination);
    Q_INVOKABLE void restoreWorkspace(const QUrl& backupFolder);
    Q_INVOKABLE void applyUpdate(const QUrl& archiveFile);

signals:
    void issuesChanged();
    void attentionChanged();
    void selectedIssueChanged();
    void timelineChanged();
    void formFieldsChanged();
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
    void refreshAttention();
    QString generateSummaryContent();
    QVariantMap toVariantMap(const issuetrace::StoredIssue& issue) const;

    std::unique_ptr<issuetrace::IssueStore> store_;
    QVariantList issues_;
    QVariantList attentionIssues_;
    QVariantMap selectedIssue_;
    QVariantList timeline_;
    QVariantList formFields_;
    FormTemplateDefinition defaultTemplate_;
    FormTemplateDefinition activeTemplate_;
    issuetrace::IssueQuery activeQuery_;
    QString workspacePath_;
    QString status_{QStringLiteral("正在打开工作区…")};
    QSet<QString> notifiedReminders_;
};
