#include "form_template.hpp"

#include <QFile>
#include <QStringList>

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {

bool rejects(const QByteArray& json) {
    try {
        static_cast<void>(parseFormTemplate(json));
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

}  // namespace

int main() {
    QFile file(QStringLiteral(ISSUETRACE_DEFAULT_FORM_PATH));
    assert(file.open(QIODevice::ReadOnly));
    const auto definition = parseFormTemplate(file.readAll());
    assert(definition.id == QStringLiteral("default-issue"));
    assert(definition.fields.size() == 15);
    assert(definition.fields.front().toMap().value(QStringLiteral("id")) ==
           QStringLiteral("title"));
    assert(definition.fields.front().toMap().value(QStringLiteral("xlsxVisible")).toBool());
    const QStringList expectedOrder{
        QStringLiteral("title"), QStringLiteral("original_problem"),
        QStringLiteral("priority"), QStringLiteral("group_name"),
        QStringLiteral("tags"), QStringLiteral("reporter"),
        QStringLiteral("assignee"), QStringLiteral("version"),
        QStringLiteral("service"), QStringLiteral("ticket"),
        QStringLiteral("progress"), QStringLiteral("conclusion"),
        QStringLiteral("reported_at"), QStringLiteral("resolved_at"),
        QStringLiteral("status")};
    for (qsizetype index = 0; index < expectedOrder.size(); ++index) {
        assert(definition.fields.at(index).toMap().value(QStringLiteral("id")) ==
               expectedOrder.at(index));
    }
    assert(definition.fields.at(3).toMap().value(QStringLiteral("type")) ==
           QStringLiteral("history_select"));
    assert(definition.fields.at(5).toMap().value(QStringLiteral("type")) ==
           QStringLiteral("history_select"));
    assert(definition.fields.at(6).toMap().value(QStringLiteral("type")) ==
           QStringLiteral("history_select"));
    assert(rejects("{}"));
    assert(rejects(R"({"schemaVersion":1})"));
    assert(rejects(R"json({"schemaVersion":1,"id":"x","version":1,"fields":[
        {"id":"title","label":"问题","type":"text","placements":{"detail":true}},
        {"id":"title","label":"重复","type":"text","placements":{"detail":true}}]})json"));
    assert(rejects(R"json({"schemaVersion":1,"id":"x","version":1,"fields":[
        {"id":"title","label":"问题","type":"unknown","placements":{"detail":true}}]})json"));
    assert(rejects(R"json({"schemaVersion":1,"id":"x","version":1,"fields":[
        {"id":"title","label":"问题","type":"text","placements":{"detail":true}},
        {"id":"status","label":"状态","type":"select","placements":{"detail":true}}]})json"));
    std::cout << "All form template tests passed.\n";
}
