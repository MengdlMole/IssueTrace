#include "app_controller.hpp"
#include "form_template.hpp"
#include "issuetrace/summary_renderer.hpp"
#include "issuetrace/xlsx_exporter.hpp"

#include <QDateTime>
#include <QBuffer>
#include <QCoreApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::filesystem::path nativePath(const QString& path) {
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toUtf8().constData());
#endif
}

QString fromUtf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toUtf8(const QVariantMap& values, const char* key) {
    return values.value(QString::fromLatin1(key)).toString().toUtf8().toStdString();
}

std::string issueValue(const QVariantMap& values, const char* snakeCase,
                       const char* legacyCamelCase = nullptr) {
    const auto key = QString::fromLatin1(snakeCase);
    if (values.contains(key)) return values.value(key).toString().toUtf8().toStdString();
    return legacyCamelCase ? toUtf8(values, legacyCamelCase) : std::string{};
}

QString timelineTypeLabel(const std::string& type) {
    static const QHash<QString, QString> labels{
        {QStringLiteral("note"), QStringLiteral("随笔")},
        {QStringLiteral("progress"), QStringLiteral("进展")},
        {QStringLiteral("data"), QStringLiteral("数据排查")},
        {QStringLiteral("log"), QStringLiteral("日志")},
        {QStringLiteral("code"), QStringLiteral("代码梳理")},
        {QStringLiteral("solution"), QStringLiteral("解决方案")},
        {QStringLiteral("verification"), QStringLiteral("验证")}};
    const auto key = QString::fromUtf8(type.data(), static_cast<qsizetype>(type.size()));
    return labels.value(key, key);
}

QString optionLabel(const QVariantMap& field, const std::string& value) {
    const auto key = fromUtf8(value);
    for (const auto& item : field.value(QStringLiteral("options")).toList()) {
        const auto option = item.toMap();
        if (option.value(QStringLiteral("value")).toString() == key) {
            return option.value(QStringLiteral("label")).toString();
        }
    }
    return key;
}

std::string templateOptionValue(const QVariantList& fields, const char* id,
                                const std::string& raw) {
    if (QString::fromLatin1(id) == QStringLiteral("status")) {
        static const QHash<QString, QString> statusLabels{
            {QStringLiteral("pending"), QStringLiteral("待处理")},
            {QStringLiteral("investigating"), QStringLiteral("处理中")},
            {QStringLiteral("waiting"), QStringLiteral("等待")},
            {QStringLiteral("completed"), QStringLiteral("已完成")}};
        const auto key = fromUtf8(raw);
        if (statusLabels.contains(key)) {
            return statusLabels.value(key).toUtf8().toStdString();
        }
    }
    for (const auto& item : fields) {
        const auto field = item.toMap();
        if (field.value(QStringLiteral("id")).toString() == QString::fromLatin1(id)) {
            return optionLabel(field, raw).toUtf8().toStdString();
        }
    }
    return raw;
}

QString portablePackageRoot() {
    QDir directory(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 6; ++level) {
        if (QFileInfo::exists(directory.filePath(QStringLiteral("release-manifest.json")))) {
            return directory.absolutePath();
        }
        if (!directory.cdUp()) break;
    }
    return {};
}

QString elapsedText(const std::int64_t since, const std::int64_t now) {
    const auto minutes = std::max<std::int64_t>(0, (now - since) / 60000);
    if (minutes < 1) return QStringLiteral("刚刚");
    if (minutes < 60) return QStringLiteral("%1 分钟").arg(minutes);
    const auto hours = minutes / 60;
    if (hours < 24) return QStringLiteral("%1 小时 %2 分钟").arg(hours).arg(minutes % 60);
    return QStringLiteral("%1 天 %2 小时").arg(hours / 24).arg(hours % 24);
}

bool localPathIsWithin(QString candidate, QString parent) {
    candidate = QDir::cleanPath(QFileInfo(candidate).absoluteFilePath());
    parent = QDir::cleanPath(QFileInfo(parent).absoluteFilePath());
#ifdef _WIN32
    candidate = candidate.toLower();
    parent = parent.toLower();
#endif
    return candidate == parent || candidate.startsWith(
        parent + QDir::separator());
}

issuetrace::XlsxCell xlsxCell(const QVariantMap& field,
                            const issuetrace::StoredIssue& issue,
                            const issuetrace::IssueStore& store) {
    const auto id = field.value(QStringLiteral("id")).toString();
    if (id == QStringLiteral("reported_at")) return {{}, issue.reportedAt};
    if (id == QStringLiteral("resolved_at")) {
        return issue.resolvedAt ? issuetrace::XlsxCell{{}, *issue.resolvedAt}
                                : issuetrace::XlsxCell{};
    }
    if (id == QStringLiteral("title")) return {issue.title, {}};
    if (id == QStringLiteral("original_problem")) return {issue.originalProblem, {}};
    if (id == QStringLiteral("reporter")) return {issue.reporter, {}};
    if (id == QStringLiteral("assignee")) return {issue.assignee, {}};
    if (id == QStringLiteral("service")) return {issue.service, {}};
    if (id == QStringLiteral("version")) return {issue.version, {}};
    if (id == QStringLiteral("ticket")) return {issue.ticket, {}};
    if (id == QStringLiteral("status")) {
        return {optionLabel(field, issue.status).toUtf8().toStdString(), {}};
    }
    if (id == QStringLiteral("priority")) {
        return {optionLabel(field, issue.priority).toUtf8().toStdString(), {}};
    }
    if (id == QStringLiteral("progress")) return {store.currentProgress(issue.id), {}};
    if (id == QStringLiteral("conclusion")) return {issue.conclusion, {}};
    return {};
}

std::string safeExportName(std::string value) {
    for (auto& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 32 || character == '/' || character == '\\' || character == ':' ||
            character == '*' || character == '?' || character == '"' || character == '<' ||
            character == '>' || character == '|') character = '_';
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) value.pop_back();
    return value.empty() ? std::string("IssueTrace-Export") : value;
}

}  // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
    loadDefaultFormTemplate();
    auto path = qEnvironmentVariable("ISSUETRACE_WORKSPACE");
    if (path.isEmpty()) {
        path = QSettings{}.value(QStringLiteral("workspace/path")).toString();
    }
    if (path.isEmpty()) {
        auto documents = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);
        if (documents.isEmpty()) documents = QDir::homePath();
        path = QDir(documents).filePath(QStringLiteral("IssueTraceWorkspace"));
    }
    openWorkspace(path);
}

QString AppController::appVersion() const {
    return QStringLiteral(ISSUETRACE_APP_VERSION);
}

bool AppController::createQuickIssue(const QString& title,
                                     const QString& reporter) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        const auto issue = store_->createIssue(
            title.trimmed().toUtf8().toStdString(),
            reporter.trimmed().toUtf8().toStdString());
        refreshIssues();
        setSelected(issue);
        refreshTimeline();
        setStatus(QStringLiteral("问题已创建，可以继续补充详情"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("创建失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

void AppController::selectIssue(const QString& id) {
    try {
        if (!store_) return;
        const auto issue = store_->findIssue(id.toUtf8().toStdString());
        if (!issue) throw std::runtime_error("问题不存在或已删除");
        setSelected(*issue);
        refreshTimeline();
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("打开失败：") + QString::fromUtf8(error.what()));
    }
}

bool AppController::saveIssue(const QVariantMap& values) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        for (const auto& item : activeTemplate_.fields) {
            const auto field = item.toMap();
            if (!field.value(QStringLiteral("required")).toBool() ||
                field.value(QStringLiteral("readOnly")).toBool()) continue;
            const auto id = field.value(QStringLiteral("id")).toString();
            if (values.value(id).toString().trimmed().isEmpty()) {
                throw std::runtime_error(
                    (field.value(QStringLiteral("label")).toString() +
                     QStringLiteral("不能为空")).toUtf8().constData());
            }
        }
        const auto id = toUtf8(values, "id");
        auto issue = store_->findIssue(id);
        if (!issue) throw std::runtime_error("问题不存在或已删除");
        issue->title = toUtf8(values, "title");
        issue->originalProblem = issueValue(values, "original_problem", "originalProblem");
        issue->reporter = toUtf8(values, "reporter");
        issue->assignee = toUtf8(values, "assignee");
        issue->service = toUtf8(values, "service");
        issue->version = toUtf8(values, "version");
        issue->ticket = toUtf8(values, "ticket");
        const auto oldStatus = issue->status;
        issue->status = toUtf8(values, "status");
        issue->priority = toUtf8(values, "priority");
        issue->conclusion = toUtf8(values, "conclusion");
        const auto isResolved = issue->status == "completed";
        const auto wasResolved = oldStatus == "completed";
        if (issue->status != oldStatus) {
            issue->statusChangedAt = QDateTime::currentMSecsSinceEpoch();
        }
        if (isResolved && !wasResolved && !issue->resolvedAt) {
            issue->resolvedAt = QDateTime::currentMSecsSinceEpoch();
        }
        if (isResolved) issue->remindAt.reset();
        store_->updateIssue(*issue);
        refreshIssues();
        if (const auto saved = store_->findIssue(id)) setSelected(*saved);
        setStatus(QStringLiteral("修改已保存"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("保存失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

void AppController::loadDefaultFormTemplate() {
    QFile file(QStringLiteral(":/issuetrace/resources/templates/forms/default-issue.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("无法读取内置表单模板");
    }
    defaultTemplate_ = parseFormTemplate(file.readAll());
    activateFormTemplate(defaultTemplate_);
}

void AppController::rebuildVisibleFormFields() {
    QVariantList visible;
    for (const auto& item : activeTemplate_.fields) {
        if (item.toMap().value(QStringLiteral("detailVisible")).toBool()) {
            visible.push_back(item);
        }
    }
    formFields_ = std::move(visible);
    emit formFieldsChanged();
}

void AppController::activateFormTemplate(FormTemplateDefinition definition) {
    activeTemplate_ = std::move(definition);
    rebuildVisibleFormFields();
}

bool AppController::addTimelineEntry(const QString& type, const QString& content) {
    try {
        if (!store_ || selectedIssue_.isEmpty()) {
            throw std::runtime_error("请先选择一个问题");
        }
        static_cast<void>(store_->createTimelineEntry(
            toUtf8(selectedIssue_, "id"), type.toUtf8().toStdString(),
            content.trimmed().toUtf8().toStdString()));
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("跟踪记录已添加"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("记录失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::addTimelineEntryWithClipboardImage(const QString& type,
                                                       const QString& content) {
    std::string entryId;
    try {
        if (!store_ || selectedIssue_.isEmpty()) {
            throw std::runtime_error("请先选择一个问题");
        }
        const auto image = QGuiApplication::clipboard()->image();
        if (image.isNull()) throw std::runtime_error("剪贴板中没有图片");
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            throw std::runtime_error("无法编码剪贴板图片");
        }
        const auto body = content.trimmed().isEmpty() ? QStringLiteral("截图记录")
                                                       : content.trimmed();
        const auto entry = store_->createTimelineEntry(
            toUtf8(selectedIssue_, "id"), type.toUtf8().toStdString(),
            body.toUtf8().toStdString());
        entryId = entry.id;
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                              .toHex().toStdString();
        const auto now = QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd-HHmmss"));
        static_cast<void>(store_->addAttachment(
            entry.id, QStringLiteral("截图-%1.png").arg(now).toUtf8().toStdString(),
            "image/png", hash,
            {reinterpret_cast<const unsigned char*>(bytes.constData()),
             static_cast<std::size_t>(bytes.size())}));
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("截图记录已添加"));
        return true;
    } catch (const std::exception& error) {
        if (store_ && !entryId.empty()) {
            try { store_->softDeleteTimelineEntry(entryId); } catch (...) {}
        }
        setStatus(QStringLiteral("截图记录失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::addTimelineEntryWithFiles(const QString& type,
                                              const QString& content,
                                              const QVariantList& files) {
    struct PendingAttachment {
        std::string name;
        std::string mime;
        std::string hash;
        QByteArray bytes;
    };
    std::vector<PendingAttachment> pending;
    std::string entryId;
    try {
        if (!store_ || selectedIssue_.isEmpty()) {
            throw std::runtime_error("请先选择一个问题");
        }
        if (files.isEmpty()) throw std::runtime_error("请选择至少一个附件");
        QMimeDatabase mimeDatabase;
        for (const auto& value : files) {
            const auto url = value.toUrl();
            if (!url.isLocalFile()) throw std::runtime_error("附件必须是本地文件");
            QFile file(url.toLocalFile());
            if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("无法读取附件");
            if (file.size() > 100LL * 1024 * 1024) {
                throw std::runtime_error("单个附件不能超过 100 MB");
            }
            PendingAttachment item;
            item.name = QFileInfo(file).fileName().toUtf8().toStdString();
            item.mime = mimeDatabase.mimeTypeForFile(file.fileName())
                            .name().toUtf8().toStdString();
            item.bytes = file.readAll();
            item.hash = QCryptographicHash::hash(item.bytes, QCryptographicHash::Sha256)
                            .toHex().toStdString();
            pending.push_back(std::move(item));
        }
        const auto body = content.trimmed().isEmpty() ? QStringLiteral("附件记录")
                                                       : content.trimmed();
        const auto entry = store_->createTimelineEntry(
            toUtf8(selectedIssue_, "id"), type.toUtf8().toStdString(),
            body.toUtf8().toStdString());
        entryId = entry.id;
        for (const auto& item : pending) {
            static_cast<void>(store_->addAttachment(
                entry.id, item.name, item.mime, item.hash,
                {reinterpret_cast<const unsigned char*>(item.bytes.constData()),
                 static_cast<std::size_t>(item.bytes.size())}));
        }
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("记录和 %1 个附件已添加").arg(pending.size()));
        return true;
    } catch (const std::exception& error) {
        if (store_ && !entryId.empty()) {
            try { store_->softDeleteTimelineEntry(entryId); } catch (...) {}
        }
        setStatus(QStringLiteral("附件记录失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::saveTimelineEntry(const QString& id, const QString& type,
                                      const QString& content) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        store_->updateTimelineEntry(id.toUtf8().toStdString(),
                                    type.toUtf8().toStdString(),
                                    content.trimmed().toUtf8().toStdString());
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("跟踪记录已更新"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("更新失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

void AppController::deleteTimelineEntry(const QString& id) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        store_->softDeleteTimelineEntry(id.toUtf8().toStdString());
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("跟踪记录已删除"));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("删除失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::pasteScreenshot(const QString& timelineEntryId) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        const auto image = QGuiApplication::clipboard()->image();
        if (image.isNull()) throw std::runtime_error("剪贴板中没有图片");
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            throw std::runtime_error("无法编码剪贴板图片");
        }
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                              .toHex().toStdString();
        const auto now = QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd-HHmmss"));
        static_cast<void>(store_->addAttachment(
            timelineEntryId.toUtf8().toStdString(),
            QStringLiteral("截图-%1.png").arg(now).toUtf8().toStdString(),
            "image/png", hash,
            {reinterpret_cast<const unsigned char*>(bytes.constData()),
             static_cast<std::size_t>(bytes.size())}));
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("截图已保存到工作区"));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("粘贴失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::attachFile(const QString& timelineEntryId,
                               const QUrl& sourceFile) {
    try {
        if (!store_ || !sourceFile.isLocalFile()) {
            throw std::runtime_error("请选择本地文件");
        }
        QFile file(sourceFile.toLocalFile());
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("无法读取附件");
        constexpr qint64 maximumSize = 100 * 1024 * 1024;
        if (file.size() > maximumSize) throw std::runtime_error("单个附件不能超过 100 MB");
        const auto bytes = file.readAll();
        const auto info = QFileInfo(file);
        const auto mime = QMimeDatabase{}.mimeTypeForFile(info).name();
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                              .toHex().toStdString();
        static_cast<void>(store_->addAttachment(
            timelineEntryId.toUtf8().toStdString(),
            info.fileName().toUtf8().toStdString(), mime.toUtf8().toStdString(), hash,
            {reinterpret_cast<const unsigned char*>(bytes.constData()),
             static_cast<std::size_t>(bytes.size())}));
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("附件已复制到工作区"));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("添加附件失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::openAttachment(const QUrl& file) {
    if (!file.isLocalFile() || !QDesktopServices::openUrl(file)) {
        setStatus(QStringLiteral("无法使用系统默认程序打开附件"));
    }
}

void AppController::deleteAttachment(const QString& id) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        store_->softDeleteAttachment(id.toUtf8().toStdString());
        refreshTimeline();
        refreshIssues();
        setStatus(QStringLiteral("附件已移入回收站"));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("删除附件失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::chooseWorkspace(const QUrl& folder) {
    if (folder.isLocalFile()) openWorkspace(folder.toLocalFile());
}

void AppController::refreshIssues() {
    if (!store_) return;
    try {
        QVariantList refreshed;
        for (const auto& issue : store_->searchIssues(activeQuery_)) {
            refreshed.push_back(toVariantMap(issue));
        }
        issues_ = std::move(refreshed);
        emit issuesChanged();
        refreshAttention();
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("读取失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::refreshAttention() {
    if (!store_) return;
    QVariantList attention;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const auto all = store_->listIssues();
    for (const auto& issue : all) {
        const auto needsAttention = issue.status != "completed" &&
            ((issue.remindAt && *issue.remindAt <= now) ||
             (issue.status == "pending" && now - issue.createdAt >= 30LL * 60000) ||
             (issue.status == "investigating" && now - issue.updatedAt >= 60LL * 60000));
        if (needsAttention) attention.push_back(toVariantMap(issue));
    }
    attentionIssues_ = std::move(attention);
    emit attentionChanged();
}

bool AppController::setSelectedIssueStatus(const QString& status) {
    if (selectedIssue_.isEmpty()) return false;
    auto values = selectedIssue_;
    values.insert(QStringLiteral("status"), status);
    return saveIssue(values);
}

bool AppController::remindSelectedIssueIn(const int minutes) {
    try {
        if (!store_ || selectedIssue_.isEmpty() || minutes <= 0) {
            throw std::runtime_error("提醒时间无效");
        }
        const auto id = toUtf8(selectedIssue_, "id");
        auto issue = store_->findIssue(id);
        if (!issue) throw std::runtime_error("问题不存在");
        const auto remindAt = QDateTime::currentMSecsSinceEpoch() +
                              static_cast<qint64>(minutes) * 60000;
        store_->setIssueReminder(id, remindAt);
        notifiedReminders_.clear();
        refreshIssues();
        setSelected(*store_->findIssue(id));
        setStatus(QStringLiteral("提醒已设置：%1")
                      .arg(QDateTime::fromMSecsSinceEpoch(remindAt)
                               .toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("设置提醒失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::clearSelectedIssueReminder() {
    try {
        if (!store_ || selectedIssue_.isEmpty()) return false;
        const auto id = toUtf8(selectedIssue_, "id");
        auto issue = store_->findIssue(id);
        if (!issue) throw std::runtime_error("问题不存在");
        store_->setIssueReminder(id, std::nullopt);
        notifiedReminders_.clear();
        refreshIssues();
        setSelected(*store_->findIssue(id));
        setStatus(QStringLiteral("提醒已取消"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("取消提醒失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

void AppController::checkReminders() {
    if (!store_) return;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& issue : store_->listIssues()) {
        if (!issue.remindAt || *issue.remindAt > now || issue.status == "completed") continue;
        const auto key = fromUtf8(issue.id) + QStringLiteral(":") +
                         QString::number(*issue.remindAt);
        if (notifiedReminders_.contains(key)) continue;
        notifiedReminders_.insert(key);
        emit reminderDue(fromUtf8(issue.id), fromUtf8(issue.title),
                         QStringLiteral("已到提醒时间，最近一次活动在 %1前")
                             .arg(elapsedText(issue.updatedAt, now)));
    }
    refreshAttention();
}

bool AppController::clipboardHasImage() const {
    return !QGuiApplication::clipboard()->image().isNull();
}

QString AppController::loadTimelineDraft(const QString& issueId) const {
    if (!store_ || issueId.isEmpty()) return {};
    const auto value = store_->workspaceValue(
        "timeline_draft." + issueId.toUtf8().toStdString());
    if (!value) return {};
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(*value));
    return document.isObject()
        ? document.object().value(QStringLiteral("content")).toString() : QString{};
}

QString AppController::loadTimelineDraftType(const QString& issueId) const {
    if (!store_ || issueId.isEmpty()) return QStringLiteral("note");
    const auto value = store_->workspaceValue(
        "timeline_draft." + issueId.toUtf8().toStdString());
    if (!value) return QStringLiteral("note");
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(*value));
    const auto type = document.isObject()
        ? document.object().value(QStringLiteral("type")).toString() : QString{};
    return type.isEmpty() ? QStringLiteral("note") : type;
}

void AppController::saveTimelineDraft(const QString& issueId,
                                      const QString& type,
                                      const QString& content) {
    if (!store_ || issueId.isEmpty()) return;
    QJsonObject object{{QStringLiteral("type"), type},
                       {QStringLiteral("content"), content},
                       {QStringLiteral("updatedAt"),
                        QDateTime::currentMSecsSinceEpoch()}};
    store_->setWorkspaceValue(
        "timeline_draft." + issueId.toUtf8().toStdString(),
        QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString());
}

void AppController::filterIssues(const QString& text, const QString& status,
                                 const QString& service,
                                 const QString& assignee,
                                 const QString& sort,
                                 int staleDays) {
    activeQuery_.text = text.trimmed().toUtf8().toStdString();
    activeQuery_.status = status.toUtf8().toStdString();
    activeQuery_.service = service.trimmed().toUtf8().toStdString();
    activeQuery_.assignee = assignee.trimmed().toUtf8().toStdString();
    activeQuery_.sort = sort.toUtf8().toStdString();
    activeQuery_.staleDays = staleDays;
    refreshIssues();
    setStatus(QStringLiteral("已找到 %1 个问题").arg(issues_.size()));
}

QString AppController::generateSummaryContent() {
    if (!store_ || selectedIssue_.isEmpty()) {
        throw std::runtime_error("请先选择一个问题");
    }
    const auto issueId = toUtf8(selectedIssue_, "id");
    const auto issue = store_->findIssue(issueId);
    if (!issue) throw std::runtime_error("问题不存在或已删除");

    QFile templateFile(
        QStringLiteral(":/issuetrace/resources/templates/summaries/default-summary.md"));
    if (!templateFile.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("无法读取内置总结模板");
    }
    issuetrace::SummaryRenderContext context;
    context.issueFields = {
        {"ticket", issue->ticket},
        {"status", templateOptionValue(activeTemplate_.fields, "status", issue->status)},
        {"service", issue->service}, {"version", issue->version},
        {"assignee", issue->assignee}, {"title", issue->title},
        {"reporter", issue->reporter},
        {"original_problem", issue->originalProblem},
        {"conclusion", issue->conclusion}};
    const auto formatTime = [](const std::int64_t value) {
        return QDateTime::fromMSecsSinceEpoch(value)
            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            .toUtf8().toStdString();
    };
    context.issueFields["reported_at"] = formatTime(issue->reportedAt);
    context.issueFields["resolved_at"] =
        issue->resolvedAt ? formatTime(*issue->resolvedAt) : std::string{};
    const auto generatedAt = QDateTime::currentMSecsSinceEpoch();
    context.createdAt = formatTime(generatedAt);
    context.updatedAt = formatTime(generatedAt);

    const auto entries = store_->listTimelineEntries(issueId);
    std::ostringstream timeline;
    std::ostringstream attachments;
    for (auto iterator = entries.rbegin(); iterator != entries.rend(); ++iterator) {
        timeline << "### " << formatTime(iterator->occurredAt) << " · "
                 << timelineTypeLabel(iterator->type).toUtf8().toStdString()
                 << "\n\n" << iterator->contentMarkdown << "\n\n";
        for (const auto& attachment : store_->listAttachments(iterator->id)) {
            const auto filename = std::filesystem::path(attachment.relativePath)
                                      .filename().generic_string();
            const auto directory = attachment.mimeType.starts_with("image/")
                                       ? std::string("图片") : std::string("附件");
            attachments << "- [" << attachment.originalName << "](" << directory
                        << "/" << filename << ")\n";
        }
    }
    context.timelineMarkdown = timeline.str();
    context.attachmentsMarkdown = attachments.str();
    return fromUtf8(issuetrace::renderSummaryMarkdown(
        templateFile.readAll().toStdString(), context));
}

void AppController::exportMarkdown(const QUrl& destination) {
    std::filesystem::path temporary;
    try {
        if (!store_ || selectedIssue_.isEmpty() || !destination.isLocalFile()) {
            throw std::runtime_error("请选择导出目录和问题");
        }
        const auto summary = generateSummaryContent();
        const auto issueId = toUtf8(selectedIssue_, "id");
        const auto issue = store_->findIssue(issueId);
        if (!issue) throw std::runtime_error("问题不存在或已删除");
        const auto folderName = safeExportName(
            (issue->ticket.empty() ? std::string{} : issue->ticket + "-") + issue->title);
        const auto root = nativePath(destination.toLocalFile());
        const auto target = root / folderName;
        if (std::filesystem::exists(target)) {
            throw std::runtime_error("导出目录已存在，请更换目录或先手动处理旧导出");
        }
        temporary = root / (".issuetrace-export-" + issue->id + "-" +
                            std::to_string(QDateTime::currentMSecsSinceEpoch()));
        std::filesystem::create_directories(temporary / "图片");
        std::filesystem::create_directories(temporary / "附件");

        std::ostringstream record;
        record << "# " << issue->title << "\n\n"
               << "- 问题单：" << issue->ticket << "\n"
               << "- 状态："
               << templateOptionValue(activeTemplate_.fields, "status", issue->status)
               << "\n"
               << "- 服务：" << issue->service << "\n"
               << "- 版本：" << issue->version << "\n"
               << "- 提出人：" << issue->reporter << "\n"
               << "- 处理人：" << issue->assignee << "\n\n"
               << "## 问题原话\n\n" << issue->originalProblem << "\n\n"
               << "## 处理记录\n\n";
        const auto entries = store_->listTimelineEntries(issueId);
        for (auto iterator = entries.rbegin(); iterator != entries.rend(); ++iterator) {
            record << "### "
                   << QDateTime::fromMSecsSinceEpoch(iterator->occurredAt)
                          .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                          .toUtf8().toStdString()
                   << " · " << timelineTypeLabel(iterator->type).toUtf8().toStdString()
                   << "\n\n" << iterator->contentMarkdown << "\n\n";
            for (const auto& attachment : store_->listAttachments(iterator->id)) {
                const auto relative = std::filesystem::path(attachment.relativePath);
                const auto category = attachment.mimeType.starts_with("image/")
                                          ? std::filesystem::path("图片")
                                          : std::filesystem::path("附件");
                std::filesystem::copy_file(store_->workspaceRoot() / relative,
                                           temporary / category / relative.filename());
                const auto link = category.generic_string() + "/" +
                                  relative.filename().generic_string();
                if (attachment.mimeType.starts_with("image/")) {
                    record << "![" << attachment.originalName << "](" << link << ")\n\n";
                } else {
                    record << "[" << attachment.originalName << "](" << link << ")\n\n";
                }
            }
        }
        const auto write = [](const std::filesystem::path& path,
                              const std::string& content) {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!output) throw std::runtime_error("Markdown 文件写入失败");
        };
        write(temporary / "问题记录.md", record.str());
        write(temporary / "问题总结.md", summary.toUtf8().toStdString());
        std::filesystem::rename(temporary, target);
        temporary.clear();
        setStatus(QStringLiteral("Markdown 已导出：") +
                  QDir::toNativeSeparators(destination.toLocalFile() +
                                           QStringLiteral("/") + fromUtf8(folderName)));
    } catch (const std::exception& error) {
        if (!temporary.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(temporary, ignored);
        }
        setStatus(QStringLiteral("导出失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::exportXlsx(const QUrl& destination) {
    try {
        if (!store_ || !destination.isLocalFile()) {
            throw std::runtime_error("请选择 XLSX 保存位置");
        }
        auto path = destination.toLocalFile();
        if (!path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive)) {
            path += QStringLiteral(".xlsx");
        }
        if (QFileInfo::exists(path)) {
            throw std::runtime_error("目标文件已存在，请更换名称或先手动处理旧文件");
        }

        QVariantList exportFields;
        issuetrace::XlsxTable table;
        for (const auto& item : activeTemplate_.fields) {
            const auto field = item.toMap();
            if (!field.value(QStringLiteral("xlsxVisible")).toBool()) continue;
            exportFields.push_back(field);
            table.headers.push_back(
                field.value(QStringLiteral("label")).toString().toUtf8().toStdString());
        }
        if (table.headers.empty()) {
            throw std::runtime_error("当前表单模板没有启用任何 XLSX 导出列");
        }

        const auto issues = store_->searchIssues(activeQuery_);
        for (const auto& issue : issues) {
            std::vector<issuetrace::XlsxCell> row;
            row.reserve(static_cast<std::size_t>(exportFields.size()));
            for (const auto& item : exportFields) {
                row.push_back(xlsxCell(item.toMap(), issue, *store_));
            }
            table.rows.push_back(std::move(row));
        }
        const auto bytes = issuetrace::buildXlsx(table);
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly) ||
            output.write(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<qint64>(bytes.size())) !=
                static_cast<qint64>(bytes.size()) ||
            !output.commit()) {
            throw std::runtime_error("XLSX 文件写入失败");
        }
        setStatus(QStringLiteral("已导出当前筛选结果（%1 项）：%2")
                      .arg(issues.size())
                      .arg(QDir::toNativeSeparators(path)));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("导出失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::verifyWorkspace() {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        const auto result = store_->verifyWorkspace();
        if (!result.ok) {
            throw std::runtime_error(result.errors.empty()
                ? "未知完整性错误" : result.errors.front());
        }
        setStatus(QStringLiteral("工作区完整：%1 个问题、%2 条记录、%3 个附件")
                      .arg(result.issueCount)
                      .arg(result.timelineCount)
                      .arg(result.attachmentCount));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("工作区验证失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::backupWorkspace(const QUrl& destination) {
    try {
        if (!store_ || !destination.isLocalFile()) {
            throw std::runtime_error("请选择备份保存位置");
        }
        const auto path = store_->createBackup(nativePath(destination.toLocalFile()));
#ifdef _WIN32
        const auto display = QString::fromStdWString(path.wstring());
#else
        const auto display = QString::fromUtf8(path.string());
#endif
        setStatus(QStringLiteral("工作区备份已创建并验证：") +
                  QDir::toNativeSeparators(display));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("备份失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::restoreWorkspace(const QUrl& backupFolder) {
    const auto originalPath = workspacePath_;
    try {
        if (!store_ || !backupFolder.isLocalFile()) {
            throw std::runtime_error("请选择 IssueTrace 备份目录");
        }
        const auto backup = nativePath(backupFolder.toLocalFile());
        const auto verification = issuetrace::IssueStore::verifyBackup(backup);
        if (!verification.ok) {
            throw std::runtime_error(verification.errors.empty()
                ? "备份验证失败" : verification.errors.front());
        }
        const auto workspace = nativePath(workspacePath_);
        const auto safetyBackup = store_->createBackup(workspace / "backups");
        store_.reset();
        issuetrace::IssueStore::restoreBackup(backup, workspace);
        openWorkspace(originalPath);
#ifdef _WIN32
        const auto safety = QString::fromStdWString(safetyBackup.wstring());
#else
        const auto safety = QString::fromUtf8(safetyBackup.string());
#endif
        setStatus(QStringLiteral("工作区已恢复；恢复前快照保存在：") +
                  QDir::toNativeSeparators(safety));
    } catch (const std::exception& error) {
        if (!store_ && !originalPath.isEmpty()) openWorkspace(originalPath);
        setStatus(QStringLiteral("恢复失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::applyUpdate(const QUrl& archiveFile) {
    try {
        if (!store_ || !archiveFile.isLocalFile()) {
            throw std::runtime_error("请选择完整的 IssueTrace 发布压缩包");
        }
        const auto verification = store_->verifyWorkspace();
        if (!verification.ok) {
            throw std::runtime_error("工作区验证失败，已取消升级：" +
                                     verification.errors.front());
        }
        const auto packageRootPath = portablePackageRoot();
        if (packageRootPath.isEmpty()) {
            throw std::runtime_error(
                "当前是开发构建或便携目录不完整，不能执行目录内升级");
        }
        QDir packageRoot(packageRootPath);
#ifdef _WIN32
        const auto updaterSource = packageRoot.filePath(
            QStringLiteral("IssueTraceUpdater.exe"));
        const auto updaterName = QStringLiteral("IssueTraceUpdater-%1-%2.exe");
#else
        const auto updaterSource = packageRoot.filePath(
            QStringLiteral("bin/IssueTraceUpdater"));
        const auto updaterName = QStringLiteral("IssueTraceUpdater-%1-%2");
#endif
        if (!QFileInfo::exists(updaterSource)) {
            throw std::runtime_error("便携包缺少 IssueTraceUpdater");
        }
        const auto temporaryRoot = QStandardPaths::writableLocation(
            QStandardPaths::TempLocation);
        if (temporaryRoot.isEmpty()) throw std::runtime_error("无法定位临时目录");
        const auto updaterCopy = QDir(temporaryRoot).filePath(
            updaterName.arg(QCoreApplication::applicationPid())
                .arg(QUuid::createUuid().toString(QUuid::Id128).left(8)));
        if (!QFile::copy(updaterSource, updaterCopy)) {
            throw std::runtime_error("无法把升级帮助程序复制到临时目录");
        }
        QFile::setPermissions(updaterCopy, QFileDevice::ReadOwner |
            QFileDevice::WriteOwner | QFileDevice::ExeOwner |
            QFileDevice::ReadGroup | QFileDevice::ExeGroup |
            QFileDevice::ReadOther | QFileDevice::ExeOther);
        const auto safetyBackup = store_->createBackup(
            nativePath(workspacePath_) / "backups");
        store_.reset();
        const auto executableRelative = packageRoot.relativeFilePath(
            QCoreApplication::applicationFilePath());
        const QStringList arguments{
            QStringLiteral("--archive"), archiveFile.toLocalFile(),
            QStringLiteral("--install-root"), packageRoot.absolutePath(),
            QStringLiteral("--executable"), executableRelative,
            QStringLiteral("--wait-pid"),
            QString::number(QCoreApplication::applicationPid())};
        qint64 updaterPid = 0;
        if (!QProcess::startDetached(updaterCopy, arguments, temporaryRoot,
                                     &updaterPid)) {
            store_ = std::make_unique<issuetrace::IssueStore>(nativePath(workspacePath_));
            QFile::remove(updaterCopy);
            throw std::runtime_error("无法启动升级帮助程序");
        }
#ifdef _WIN32
        const auto backupDisplay = QString::fromStdWString(safetyBackup.wstring());
#else
        const auto backupDisplay = QString::fromUtf8(safetyBackup.string());
#endif
        setStatus(QStringLiteral("升级已开始；工作区快照：") +
                  QDir::toNativeSeparators(backupDisplay));
        QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("升级未开始：") + QString::fromUtf8(error.what()));
    }
}

void AppController::openWorkspace(const QString& path) {
    try {
        const auto packageRoot = portablePackageRoot();
        if (!packageRoot.isEmpty() && localPathIsWithin(path, packageRoot)) {
            throw std::runtime_error("工作区不能位于 IssueTrace 便携程序目录中");
        }
        auto replacement = std::make_unique<issuetrace::IssueStore>(nativePath(path));
        store_ = std::move(replacement);
        activateFormTemplate(defaultTemplate_);
        workspacePath_ = QDir::toNativeSeparators(path);
        if (!qEnvironmentVariableIsSet("ISSUETRACE_WORKSPACE")) {
            QSettings{}.setValue(QStringLiteral("workspace/path"), workspacePath_);
        }
        selectedIssue_.clear();
        timeline_.clear();
        emit workspacePathChanged();
        emit selectedIssueChanged();
        emit timelineChanged();
        refreshIssues();
        setStatus(QStringLiteral("工作区已就绪"));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("无法打开工作区：") + QString::fromUtf8(error.what()));
    }
}

void AppController::setStatus(QString value) {
    if (status_ == value) return;
    status_ = std::move(value);
    emit statusChanged();
}

void AppController::setSelected(const issuetrace::StoredIssue& issue) {
    selectedIssue_ = toVariantMap(issue);
    emit selectedIssueChanged();
}

void AppController::refreshTimeline() {
    QVariantList refreshed;
    if (store_ && !selectedIssue_.isEmpty()) {
        for (const auto& entry :
             store_->listTimelineEntries(toUtf8(selectedIssue_, "id"))) {
            QVariantMap value;
            value.insert(QStringLiteral("id"), fromUtf8(entry.id));
            value.insert(QStringLiteral("type"), fromUtf8(entry.type));
            value.insert(QStringLiteral("content"), fromUtf8(entry.contentMarkdown));
            value.insert(QStringLiteral("occurredAt"),
                         QDateTime::fromMSecsSinceEpoch(entry.occurredAt).toString(
                             QStringLiteral("yyyy-MM-dd HH:mm:ss")));
            QVariantList attachments;
            for (const auto& attachment : store_->listAttachments(entry.id)) {
                QVariantMap item;
                item.insert(QStringLiteral("id"), fromUtf8(attachment.id));
                item.insert(QStringLiteral("name"), fromUtf8(attachment.originalName));
                item.insert(QStringLiteral("mimeType"), fromUtf8(attachment.mimeType));
                item.insert(QStringLiteral("size"), attachment.byteSize);
                item.insert(QStringLiteral("sha256"), fromUtf8(attachment.sha256));
                const auto absolute = nativePath(workspacePath_) /
                                      std::filesystem::path(attachment.relativePath);
#ifdef _WIN32
                const auto localPath = QString::fromStdWString(absolute.wstring());
#else
                const auto localPath = QString::fromUtf8(absolute.string());
#endif
                item.insert(QStringLiteral("url"), QUrl::fromLocalFile(localPath));
                attachments.push_back(std::move(item));
            }
            value.insert(QStringLiteral("attachments"), attachments);
            refreshed.push_back(std::move(value));
        }
    }
    timeline_ = std::move(refreshed);
    emit timelineChanged();
}

QVariantMap AppController::toVariantMap(const issuetrace::StoredIssue& issue) const {
    QVariantMap result;
    result.insert(QStringLiteral("id"), fromUtf8(issue.id));
    result.insert(QStringLiteral("title"), fromUtf8(issue.title));
    result.insert(QStringLiteral("originalProblem"), fromUtf8(issue.originalProblem));
    result.insert(QStringLiteral("original_problem"), fromUtf8(issue.originalProblem));
    result.insert(QStringLiteral("reporter"), fromUtf8(issue.reporter));
    result.insert(QStringLiteral("assignee"), fromUtf8(issue.assignee));
    result.insert(QStringLiteral("service"), fromUtf8(issue.service));
    result.insert(QStringLiteral("version"), fromUtf8(issue.version));
    result.insert(QStringLiteral("ticket"), fromUtf8(issue.ticket));
    result.insert(QStringLiteral("status"), fromUtf8(issue.status));
    result.insert(QStringLiteral("priority"), fromUtf8(issue.priority));
    result.insert(QStringLiteral("conclusion"), fromUtf8(issue.conclusion));
    result.insert(QStringLiteral("progress"),
                  store_ ? fromUtf8(store_->currentProgress(issue.id)) : QString{});
    result.insert(QStringLiteral("reportedAt"),
                  QDateTime::fromMSecsSinceEpoch(issue.reportedAt).toString(
                      QStringLiteral("yyyy-MM-dd HH:mm")));
    result.insert(QStringLiteral("reported_at"), result.value(QStringLiteral("reportedAt")));
    result.insert(QStringLiteral("resolved_at"),
                  issue.resolvedAt
                      ? QDateTime::fromMSecsSinceEpoch(*issue.resolvedAt).toString(
                            QStringLiteral("yyyy-MM-dd HH:mm"))
                      : QString{});
    result.insert(QStringLiteral("updatedAt"),
                  QDateTime::fromMSecsSinceEpoch(issue.updatedAt).toString(
                      QStringLiteral("yyyy-MM-dd HH:mm")));
    const auto now = QDateTime::currentMSecsSinceEpoch();
    result.insert(QStringLiteral("ageText"), elapsedText(issue.createdAt, now));
    result.insert(QStringLiteral("statusDurationText"), elapsedText(
        issue.statusChangedAt ? issue.statusChangedAt : issue.updatedAt, now));
    result.insert(QStringLiteral("inactiveText"), elapsedText(issue.updatedAt, now));
    result.insert(QStringLiteral("remindAt"), issue.remindAt
        ? QDateTime::fromMSecsSinceEpoch(*issue.remindAt).toString(
              QStringLiteral("yyyy-MM-dd HH:mm")) : QString{});
    result.insert(QStringLiteral("reminderDue"), issue.remindAt && *issue.remindAt <= now);
    return result;
}
