#include "form_template.hpp"
#include "issue_export_service.hpp"

#include <QFile>

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool rejects(const auto& operation) {
    try {
        operation();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

struct DirectoryCleanup {
    std::filesystem::path path;
    ~DirectoryCleanup() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

}  // namespace

int main() {
    const auto testRoot = std::filesystem::temp_directory_path() /
                          "issuetrace-export-service-tests";
    const auto workspace = testRoot / "工作区";
    const auto output = testRoot / "导出结果";
    std::error_code ignored;
    std::filesystem::remove_all(testRoot, ignored);
    const DirectoryCleanup cleanup{testRoot};
    std::filesystem::create_directories(output);

    issuetrace::IssueStore store(workspace);
    auto issue = store.createIssue("支付/回调超时", "李雷");
    issue.ticket = "BUG-42";
    issue.status = "investigating";
    issue.service = "支付服务";
    issue.version = "2.4.0";
    issue.assignee = "韩梅梅";
    issue.originalProblem = "客户反馈回调间歇性超时。";
    issue.conclusion = "上游连接池容量不足。";
    store.updateIssue(issue);

    const auto timeline = store.createTimelineEntry(
        issue.id, "log", "发现连接池等待时间明显升高。");
    const std::array<unsigned char, 8> pngHeader{
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    const auto attachment = store.addAttachment(
        timeline.id, "现场截图.png", "image/png", "test-sha256", pngHeader);
    const auto descriptionEntry = store.createTimelineEntry(
        issue.id, "_description_attachment", "事件描述图片");
    const auto descriptionAttachment = store.addAttachment(
        descriptionEntry.id, "原始反馈.png", "image/png", "description-sha256", pngHeader);
    assert(store.listTimelineEntries(issue.id).size() == 1);

    QFile formFile(QStringLiteral(ISSUETRACE_DEFAULT_FORM_PATH));
    QFile summaryFile(QStringLiteral(ISSUETRACE_DEFAULT_SUMMARY_PATH));
    assert(formFile.open(QIODevice::ReadOnly));
    assert(summaryFile.open(QIODevice::ReadOnly));
    const auto form = parseFormTemplate(formFile.readAll());
    const auto summaryTemplate = summaryFile.readAll();

    const auto markdownFolder = IssueExportService::exportMarkdown(
        store, *store.findIssue(issue.id), form, summaryTemplate, output);
    assert(markdownFolder.filename().string().ends_with(" BUG-42-支付_回调超时"));
    assert(markdownFolder.filename().string().substr(0, 10).find_first_not_of("0123456789") == std::string::npos);
    assert(std::filesystem::is_regular_file(markdownFolder / "事件记录.md"));
    assert(std::filesystem::is_regular_file(markdownFolder / "事件总结.md"));
    for (const auto& item : std::filesystem::directory_iterator(output)) {
        assert(!item.path().filename().string().starts_with(".issuetrace-export-"));
    }
    const auto exportedAttachmentName =
        std::filesystem::path(attachment.relativePath).filename();
    assert(std::filesystem::is_regular_file(
        markdownFolder / "图片" / exportedAttachmentName));
    const auto descriptionAttachmentName =
        std::filesystem::path(descriptionAttachment.relativePath).filename();
    assert(std::filesystem::is_regular_file(
        markdownFolder / "图片" / descriptionAttachmentName));

    const auto record = readText(markdownFolder / "事件记录.md");
    const auto summary = readText(markdownFolder / "事件总结.md");
    assert(record.find("支付/回调超时") != std::string::npos);
    assert(record.find("状态：处理中") != std::string::npos);
    assert(record.find("发现连接池等待时间明显升高") != std::string::npos);
    assert(record.find("![现场截图.png](图片/" +
                       exportedAttachmentName.generic_string() + ")") !=
           std::string::npos);
    assert(record.find("![原始反馈.png](图片/" +
                       descriptionAttachmentName.generic_string() + ")") !=
           std::string::npos);
    assert(summary.find("事件处理的关键步骤和操作") != std::string::npos);
    assert(summary.find("验证和遗留问题") != std::string::npos);
    assert(summary.find("验证环境、验证方法、验证结果") != std::string::npos);
    assert(summary.find("上游连接池容量不足") == std::string::npos);
    assert(summary.find("发现连接池等待时间明显升高") == std::string::npos);
    assert(summary.find("现场截图.png") == std::string::npos);
    assert(rejects([&] {
        static_cast<void>(IssueExportService::exportMarkdown(
            store, issue, form, summaryTemplate, output));
    }));

    const auto xlsx = IssueExportService::exportXlsx(
        store, {}, form, output / "问题清单.XLSX");
    assert(xlsx.issueCount == 1);
    assert(xlsx.path.filename() == "问题清单.XLSX");
    assert(std::filesystem::file_size(xlsx.path) > 1000);
    std::ifstream xlsxInput(xlsx.path, std::ios::binary);
    std::array<char, 2> signature{};
    xlsxInput.read(signature.data(), static_cast<std::streamsize>(signature.size()));
    assert(signature[0] == 'P' && signature[1] == 'K');
    assert(rejects([&] {
        static_cast<void>(IssueExportService::exportXlsx(
            store, {}, form, xlsx.path));
    }));

    std::cout << "All issue export service tests passed.\n";
}
