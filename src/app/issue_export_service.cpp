#include "issue_export_service.hpp"

#include "issuetrace/summary_renderer.hpp"
#include "issuetrace/xlsx_exporter.hpp"

#include <QDateTime>
#include <QHash>
#include <QSaveFile>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

QString fromUtf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString fromNativePath(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromUtf8(path.string());
#endif
}

QString timelineTypeLabel(const std::string& type) {
    static const QHash<QString, QString> labels{
        {QStringLiteral("note"), QStringLiteral("随笔")},
        {QStringLiteral("progress"), QStringLiteral("进展")},
        {QStringLiteral("data"), QStringLiteral("数据排查")},
        {QStringLiteral("log"), QStringLiteral("日志")},
        {QStringLiteral("code"), QStringLiteral("代码梳理")},
        {QStringLiteral("solution"), QStringLiteral("解决方案")},
        {QStringLiteral("verification"), QStringLiteral("验证")},
        {QStringLiteral("conclusion"), QStringLiteral("结论")}};
    const auto key = fromUtf8(type);
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
        static const QHash<QString, QString> fallbackLabels{
            {QStringLiteral("pending"), QStringLiteral("待处理")},
            {QStringLiteral("investigating"), QStringLiteral("处理中")},
            {QStringLiteral("waiting"), QStringLiteral("等待中")},
            {QStringLiteral("resolved"), QStringLiteral("已解决")},
            {QStringLiteral("closed"), QStringLiteral("已关闭")}};
        const auto key = fromUtf8(raw);
        if (fallbackLabels.contains(key)) {
            return fallbackLabels.value(key).toUtf8().toStdString();
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
    if (id == QStringLiteral("group_name")) return {issue.groupName, {}};
    if (id == QStringLiteral("tags")) return {issue.tags, {}};
    if (id == QStringLiteral("progress")) return {store.currentProgress(issue.id), {}};
    if (id == QStringLiteral("conclusion")) return {issue.conclusion, {}};
    return {};
}

std::string safeExportName(std::string value) {
    for (auto& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 32 || character == '/' || character == '\\' || character == ':' ||
            character == '*' || character == '?' || character == '"' || character == '<' ||
            character == '>' || character == '|') {
            character = '_';
        }
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '.')) value.pop_back();
    return value.empty() ? std::string("IssueTrace-Export") : value;
}

std::string formatTime(const std::int64_t value) {
    return QDateTime::fromMSecsSinceEpoch(value)
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        .toUtf8().toStdString();
}

std::string renderSummary(issuetrace::IssueStore&,
                          const issuetrace::StoredIssue& issue,
                          const FormTemplateDefinition& form,
                          const QByteArray& summaryTemplate) {
    issuetrace::SummaryRenderContext context;
    context.issueFields = {
        {"ticket", issue.ticket},
        {"status", templateOptionValue(form.fields, "status", issue.status)},
        {"service", issue.service}, {"version", issue.version},
        {"assignee", issue.assignee}, {"title", issue.title},
        {"reporter", issue.reporter},
        {"original_problem", issue.originalProblem},
        {"conclusion", issue.conclusion}};
    context.issueFields["reported_at"] = formatTime(issue.reportedAt);
    context.issueFields["resolved_at"] =
        issue.resolvedAt ? formatTime(*issue.resolvedAt) : std::string{};
    const auto generatedAt = QDateTime::currentMSecsSinceEpoch();
    context.createdAt = formatTime(generatedAt);
    context.updatedAt = formatTime(generatedAt);

    context.timelineMarkdown.clear();
    context.attachmentsMarkdown.clear();
    return issuetrace::renderSummaryMarkdown(summaryTemplate.toStdString(), context);
}

void writeText(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output) throw std::runtime_error("Markdown 文件写入失败");
}

}  // namespace

std::filesystem::path IssueExportService::exportMarkdown(
    issuetrace::IssueStore& store,
    const issuetrace::StoredIssue& issue,
    const FormTemplateDefinition& form,
    const QByteArray& summaryTemplate,
    const std::filesystem::path& destinationRoot) {
    if (summaryTemplate.isEmpty()) throw std::runtime_error("总结模板为空");
    const auto creationPrefix = QDateTime::fromMSecsSinceEpoch(issue.createdAt)
        .toString(QStringLiteral("yyyyMMddHH")).toUtf8().toStdString();
    const auto folderName = safeExportName(creationPrefix + " " +
        (issue.ticket.empty() ? std::string{} : issue.ticket + "-") + issue.title);
    const auto target = destinationRoot / folderName;
    if (std::filesystem::exists(target)) {
        throw std::runtime_error("导出目录已存在，请更换目录或先手动处理旧导出");
    }
    const auto temporary = destinationRoot / (".issuetrace-export-" + issue.id + "-" +
        std::to_string(QDateTime::currentMSecsSinceEpoch()));
    try {
        std::filesystem::create_directories(temporary / "图片");
        std::filesystem::create_directories(temporary / "附件");

        std::ostringstream record;
        record << "# " << issue.title << "\n\n"
               << "- 跟踪单：" << issue.ticket << "\n"
               << "- 状态：" << templateOptionValue(form.fields, "status", issue.status) << "\n"
               << "- 服务：" << issue.service << "\n"
               << "- 版本：" << issue.version << "\n"
               << "- 提出人：" << issue.reporter << "\n"
               << "- 处理人：" << issue.assignee << "\n\n"
               << "- 分组：" << issue.groupName << "\n"
               << "- 标签：" << issue.tags << "\n\n"
               << "## 事件描述\n\n" << issue.originalProblem << "\n\n";
        for (const auto& attachment : store.listDescriptionAttachments(issue.id)) {
            const auto relative = std::filesystem::path(attachment.relativePath);
            std::filesystem::copy_file(store.workspaceRoot() / relative,
                                       temporary / "图片" / relative.filename());
            record << "![" << attachment.originalName << "](图片/"
                   << relative.filename().generic_string() << ")\n\n";
        }
        record << "## 事件记录\n\n";
        const auto entries = store.listTimelineEntries(issue.id);
        for (auto iterator = entries.rbegin(); iterator != entries.rend(); ++iterator) {
            record << "### " << formatTime(iterator->occurredAt) << " · "
                   << timelineTypeLabel(iterator->type).toUtf8().toStdString()
                   << "\n\n" << iterator->contentMarkdown << "\n\n";
            for (const auto& attachment : store.listAttachments(iterator->id)) {
                const auto relative = std::filesystem::path(attachment.relativePath);
                const auto category = attachment.mimeType.starts_with("image/")
                    ? std::filesystem::path("图片") : std::filesystem::path("附件");
                std::filesystem::copy_file(store.workspaceRoot() / relative,
                                           temporary / category / relative.filename());
                const auto link = category.generic_string() + "/" +
                                  relative.filename().generic_string();
                record << (attachment.mimeType.starts_with("image/") ? "![" : "[")
                       << attachment.originalName << "](" << link << ")\n\n";
            }
        }
        writeText(temporary / "事件记录.md", record.str());
        writeText(temporary / "事件总结.md",
                  renderSummary(store, issue, form, summaryTemplate));
        std::filesystem::rename(temporary, target);
        return target;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(temporary, ignored);
        throw;
    }
}

XlsxExportResult IssueExportService::exportXlsx(
    issuetrace::IssueStore& store,
    const issuetrace::IssueQuery& query,
    const FormTemplateDefinition& form,
    std::filesystem::path destination) {
    if (!fromNativePath(destination).endsWith(QStringLiteral(".xlsx"),
                                               Qt::CaseInsensitive)) {
        destination += ".xlsx";
    }
    if (std::filesystem::exists(destination)) {
        throw std::runtime_error("目标文件已存在，请更换名称或先手动处理旧文件");
    }

    QVariantList exportFields;
    issuetrace::XlsxTable table;
    for (const auto& item : form.fields) {
        const auto field = item.toMap();
        if (!field.value(QStringLiteral("xlsxVisible")).toBool()) continue;
        exportFields.push_back(field);
        table.headers.push_back(
            field.value(QStringLiteral("label")).toString().toUtf8().toStdString());
    }
    if (table.headers.empty()) {
        throw std::runtime_error("当前表单模板没有启用任何 XLSX 导出列");
    }

    const auto issues = store.searchIssues(query);
    for (const auto& issue : issues) {
        std::vector<issuetrace::XlsxCell> row;
        row.reserve(static_cast<std::size_t>(exportFields.size()));
        for (const auto& item : exportFields) {
            row.push_back(xlsxCell(item.toMap(), issue, store));
        }
        table.rows.push_back(std::move(row));
    }
    const auto bytes = issuetrace::buildXlsx(table);
    QSaveFile output(fromNativePath(destination));
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size()) ||
        !output.commit()) {
        throw std::runtime_error("XLSX 文件写入失败");
    }
    return {std::move(destination), issues.size()};
}
