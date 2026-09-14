#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace issuetrace {

struct XlsxCell {
    std::string text;
    std::optional<std::int64_t> utcMilliseconds;
};

struct XlsxTable {
    std::vector<std::string> headers;
    std::vector<std::vector<XlsxCell>> rows;
};

[[nodiscard]] std::vector<unsigned char> buildXlsx(const XlsxTable& table);

}  // namespace issuetrace
