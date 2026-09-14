#pragma once

#include <QByteArray>
#include <QVariantList>
#include <QString>

struct FormTemplateDefinition {
    QString id;
    QString name;
    int version{};
    QVariantList fields;
};

FormTemplateDefinition parseFormTemplate(const QByteArray& json);
