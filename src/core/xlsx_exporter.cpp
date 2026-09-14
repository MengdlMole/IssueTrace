#include "issuetrace/xlsx_exporter.hpp"

#include <xlsxwriter.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <stdexcept>

namespace issuetrace {
namespace {

std::size_t utf8Characters(const std::string& text) {
    std::size_t result = 0;
    for (const unsigned char value : text) {
        if ((value & 0xc0U) != 0x80U) ++result;
    }
    return result;
}

std::string excelText(const std::string& text) {
    constexpr std::size_t maximumCharacters = 32767;
    if (utf8Characters(text) <= maximumCharacters) return text;
    const std::string suffix = "\n…（内容过长，已截断）";
    const auto contentLimit = maximumCharacters - utf8Characters(suffix);
    std::size_t characters = 0;
    std::size_t cutoff = 0;
    while (cutoff < text.size()) {
        const auto byte = static_cast<unsigned char>(text[cutoff]);
        if ((byte & 0xc0U) != 0x80U) {
            if (characters == contentLimit) break;
            ++characters;
        }
        ++cutoff;
    }
    return text.substr(0, cutoff) + suffix;
}

void check(const lxw_error error, const char* operation) {
    if (error != LXW_NO_ERROR) {
        throw std::runtime_error(std::string(operation) + ": " +
                                 lxw_strerror(error));
    }
}

lxw_datetime localDateTime(const std::int64_t milliseconds) {
    const auto seconds = static_cast<std::time_t>(milliseconds / 1000);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    return {local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
            local.tm_hour, local.tm_min, static_cast<double>(local.tm_sec)};
}

}  // namespace

std::vector<unsigned char> buildXlsx(const XlsxTable& table) {
    if (table.headers.empty()) throw std::invalid_argument("XLSX 表头不能为空");
    for (const auto& row : table.rows) {
        if (row.size() != table.headers.size()) {
            throw std::invalid_argument("XLSX 数据列数与表头不一致");
        }
    }

    const char* outputBuffer = nullptr;
    std::size_t outputSize = 0;
    lxw_workbook_options options{};
    options.output_buffer = &outputBuffer;
    options.output_buffer_size = &outputSize;
    auto* workbook = workbook_new_opt(nullptr, &options);
    if (!workbook) throw std::runtime_error("无法创建 XLSX 工作簿");
    auto* worksheet = workbook_add_worksheet(workbook, "问题清单");
    if (!worksheet) {
        workbook_close(workbook);
        throw std::runtime_error("无法创建 XLSX 工作表");
    }

    auto* header = workbook_add_format(workbook);
    format_set_bold(header);
    format_set_font_color(header, LXW_COLOR_WHITE);
    format_set_bg_color(header, 0x1F4E78);
    format_set_align(header, LXW_ALIGN_CENTER);
    format_set_align(header, LXW_ALIGN_VERTICAL_CENTER);
    format_set_border(header, LXW_BORDER_THIN);
    format_set_border_color(header, 0xD9E2F3);

    auto* body = workbook_add_format(workbook);
    format_set_text_wrap(body);
    format_set_align(body, LXW_ALIGN_VERTICAL_TOP);

    auto* date = workbook_add_format(workbook);
    format_set_num_format(date, "yyyy-mm-dd hh:mm");
    format_set_align(date, LXW_ALIGN_VERTICAL_TOP);

    std::vector<std::size_t> widths(table.headers.size(), 8);
    for (std::size_t column = 0; column < table.headers.size(); ++column) {
        const auto value = excelText(table.headers[column]);
        check(worksheet_write_string(worksheet, 0, static_cast<lxw_col_t>(column),
                                     value.c_str(), header),
              "写入 XLSX 表头失败");
        widths[column] = std::max<std::size_t>(8, utf8Characters(value) + 2);
    }
    for (std::size_t row = 0; row < table.rows.size(); ++row) {
        for (std::size_t column = 0; column < table.headers.size(); ++column) {
            const auto& cell = table.rows[row][column];
            if (cell.utcMilliseconds) {
                auto value = localDateTime(*cell.utcMilliseconds);
                check(worksheet_write_datetime(worksheet, static_cast<lxw_row_t>(row + 1),
                                               static_cast<lxw_col_t>(column),
                                               &value, date),
                      "写入 XLSX 日期失败");
                widths[column] = std::max<std::size_t>(widths[column], 18);
            } else {
                const auto value = excelText(cell.text);
                check(worksheet_write_string(worksheet, static_cast<lxw_row_t>(row + 1),
                                             static_cast<lxw_col_t>(column),
                                             value.c_str(), body),
                      "写入 XLSX 文本失败");
                widths[column] = std::max(widths[column],
                    std::min<std::size_t>(utf8Characters(value) + 2, 50));
            }
        }
    }
    worksheet_freeze_panes(worksheet, 1, 0);
    check(worksheet_autofilter(worksheet, 0, 0,
                              static_cast<lxw_row_t>(table.rows.size()),
                              static_cast<lxw_col_t>(table.headers.size() - 1)),
          "添加 XLSX 筛选失败");
    check(worksheet_set_row(worksheet, 0, 24, header), "设置 XLSX 表头高度失败");
    for (std::size_t column = 0; column < widths.size(); ++column) {
        check(worksheet_set_column(worksheet, static_cast<lxw_col_t>(column),
                                   static_cast<lxw_col_t>(column),
                                   static_cast<double>(widths[column]), nullptr),
              "设置 XLSX 列宽失败");
    }

    const auto closeError = workbook_close(workbook);
    if (closeError != LXW_NO_ERROR) {
        std::free(const_cast<char*>(outputBuffer));
        throw std::runtime_error(std::string("生成 XLSX 失败: ") +
                                 lxw_strerror(closeError));
    }
    std::vector<unsigned char> result(outputBuffer, outputBuffer + outputSize);
    std::free(const_cast<char*>(outputBuffer));
    return result;
}

}  // namespace issuetrace
