#include "issuetrace/summary_renderer.hpp"

#include <array>
#include <regex>
#include <stdexcept>

namespace issuetrace {
namespace {

void replaceAll(std::string& text, const std::string& needle,
                const std::string& replacement) {
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        text.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

std::string escapeYaml(const std::string& value) {
    std::string result;
    for (const char character : value) {
        if (character == '\\' || character == '"') result.push_back('\\');
        if (character == '\n') result += "\\n";
        else if (character != '\r') result.push_back(character);
    }
    return result;
}

void replaceField(std::string& result, const std::string& placeholder,
                  const std::string& value) {
    const auto token = "{{" + placeholder + "}}";
    const bool yamlQuoted = result.find(": \"" + token + "\"") !=
                            std::string::npos;
    replaceAll(result, token, yamlQuoted ? escapeYaml(value) : value);
}

std::pair<std::size_t, std::size_t> sectionContentRange(
    const std::string_view text, const std::string_view name) {
    const auto startMarker = "<!-- issuetrace:auto:" + std::string(name) + ":start -->";
    const auto endMarker = "<!-- issuetrace:auto:" + std::string(name) + ":end -->";
    const auto start = text.find(startMarker);
    if (start == std::string_view::npos) {
        throw std::runtime_error("总结草稿缺少自动区间起始标记: " + std::string(name));
    }
    const auto contentStart = start + startMarker.size();
    const auto end = text.find(endMarker, contentStart);
    if (end == std::string_view::npos) {
        throw std::runtime_error("总结草稿缺少自动区间结束标记: " + std::string(name));
    }
    return {contentStart, end};
}

}  // namespace

std::string renderSummaryMarkdown(const std::string_view templateText,
                                  const SummaryRenderContext& context) {
    std::string result(templateText);
    for (const auto& [field, value] : context.issueFields) {
        replaceField(result, "issue." + field, value);
    }
    replaceField(result, "timeline", context.timelineMarkdown);
    replaceField(result, "attachments", context.attachmentsMarkdown);
    replaceField(result, "summary.created_at", context.createdAt);
    replaceField(result, "summary.updated_at", context.updatedAt);

    static const std::regex unresolved(R"(\{\{([a-zA-Z0-9_.]+)\}\})");
    std::smatch match;
    if (std::regex_search(result, match, unresolved)) {
        throw std::runtime_error("总结模板包含未知变量: " + match[1].str());
    }
    return result;
}

std::string refreshSummaryAutoSections(const std::string_view existingDraft,
                                       const std::string_view freshlyRendered) {
    std::string result(existingDraft);
    constexpr std::array<std::string_view, 2> sections{"timeline", "attachments"};
    for (const auto section : sections) {
        const auto [newStart, newEnd] = sectionContentRange(freshlyRendered, section);
        const std::string replacement(freshlyRendered.substr(newStart, newEnd - newStart));
        const auto [oldStart, oldEnd] = sectionContentRange(result, section);
        result.replace(oldStart, oldEnd - oldStart, replacement);
    }
    return result;
}

}  // namespace issuetrace
