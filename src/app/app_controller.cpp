#include "app_controller.hpp"
#include "form_template.hpp"
#include "issue_export_service.hpp"

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
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTimeZone>
#include <QTimer>
#include <QUuid>

#include <filesystem>
#include <algorithm>
#include <functional>
#include <limits>
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

QString statusDisplayName(const QString& status) {
    if (status == QStringLiteral("pending")) return QStringLiteral("待处理");
    if (status == QStringLiteral("investigating")) return QStringLiteral("处理中");
    if (status == QStringLiteral("waiting")) return QStringLiteral("等待");
    if (status == QStringLiteral("completed")) return QStringLiteral("完成");
    return status;
}

std::string normalizedTags(QString value) {
    value.replace(QChar(0xff0c), QChar(','));
    QStringList result;
    QSet<QString> seen;
    for (const auto& part : value.split(',', Qt::SkipEmptyParts)) {
        const auto tag = part.trimmed();
        if (!tag.isEmpty() && !seen.contains(tag)) {
            result.push_back(tag);
            seen.insert(tag);
        }
    }
    return result.join(QStringLiteral(",")).toUtf8().toStdString();
}

std::string normalizedGroup(const QString& value) {
    QStringList result;
    for (const auto& part : value.split('/', Qt::SkipEmptyParts)) {
        const auto level = part.trimmed();
        if (!level.isEmpty()) result.push_back(level);
    }
    return result.join(QStringLiteral("/")).toUtf8().toStdString();
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

}  // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
    activeQuery_.sort = "updated_desc";
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
                                     const QString& reporter,
                                     const QString& assignee,
                                     const QString& group,
                                     const QString& tags,
                                     const QString& version,
                                     const QString& service,
                                     const QString& priority,
                                     const QString& originalProblem,
                                     const QString& ticket) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        auto issue = store_->createIssue(
            title.trimmed().toUtf8().toStdString(),
            reporter.trimmed().toUtf8().toStdString());
        issue.assignee = assignee.trimmed().toUtf8().toStdString();
        issue.groupName = normalizedGroup(group);
        issue.tags = normalizedTags(tags);
        issue.version = version.trimmed().toUtf8().toStdString();
        issue.service = service.trimmed().toUtf8().toStdString();
        issue.originalProblem = originalProblem.trimmed().toUtf8().toStdString();
        issue.ticket = ticket.trimmed().toUtf8().toStdString();
        const auto normalizedPriority = priority.trimmed();
        issue.priority = normalizedPriority.isEmpty()
            ? "normal" : normalizedPriority.toUtf8().toStdString();
        store_->updateIssue(issue);
        const auto saved = store_->findIssue(issue.id);
        if (!saved) throw std::runtime_error("事件创建后无法重新读取");
        refreshIssues();
        refreshFieldOptions();
        setSelected(*saved);
        refreshTimeline();
        setStatus(QStringLiteral("事件已创建，可以继续补充详情"));
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
        if (!issue) throw std::runtime_error("事件不存在或已删除");
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
        if (!issue) throw std::runtime_error("事件不存在或已删除");
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
        issue->groupName = normalizedGroup(values.value(QStringLiteral("group_name")).toString());
        issue->tags = normalizedTags(values.value(QStringLiteral("tags")).toString());
        issue->conclusion = toUtf8(values, "conclusion");
        const auto createdText = values.value(QStringLiteral("created_at")).toString().trimmed();
        if (!createdText.isEmpty()) {
            const auto created = QDateTime::fromString(
                createdText, QStringLiteral("yyyy-MM-dd HH:mm"));
            if (!created.isValid()) {
                throw std::runtime_error("创建时间格式应为 yyyy-MM-dd HH:mm");
            }
            if (created > QDateTime::currentDateTime()) {
                throw std::runtime_error("创建时间不能晚于当前时间");
            }
            issue->createdAt = created.toMSecsSinceEpoch();
        }
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
        refreshFieldOptions();
        if (const auto saved = store_->findIssue(id)) setSelected(*saved);
        setStatus(QStringLiteral("修改已保存"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("保存失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::deleteIssue(const QString& id) {
    try {
        if (!store_ || id.trimmed().isEmpty()) {
            throw std::runtime_error("请选择要删除的事件");
        }
        const auto issueId = id.toUtf8().toStdString();
        store_->softDeleteIssue(issueId);
        if (toUtf8(selectedIssue_, "id") == issueId) {
            selectedIssue_.clear();
            timeline_.clear();
            descriptionAttachments_.clear();
            emit selectedIssueChanged();
            emit timelineChanged();
            emit descriptionAttachmentsChanged();
        }
        refreshIssues();
        refreshTrash();
        refreshFieldOptions();
        setStatus(QStringLiteral("事件已删除，数据仍保留在工作区中"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("删除失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::restoreIssue(const QString& id) {
    try {
        if (!store_ || id.trimmed().isEmpty()) {
            throw std::runtime_error("请选择要恢复的事件");
        }
        store_->restoreIssue(id.toUtf8().toStdString());
        refreshIssues();
        refreshTrash();
        refreshFieldOptions();
        setStatus(QStringLiteral("事件已从回收站恢复"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("恢复失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::permanentlyDeleteIssue(const QString& id) {
    try {
        if (!store_ || id.trimmed().isEmpty()) {
            throw std::runtime_error("请选择要永久删除的事件");
        }
        store_->permanentlyDeleteIssue(id.toUtf8().toStdString());
        refreshIssues();
        refreshTrash();
        refreshFieldOptions();
        setStatus(QStringLiteral("事件及其记录和附件已永久删除"));
        return true;
    } catch (const std::exception& error) {
        refreshTrash();
        setStatus(QStringLiteral("永久删除失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::moveIssueGroup(const QString& sourcePath,
                                   const QString& targetPath,
                                   const QString& placement) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        const auto source = QString::fromUtf8(normalizedGroup(sourcePath).c_str());
        const auto target = QString::fromUtf8(normalizedGroup(targetPath).c_str());
        const auto dropPlacement = target.isEmpty() ? QStringLiteral("root")
                                                    : placement;
        if (dropPlacement != QStringLiteral("before") &&
            dropPlacement != QStringLiteral("child") &&
            dropPlacement != QStringLiteral("after") &&
            dropPlacement != QStringLiteral("root")) {
            throw std::invalid_argument("未知的分组拖放位置");
        }
        if (source.isEmpty() || targetPath == QStringLiteral("__default__") ||
            source == target || (!target.isEmpty() && target.startsWith(source + '/'))) {
            throw std::invalid_argument("不能把分组移动到该位置");
        }
        const auto leaf = source.section('/', -1);
        const auto separator = target.lastIndexOf('/');
        const auto parent = dropPlacement == QStringLiteral("child")
            ? target : (separator < 0 ? QString{} : target.left(separator));
        const auto destination = parent.isEmpty() ? leaf : parent + '/' + leaf;

        bool destinationExists = false;
        for (const auto& item : issueGroups_) {
            const auto path = item.toMap().value(QStringLiteral("path")).toString();
            if (path == destination && path != source) {
                destinationExists = true;
                break;
            }
        }
        if (destinationExists) throw std::invalid_argument("目标位置已有同名分组");

        QStringList order;
        for (const auto& item : issueGroups_) {
            const auto path = item.toMap().value(QStringLiteral("path")).toString();
            if (!path.isEmpty() && path != QStringLiteral("__default__")) {
                order.push_back(path);
            }
        }
        QStringList movingSubtree;
        for (const auto& path : order) {
            if (path == source || path.startsWith(source + '/')) {
                movingSubtree.push_back(destination + path.mid(source.size()));
            }
        }
        order.erase(std::remove_if(order.begin(), order.end(),
            [&source](const QString& path) {
                return path == source || path.startsWith(source + '/');
            }), order.end());
        if (movingSubtree.isEmpty()) throw std::runtime_error("待移动分组不存在");

        int insertionIndex = order.size();
        if (!target.isEmpty()) {
            insertionIndex = order.indexOf(target);
            if (insertionIndex < 0) throw std::runtime_error("目标分组不存在");
            if (dropPlacement == QStringLiteral("after") ||
                dropPlacement == QStringLiteral("child")) {
                ++insertionIndex;
                const auto targetPrefix = target + '/';
                while (insertionIndex < order.size() &&
                       order.at(insertionIndex).startsWith(targetPrefix)) {
                    ++insertionIndex;
                }
            }
        }
        if (destination != source) {
            store_->renameIssueGroupPrefix(source.toUtf8().toStdString(),
                                           destination.toUtf8().toStdString());
        }
        for (const auto& path : movingSubtree) {
            order.insert(insertionIndex++, path);
        }
        QJsonArray serialized;
        for (const auto& path : order) serialized.push_back(path);
        store_->setWorkspaceValue("group_order.v1",
            QJsonDocument(serialized).toJson(QJsonDocument::Compact).toStdString());
        if (activeQuery_.groupPath == source.toUtf8().toStdString() ||
            QString::fromUtf8(activeQuery_.groupPath).startsWith(source + '/')) {
            const auto current = QString::fromUtf8(activeQuery_.groupPath);
            activeQuery_.groupPath = (destination + current.mid(source.size()))
                                         .toUtf8().toStdString();
        }
        refreshIssues();
        refreshFieldOptions();
        if (!selectedIssue_.isEmpty()) {
            if (const auto selected = store_->findIssue(toUtf8(selectedIssue_, "id"))) {
                setSelected(*selected);
            }
        }
        emit issueGroupMoved(source, destination);
        setStatus(destination == source
            ? QStringLiteral("分组顺序已更新")
            : QStringLiteral("分组已移动到 %1").arg(destination));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("移动分组失败：") + QString::fromUtf8(error.what()));
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
            throw std::runtime_error("请先选择一个事件");
        }
        static_cast<void>(store_->createTimelineEntry(
            toUtf8(selectedIssue_, "id"), type.toUtf8().toStdString(),
            content.trimmed().toUtf8().toStdString()));
        refreshTimeline();
        refreshIssues();
        refreshSelectedIssue();
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
            throw std::runtime_error("请先选择一个事件");
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
        refreshSelectedIssue();
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
            throw std::runtime_error("请先选择一个事件");
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
        refreshSelectedIssue();
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
                                      const QString& content,
                                      const QString& occurredAt) {
    try {
        if (!store_) throw std::runtime_error("工作区尚未打开");
        const auto occurred = QDateTime::fromString(
            occurredAt.trimmed(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!occurred.isValid()) {
            throw std::runtime_error("记录时间格式应为 yyyy-MM-dd HH:mm:ss");
        }
        if (occurred > QDateTime::currentDateTime()) {
            throw std::runtime_error("记录时间不能晚于当前时间");
        }
        store_->updateTimelineEntry(id.toUtf8().toStdString(),
                                    type.toUtf8().toStdString(),
                                    content.trimmed().toUtf8().toStdString(),
                                    occurred.toMSecsSinceEpoch());
        refreshTimeline();
        refreshIssues();
        refreshSelectedIssue();
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
        refreshSelectedIssue();
        refreshTrash();
        setStatus(QStringLiteral("跟踪记录已删除"));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("删除失败：") + QString::fromUtf8(error.what()));
    }
}

bool AppController::restoreTimelineEntry(const QString& id) {
    try {
        if (!store_ || id.trimmed().isEmpty()) {
            throw std::runtime_error("请选择要恢复的记录");
        }
        store_->restoreTimelineEntry(id.toUtf8().toStdString());
        refreshIssues();
        refreshTrash();
        refreshTimeline();
        refreshSelectedIssue();
        setStatus(QStringLiteral("事件记录已从回收站恢复"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("恢复失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::permanentlyDeleteTimelineEntry(const QString& id) {
    try {
        if (!store_ || id.trimmed().isEmpty()) {
            throw std::runtime_error("请选择要永久删除的记录");
        }
        store_->permanentlyDeleteTimelineEntry(id.toUtf8().toStdString());
        refreshIssues();
        refreshTrash();
        refreshTimeline();
        refreshSelectedIssue();
        setStatus(QStringLiteral("事件记录及其附件已永久删除"));
        return true;
    } catch (const std::exception& error) {
        refreshTrash();
        setStatus(QStringLiteral("永久删除失败：") + QString::fromUtf8(error.what()));
        return false;
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
        refreshSelectedIssue();
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
        refreshSelectedIssue();
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
        refreshDescriptionAttachments();
        refreshIssues();
        setStatus(QStringLiteral("附件已从事件中隐藏，原文件仍保留"));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("删除附件失败：") + QString::fromUtf8(error.what()));
    }
}

bool AppController::addDescriptionClipboardImage() {
    std::string entryId;
    try {
        if (!store_ || selectedIssue_.isEmpty()) throw std::runtime_error("请先选择一个事件");
        const auto image = QGuiApplication::clipboard()->image();
        if (image.isNull()) throw std::runtime_error("剪贴板中没有图片");
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
            throw std::runtime_error("无法编码剪贴板图片");
        const auto entry = store_->createTimelineEntry(toUtf8(selectedIssue_, "id"),
            "_description_attachment", "事件描述图片");
        entryId = entry.id;
        const auto name = QStringLiteral("事件描述-%1.png").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
        static_cast<void>(store_->addAttachment(entry.id, name.toUtf8().toStdString(),
            "image/png", hash.toStdString(),
            {reinterpret_cast<const unsigned char*>(bytes.constData()), static_cast<std::size_t>(bytes.size())}));
        refreshDescriptionAttachments();
        refreshIssues();
        setStatus(QStringLiteral("图片已加入事件描述"));
        return true;
    } catch (const std::exception& error) {
        if (store_ && !entryId.empty()) {
            try { store_->softDeleteTimelineEntry(entryId); } catch (...) {}
        }
        setStatus(QStringLiteral("添加描述图片失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::addDescriptionImage(const QUrl& sourceFile) {
    std::string entryId;
    try {
        if (!store_ || selectedIssue_.isEmpty() || !sourceFile.isLocalFile())
            throw std::runtime_error("请选择本地图片");
        QFile file(sourceFile.toLocalFile());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 100LL * 1024 * 1024)
            throw std::runtime_error("无法读取图片或图片超过 100 MB");
        const auto mime = QMimeDatabase{}.mimeTypeForFile(file.fileName()).name();
        if (!mime.startsWith(QStringLiteral("image/"))) throw std::runtime_error("文件不是图片");
        const auto bytes = file.readAll();
        const auto entry = store_->createTimelineEntry(toUtf8(selectedIssue_, "id"),
            "_description_attachment", "事件描述图片");
        entryId = entry.id;
        const auto hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
        static_cast<void>(store_->addAttachment(entry.id, QFileInfo(file).fileName().toUtf8().toStdString(),
            mime.toUtf8().toStdString(), hash.toStdString(),
            {reinterpret_cast<const unsigned char*>(bytes.constData()), static_cast<std::size_t>(bytes.size())}));
        refreshDescriptionAttachments();
        refreshIssues();
        setStatus(QStringLiteral("图片已加入事件描述"));
        return true;
    } catch (const std::exception& error) {
        if (store_ && !entryId.empty()) {
            try { store_->softDeleteTimelineEntry(entryId); } catch (...) {}
        }
        setStatus(QStringLiteral("添加描述图片失败：") + QString::fromUtf8(error.what()));
        return false;
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
        QVariantList calendar;
        for (const auto& issue : store_->listIssues()) {
            calendar.push_back(QVariantMap{
                {QStringLiteral("id"), fromUtf8(issue.id)},
                {QStringLiteral("title"), fromUtf8(issue.title)},
                {QStringLiteral("status"), fromUtf8(issue.status)},
                {QStringLiteral("group_name"), fromUtf8(issue.groupName)},
                {QStringLiteral("reportedAt"),
                 QDateTime::fromMSecsSinceEpoch(issue.reportedAt).toString(
                     QStringLiteral("yyyy-MM-dd HH:mm"))},
                {QStringLiteral("reportedAtMs"), issue.reportedAt},
                {QStringLiteral("createdAt"),
                 QDateTime::fromMSecsSinceEpoch(issue.createdAt).toString(
                     QStringLiteral("yyyy-MM-dd HH:mm"))},
                {QStringLiteral("createdAtMs"), issue.createdAt},
                {QStringLiteral("resolvedAt"), issue.resolvedAt
                    ? QDateTime::fromMSecsSinceEpoch(*issue.resolvedAt).toString(
                          QStringLiteral("yyyy-MM-dd HH:mm"))
                    : QString{}},
                {QStringLiteral("resolvedAtMs"),
                 issue.resolvedAt ? QVariant::fromValue(*issue.resolvedAt) : QVariant{}}});
        }
        calendarIssues_ = std::move(calendar);
        emit calendarIssuesChanged();
        refreshEditorIssues();
        refreshIssueGroups();
        refreshAttention();
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("读取失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::refreshTrash() {
    QVariantList deletedIssues;
    QVariantList deletedEntries;
    if (store_) {
        try {
            QHash<QString, QString> issueTitles;
            for (const auto& issue : store_->listIssues()) {
                issueTitles.insert(fromUtf8(issue.id), fromUtf8(issue.title));
            }
            for (const auto& issue : store_->listDeletedIssues()) {
                issueTitles.insert(fromUtf8(issue.id), fromUtf8(issue.title));
                deletedIssues.push_back(toVariantMap(issue));
            }
            for (const auto& entry : store_->listDeletedTimelineEntries()) {
                const auto issueId = fromUtf8(entry.issueId);
                deletedEntries.push_back(QVariantMap{
                    {QStringLiteral("id"), fromUtf8(entry.id)},
                    {QStringLiteral("issueId"), issueId},
                    {QStringLiteral("issueTitle"), issueTitles.value(issueId)},
                    {QStringLiteral("type"), fromUtf8(entry.type)},
                    {QStringLiteral("content"), fromUtf8(entry.contentMarkdown)},
                    {QStringLiteral("occurredAt"),
                     QDateTime::fromMSecsSinceEpoch(entry.occurredAt).toString(
                         QStringLiteral("yyyy-MM-dd HH:mm:ss"))},
                    {QStringLiteral("deletedAt"), entry.deletedAt
                         ? QDateTime::fromMSecsSinceEpoch(*entry.deletedAt).toString(
                               QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                         : QString{}}});
            }
        } catch (const std::exception& error) {
            setStatus(QStringLiteral("读取回收站失败：") + QString::fromUtf8(error.what()));
        }
    }
    trashIssues_ = std::move(deletedIssues);
    trashTimelineEntries_ = std::move(deletedEntries);
    emit trashChanged();
}

void AppController::refreshEditorIssues() {
    if (!store_) return;
    QVariantList refreshed;
    for (const auto& issue : store_->searchIssues(editorQuery_)) {
        refreshed.push_back(toVariantMap(issue));
    }
    editorIssues_ = std::move(refreshed);
    emit editorIssuesChanged();
}

void AppController::refreshIssueGroups() {
    if (!store_) return;
    const auto all = store_->listIssues();
    QSet<QString> paths;
    int defaultCount = 0;
    for (const auto& issue : all) {
        const auto fullPath = fromUtf8(issue.groupName);
        if (fullPath.isEmpty()) {
            ++defaultCount;
            continue;
        }
        QString current;
        for (const auto& level : fullPath.split('/', Qt::SkipEmptyParts)) {
            current = current.isEmpty() ? level : current + QStringLiteral("/") + level;
            paths.insert(current);
        }
    }
    QStringList savedOrder;
    if (const auto saved = store_->workspaceValue("group_order.v1")) {
        const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(*saved));
        if (document.isArray()) {
            for (const auto& value : document.array()) savedOrder.push_back(value.toString());
        }
    }
    auto rank = [&savedOrder](const QString& path) {
        const auto index = savedOrder.indexOf(path);
        return index < 0 ? std::numeric_limits<int>::max() : index;
    };
    QStringList ordered;
    std::function<void(const QString&)> appendChildren = [&](const QString& parent) {
        QStringList children;
        for (const auto& candidate : paths) {
            const auto separator = candidate.lastIndexOf('/');
            const auto candidateParent = separator < 0 ? QString{} : candidate.left(separator);
            if (candidateParent == parent) children.push_back(candidate);
        }
        std::sort(children.begin(), children.end(), [&](const QString& left,
                                                        const QString& right) {
            const auto leftRank = rank(left);
            const auto rightRank = rank(right);
            if (leftRank != rightRank) return leftRank < rightRank;
            return QString::localeAwareCompare(left, right) < 0;
        });
        for (const auto& child : children) {
            ordered.push_back(child);
            appendChildren(child);
        }
    };
    appendChildren({});
    QVariantList groups;
    groups.push_back(QVariantMap{{QStringLiteral("path"), QString{}},
        {QStringLiteral("label"), QStringLiteral("全部事件")},
        {QStringLiteral("depth"), 0},
        {QStringLiteral("count"), static_cast<qlonglong>(all.size())},
        {QStringLiteral("hasChildren"), false},
        {QStringLiteral("draggable"), false}});
    groups.push_back(QVariantMap{{QStringLiteral("path"), QStringLiteral("__default__")},
        {QStringLiteral("label"), QStringLiteral("默认分组")},
        {QStringLiteral("depth"), 0}, {QStringLiteral("count"), defaultCount},
        {QStringLiteral("hasChildren"), false},
        {QStringLiteral("draggable"), false}});
    for (const auto& path : ordered) {
        const auto prefix = path + QStringLiteral("/");
        int count = 0;
        bool hasChildren = false;
        for (const auto& issue : all) {
            const auto issuePath = fromUtf8(issue.groupName);
            if (issuePath == path || issuePath.startsWith(prefix)) ++count;
        }
        for (const auto& candidate : ordered) {
            if (candidate.startsWith(prefix)) {
                hasChildren = true;
                break;
            }
        }
        groups.push_back(QVariantMap{{QStringLiteral("path"), path},
            {QStringLiteral("label"), path.section('/', -1)},
            {QStringLiteral("depth"), path.count('/')},
            {QStringLiteral("count"), count},
            {QStringLiteral("hasChildren"), hasChildren},
            {QStringLiteral("draggable"), true}});
    }
    issueGroups_ = std::move(groups);
    emit issueGroupsChanged();
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
    try {
        if (!store_ || selectedIssue_.isEmpty()) {
            throw std::runtime_error("请先选择一个事件");
        }
        static const QSet<QString> allowedStatuses{
            QStringLiteral("pending"), QStringLiteral("investigating"),
            QStringLiteral("waiting"), QStringLiteral("completed")};
        if (!allowedStatuses.contains(status)) {
            throw std::runtime_error("事件状态无效");
        }

        const auto id = toUtf8(selectedIssue_, "id");
        auto issue = store_->findIssue(id);
        if (!issue) throw std::runtime_error("事件不存在或已删除");
        const auto nextStatus = status.toUtf8().toStdString();
        if (issue->status != nextStatus) {
            const auto wasCompleted = issue->status == "completed";
            issue->status = nextStatus;
            issue->statusChangedAt = QDateTime::currentMSecsSinceEpoch();
            if (issue->status == "completed") {
                if (!wasCompleted && !issue->resolvedAt) {
                    issue->resolvedAt = issue->statusChangedAt;
                }
                issue->remindAt.reset();
                if (issue->timerStartedAt) {
                    store_->pauseIssueTimer(id);
                    issue = store_->findIssue(id);
                    if (!issue) throw std::runtime_error("事件不存在");
                    issue->status = nextStatus;
                    issue->statusChangedAt = QDateTime::currentMSecsSinceEpoch();
                    if (!issue->resolvedAt) issue->resolvedAt = issue->statusChangedAt;
                    issue->remindAt.reset();
                }
            }
            store_->updateIssue(*issue);
        }

        const auto saved = store_->findIssue(id);
        if (!saved) throw std::runtime_error("状态保存后无法重新读取事件");
        setSelected(*saved);
        refreshIssues();
        setStatus(QStringLiteral("事件状态已更新为：") + statusDisplayName(status));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("状态更新失败：") + QString::fromUtf8(error.what()));
        // A checkable QML button changes its local state before invoking this
        // method. Re-notify the unchanged projection so a failed write rolls
        // the button group back to the persisted status immediately.
        emit selectedIssueChanged();
        return false;
    }
}

bool AppController::remindSelectedIssueIn(const int minutes) {
    try {
        if (!store_ || selectedIssue_.isEmpty() || minutes <= 0) {
            throw std::runtime_error("提醒时间无效");
        }
        const auto id = toUtf8(selectedIssue_, "id");
        auto issue = store_->findIssue(id);
        if (!issue) throw std::runtime_error("事件不存在");
        const auto remindAt = QDateTime::currentMSecsSinceEpoch() +
                              static_cast<qint64>(minutes) * 60000;
        store_->setIssueReminder(id, remindAt);
        notifiedReminders_.clear();
        const auto saved = store_->findIssue(id);
        if (!saved) throw std::runtime_error("提醒保存后无法重新读取事件");
        setSelected(*saved);
        refreshIssues();
        setStatus(QStringLiteral("提醒已设置：%1")
                      .arg(QDateTime::fromMSecsSinceEpoch(remindAt)
                               .toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("设置提醒失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::remindSelectedIssueAt(const QString& localDateTime) {
    try {
        if (!store_ || selectedIssue_.isEmpty()) {
            throw std::runtime_error("请先选择一个事件");
        }
        const auto value = localDateTime.trimmed();
        const auto format = QStringLiteral("yyyy-MM-dd HH:mm");
        const auto parsed = QDateTime::fromString(value, format);
        if (!parsed.isValid() || parsed.toString(format) != value) {
            throw std::runtime_error("请输入 yyyy-MM-dd HH:mm 格式的时间");
        }
        const QDateTime target(parsed.date(), parsed.time(),
                               QTimeZone::systemTimeZone());
        if (!target.isValid()) throw std::runtime_error("该本地时间不存在");
        const auto remindAt = target.toMSecsSinceEpoch();
        if (remindAt <= QDateTime::currentMSecsSinceEpoch()) {
            throw std::runtime_error("提醒时间必须晚于当前时间");
        }

        const auto id = toUtf8(selectedIssue_, "id");
        if (!store_->findIssue(id)) throw std::runtime_error("事件不存在");
        store_->setIssueReminder(id, remindAt);
        notifiedReminders_.clear();
        const auto saved = store_->findIssue(id);
        if (!saved) throw std::runtime_error("提醒保存后无法重新读取事件");
        setSelected(*saved);
        refreshIssues();
        setStatus(QStringLiteral("提醒已设置：%1").arg(target.toString(format)));
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
        if (!issue) throw std::runtime_error("事件不存在");
        store_->setIssueReminder(id, std::nullopt);
        notifiedReminders_.clear();
        const auto saved = store_->findIssue(id);
        if (!saved) throw std::runtime_error("提醒取消后无法重新读取事件");
        setSelected(*saved);
        refreshIssues();
        setStatus(QStringLiteral("提醒已取消"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("取消提醒失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::startSelectedIssueTimer() {
    try {
        if (!store_ || selectedIssue_.isEmpty()) throw std::runtime_error("请先选择一个事件");
        const auto id = toUtf8(selectedIssue_, "id");
        store_->startIssueTimer(id);
        if (const auto issue = store_->findIssue(id)) setSelected(*issue);
        refreshIssues();
        setStatus(QStringLiteral("事件计时已开始"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("开始计时失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::pauseSelectedIssueTimer() {
    try {
        if (!store_ || selectedIssue_.isEmpty()) throw std::runtime_error("请先选择一个事件");
        const auto id = toUtf8(selectedIssue_, "id");
        store_->pauseIssueTimer(id);
        if (const auto issue = store_->findIssue(id)) setSelected(*issue);
        refreshIssues();
        setStatus(QStringLiteral("事件计时已暂停"));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("暂停计时失败：") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::setSelectedIssueTrackedDuration(const int hours,
                                                    const int minutes) {
    try {
        if (!store_ || selectedIssue_.isEmpty()) {
            throw std::runtime_error("请先选择一个事件");
        }
        if (hours < 0 || minutes < 0 || minutes > 59) {
            throw std::runtime_error("请输入有效的小时和分钟");
        }
        const auto id = toUtf8(selectedIssue_, "id");
        const auto issue = store_->findIssue(id);
        if (!issue) throw std::runtime_error("事件不存在");
        if (issue->timerStartedAt) {
            throw std::runtime_error("请先暂停计时再修改累计处理时间");
        }
        constexpr std::int64_t millisecondsPerMinute = 60LL * 1000LL;
        const auto totalMinutes = static_cast<std::int64_t>(hours) * 60LL + minutes;
        store_->setIssueTrackedMilliseconds(id, totalMinutes * millisecondsPerMinute);
        const auto saved = store_->findIssue(id);
        if (!saved) throw std::runtime_error("计时数据保存后无法重新读取事件");
        setSelected(*saved);
        refreshIssues();
        setStatus(QStringLiteral("累计处理时间已更新为 %1 小时 %2 分钟")
                      .arg(hours)
                      .arg(minutes));
        return true;
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("修改计时数据失败：") + QString::fromUtf8(error.what()));
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
                                 int staleDays, const QString& priority,
                                 const QString& tag, const QString& progress,
                                 int minimumTrackedMinutes,
                                 int maximumTrackedMinutes,
                                 const QString& groupPath,
                                 const QString& version,
                                 const QString& ticket) {
    activeQuery_.text = text.trimmed().toUtf8().toStdString();
    activeQuery_.titleText.clear();
    activeQuery_.status = status.toUtf8().toStdString();
    activeQuery_.service = service.trimmed().toUtf8().toStdString();
    activeQuery_.assignee = assignee.trimmed().toUtf8().toStdString();
    activeQuery_.sort = sort.isEmpty() ? "updated_desc" : sort.toUtf8().toStdString();
    activeQuery_.staleDays = staleDays;
    activeQuery_.priority = priority.toUtf8().toStdString();
    activeQuery_.tag = tag.trimmed().toUtf8().toStdString();
    activeQuery_.progress = progress.trimmed().toUtf8().toStdString();
    activeQuery_.minimumTrackedMinutes = std::max(0, minimumTrackedMinutes);
    activeQuery_.maximumTrackedMinutes = std::max(0, maximumTrackedMinutes);
    activeQuery_.groupPath = groupPath.toUtf8().toStdString();
    activeQuery_.version = version.trimmed().toUtf8().toStdString();
    activeQuery_.ticket = ticket.trimmed().toUtf8().toStdString();
    refreshIssues();
    setStatus(QStringLiteral("已找到 %1 个事件").arg(issues_.size()));
}

void AppController::filterEditorIssues(const QString& priority,
                                       const QString& sort,
                                       const QString& text) {
    editorQuery_ = {};
    editorQuery_.priority = priority.toUtf8().toStdString();
    editorQuery_.sort = sort.isEmpty() ? "updated_desc" : sort.toUtf8().toStdString();
    editorQuery_.text = text.trimmed().toUtf8().toStdString();
    refreshEditorIssues();
}

void AppController::exportMarkdown(const QUrl& destination) {
    try {
        if (!store_ || selectedIssue_.isEmpty() || !destination.isLocalFile()) {
            throw std::runtime_error("请选择导出目录和事件");
        }
        const auto issueId = toUtf8(selectedIssue_, "id");
        const auto issue = store_->findIssue(issueId);
        if (!issue) throw std::runtime_error("事件不存在或已删除");
        QFile templateFile(
            QStringLiteral(":/issuetrace/resources/templates/summaries/default-summary.md"));
        if (!templateFile.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("无法读取内置总结模板");
        }
        const auto target = IssueExportService::exportMarkdown(
            *store_, *issue, activeTemplate_, templateFile.readAll(),
            nativePath(destination.toLocalFile()));
#ifdef _WIN32
        const auto display = QString::fromStdWString(target.wstring());
#else
        const auto display = QString::fromUtf8(target.string());
#endif
        setStatus(QStringLiteral("Markdown 已导出：") + QDir::toNativeSeparators(display));
    } catch (const std::exception& error) {
        setStatus(QStringLiteral("导出失败：") + QString::fromUtf8(error.what()));
    }
}

void AppController::exportXlsx(const QUrl& destination) {
    try {
        if (!store_ || !destination.isLocalFile()) {
            throw std::runtime_error("请选择 XLSX 保存位置");
        }
        const auto result = IssueExportService::exportXlsx(
            *store_, activeQuery_, activeTemplate_, nativePath(destination.toLocalFile()));
#ifdef _WIN32
        const auto display = QString::fromStdWString(result.path.wstring());
#else
        const auto display = QString::fromUtf8(result.path.string());
#endif
        setStatus(QStringLiteral("已导出当前筛选结果（%1 项）：%2")
                      .arg(result.issueCount)
                      .arg(QDir::toNativeSeparators(display)));
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
        setStatus(QStringLiteral("工作区完整：%1 个事件、%2 条记录、%3 个附件")
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
        descriptionAttachments_.clear();
        editorIssues_.clear();
        issueGroups_.clear();
        trashIssues_.clear();
        trashTimelineEntries_.clear();
        emit workspacePathChanged();
        emit selectedIssueChanged();
        emit timelineChanged();
        emit descriptionAttachmentsChanged();
        emit editorIssuesChanged();
        emit issueGroupsChanged();
        emit trashChanged();
        refreshIssues();
        refreshTrash();
        refreshFieldOptions();
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
    refreshDescriptionAttachments();
}

void AppController::refreshSelectedIssue() {
    if (!store_ || selectedIssue_.isEmpty()) return;
    const auto id = toUtf8(selectedIssue_, "id");
    if (const auto issue = store_->findIssue(id)) setSelected(*issue);
}

void AppController::refreshDescriptionAttachments() {
    QVariantList values;
    if (store_ && !selectedIssue_.isEmpty()) {
        for (const auto& attachment : store_->listDescriptionAttachments(toUtf8(selectedIssue_, "id"))) {
            const auto absolute = nativePath(workspacePath_) / std::filesystem::path(attachment.relativePath);
#ifdef _WIN32
            const auto localPath = QString::fromStdWString(absolute.wstring());
#else
            const auto localPath = QString::fromUtf8(absolute.string());
#endif
            values.push_back(QVariantMap{{QStringLiteral("id"), fromUtf8(attachment.id)},
                {QStringLiteral("name"), fromUtf8(attachment.originalName)},
                {QStringLiteral("url"), QUrl::fromLocalFile(localPath)}});
        }
    }
    descriptionAttachments_ = std::move(values);
    emit descriptionAttachmentsChanged();
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
            value.insert(QStringLiteral("occurredAtMs"), entry.occurredAt);
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
    result.insert(QStringLiteral("group_name"), fromUtf8(issue.groupName));
    result.insert(QStringLiteral("groupDisplay"), issue.groupName.empty()
        ? QStringLiteral("默认分组") : fromUtf8(issue.groupName));
    result.insert(QStringLiteral("tags"), fromUtf8(issue.tags));
    result.insert(QStringLiteral("conclusion"), fromUtf8(issue.conclusion));
    result.insert(QStringLiteral("progress"),
                  store_ ? fromUtf8(store_->currentProgress(issue.id)) : QString{});
    result.insert(QStringLiteral("reportedAt"),
                  QDateTime::fromMSecsSinceEpoch(issue.reportedAt).toString(
                      QStringLiteral("yyyy-MM-dd HH:mm")));
    result.insert(QStringLiteral("reported_at"), result.value(QStringLiteral("reportedAt")));
    result.insert(QStringLiteral("reportedAtMs"), issue.reportedAt);
    result.insert(QStringLiteral("resolved_at"),
                  issue.resolvedAt
                      ? QDateTime::fromMSecsSinceEpoch(*issue.resolvedAt).toString(
                            QStringLiteral("yyyy-MM-dd HH:mm"))
                      : QString{});
    result.insert(QStringLiteral("updatedAt"),
                  QDateTime::fromMSecsSinceEpoch(issue.updatedAt).toString(
                      QStringLiteral("yyyy-MM-dd HH:mm")));
    result.insert(QStringLiteral("createdAt"),
                  QDateTime::fromMSecsSinceEpoch(issue.createdAt).toString(
                      QStringLiteral("yyyy-MM-dd HH:mm")));
    result.insert(QStringLiteral("created_at"), result.value(QStringLiteral("createdAt")));
    result.insert(QStringLiteral("createdAtMs"), issue.createdAt);
    result.insert(QStringLiteral("updatedAtMs"), issue.updatedAt);
    const auto now = QDateTime::currentMSecsSinceEpoch();
    result.insert(QStringLiteral("ageText"), elapsedText(issue.createdAt, now));
    result.insert(QStringLiteral("statusDurationText"), elapsedText(
        issue.statusChangedAt ? issue.statusChangedAt : issue.updatedAt, now));
    result.insert(QStringLiteral("inactiveText"), elapsedText(issue.updatedAt, now));
    result.insert(QStringLiteral("remindAt"), issue.remindAt
        ? QDateTime::fromMSecsSinceEpoch(*issue.remindAt).toString(
              QStringLiteral("yyyy-MM-dd HH:mm")) : QString{});
    result.insert(QStringLiteral("reminderDue"), issue.remindAt && *issue.remindAt <= now);
    result.insert(QStringLiteral("trackedMilliseconds"), issue.trackedMilliseconds);
    result.insert(QStringLiteral("timerRunning"), issue.timerStartedAt.has_value());
    result.insert(QStringLiteral("timerStartedAtMs"), issue.timerStartedAt.value_or(0));
    result.insert(QStringLiteral("trackedTotalMilliseconds"),
        issue.trackedMilliseconds + (issue.timerStartedAt
            ? std::max<std::int64_t>(0, now - *issue.timerStartedAt) : 0));
    result.insert(QStringLiteral("deletedAt"), issue.deletedAt
        ? QDateTime::fromMSecsSinceEpoch(*issue.deletedAt).toString(
              QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString{});
    return result;
}

void AppController::refreshFieldOptions() {
    if (!store_) return;
    serviceOptions_.clear();
    versionOptions_.clear();
    reporterOptions_.clear();
    assigneeOptions_.clear();
    groupOptions_.clear();
    for (const auto& value : store_->distinctServices()) serviceOptions_.push_back(fromUtf8(value));
    for (const auto& value : store_->distinctVersions()) versionOptions_.push_back(fromUtf8(value));
    for (const auto& value : store_->distinctReporters()) reporterOptions_.push_back(fromUtf8(value));
    for (const auto& value : store_->distinctAssignees()) assigneeOptions_.push_back(fromUtf8(value));
    for (const auto& value : store_->distinctGroups()) groupOptions_.push_back(fromUtf8(value));
    emit fieldOptionsChanged();
}
