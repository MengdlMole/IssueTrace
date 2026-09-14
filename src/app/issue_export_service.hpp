#pragma once

#include "form_template.hpp"
#include "issuetrace/issue_store.hpp"

#include <QByteArray>

#include <cstddef>
#include <filesystem>

struct XlsxExportResult {
    std::filesystem::path path;
    std::size_t issueCount{};
};

class IssueExportService final {
public:
    [[nodiscard]] static std::filesystem::path exportMarkdown(
        issuetrace::IssueStore& store,
        const issuetrace::StoredIssue& issue,
        const FormTemplateDefinition& form,
        const QByteArray& summaryTemplate,
        const std::filesystem::path& destinationRoot);

    [[nodiscard]] static XlsxExportResult exportXlsx(
        issuetrace::IssueStore& store,
        const issuetrace::IssueQuery& query,
        const FormTemplateDefinition& form,
        std::filesystem::path destination);
};
