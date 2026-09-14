#include "form_template.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <stdexcept>

FormTemplateDefinition parseFormTemplate(const QByteArray& json) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error((QStringLiteral("表单模板 JSON 无效：") +
                                  parseError.errorString()).toUtf8().constData());
    }
    const auto root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt() != 1 ||
        root.value(QStringLiteral("id")).toString().isEmpty() ||
        root.value(QStringLiteral("version")).toInt() < 1 ||
        !root.value(QStringLiteral("fields")).isArray()) {
        throw std::runtime_error("表单模板缺少有效的 schemaVersion、id、version 或 fields");
    }
    static const QRegularExpression validId(QStringLiteral("^[a-z][a-z0-9_]*$"));
    const QSet<QString> allowedTypes{QStringLiteral("text"),
        QStringLiteral("multiline_text"), QStringLiteral("person"),
        QStringLiteral("datetime"), QStringLiteral("select"),
        QStringLiteral("timeline_summary")};
    const QHash<QString, QString> expectedFields{
        {QStringLiteral("title"), QStringLiteral("multiline_text")},
        {QStringLiteral("original_problem"), QStringLiteral("multiline_text")},
        {QStringLiteral("reporter"), QStringLiteral("person")},
        {QStringLiteral("reported_at"), QStringLiteral("datetime")},
        {QStringLiteral("assignee"), QStringLiteral("person")},
        {QStringLiteral("service"), QStringLiteral("text")},
        {QStringLiteral("version"), QStringLiteral("text")},
        {QStringLiteral("ticket"), QStringLiteral("text")},
        {QStringLiteral("status"), QStringLiteral("select")},
        {QStringLiteral("priority"), QStringLiteral("select")},
        {QStringLiteral("progress"), QStringLiteral("timeline_summary")},
        {QStringLiteral("conclusion"), QStringLiteral("multiline_text")},
        {QStringLiteral("resolved_at"), QStringLiteral("datetime")}};
    QSet<QString> ids;
    QVariantList fields;
    for (const auto& item : root.value(QStringLiteral("fields")).toArray()) {
        if (!item.isObject()) throw std::runtime_error("表单字段必须是 JSON 对象");
        const auto field = item.toObject();
        const auto id = field.value(QStringLiteral("id")).toString();
        const auto label = field.value(QStringLiteral("label")).toString();
        const auto type = field.value(QStringLiteral("type")).toString();
        if (!validId.match(id).hasMatch() || ids.contains(id) || label.isEmpty() ||
            !allowedTypes.contains(type) || expectedFields.value(id) != type) {
            throw std::runtime_error((QStringLiteral("无效或重复的表单字段：") + id)
                                         .toUtf8().constData());
        }
        ids.insert(id);
        if (type == QStringLiteral("select") &&
            !field.value(QStringLiteral("options")).isArray()) {
            throw std::runtime_error((QStringLiteral("单选字段缺少 options：") + id)
                                         .toUtf8().constData());
        }
        const auto placements = field.value(QStringLiteral("placements")).toObject();
        QVariantMap value = field.toVariantMap();
        value.insert(QStringLiteral("detailVisible"),
                     placements.value(QStringLiteral("detail")).toBool());
        value.insert(QStringLiteral("xlsxVisible"),
                     placements.value(QStringLiteral("xlsx")).toBool());
        value.insert(QStringLiteral("readOnly"),
                     field.value(QStringLiteral("readOnly")).toBool() ||
                         type == QStringLiteral("timeline_summary") ||
                         id == QStringLiteral("reported_at") ||
                         id == QStringLiteral("resolved_at"));
        fields.push_back(std::move(value));
    }
    if (ids != QSet<QString>(expectedFields.keyBegin(), expectedFields.keyEnd())) {
        throw std::runtime_error("表单模板必须且只能包含 IssueTrace 的固定标准字段");
    }
    return {root.value(QStringLiteral("id")).toString(),
            root.value(QStringLiteral("name")).toString(),
            root.value(QStringLiteral("version")).toInt(), std::move(fields)};
}
