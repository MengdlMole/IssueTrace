#include "form_template.hpp"

#include <QFile>

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
    assert(definition.fields.size() == 13);
    assert(definition.fields.front().toMap().value(QStringLiteral("id")) ==
           QStringLiteral("title"));
    assert(definition.fields.front().toMap().value(QStringLiteral("xlsxVisible")).toBool());
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
