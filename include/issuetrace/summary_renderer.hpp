#pragma once

#include <map>
#include <string>
#include <string_view>

namespace issuetrace {

struct SummaryRenderContext {
    std::map<std::string, std::string> issueFields;
    std::string timelineMarkdown;
    std::string attachmentsMarkdown;
    std::string createdAt;
    std::string updatedAt;
};

[[nodiscard]] std::string renderSummaryMarkdown(
    std::string_view templateText, const SummaryRenderContext& context);

[[nodiscard]] std::string refreshSummaryAutoSections(
    std::string_view existingDraft, std::string_view freshlyRendered);

}  // namespace issuetrace
