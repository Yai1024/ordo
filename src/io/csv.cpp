#include "ordo/io/csv.h"

#include <utility>

namespace ordo::csv {

std::string_view strip_bom(std::string_view text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        return text.substr(3);
    }
    return text;
}

Result<std::vector<std::string>> parse_line(std::string_view line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;
    bool field_started_with_quote = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;  // 跳过转义用的第二个引号
                } else {
                    in_quotes = false;
                }
            } else {
                field.push_back(ch);
            }
            continue;
        }

        if (ch == '"' && field.empty() && !field_started_with_quote) {
            in_quotes = true;
            field_started_with_quote = true;
        } else if (ch == ',') {
            fields.push_back(std::move(field));
            field.clear();
            field_started_with_quote = false;
        } else {
            field.push_back(ch);
        }
    }

    if (in_quotes) {
        return Result<std::vector<std::string>>::fail(ErrorCode::InvalidFormat,
                                                      "CSV 行中存在未闭合的双引号");
    }

    fields.push_back(std::move(field));
    return Result<std::vector<std::string>>::ok(std::move(fields));
}

std::string quote_field(std::string_view field) {
    const bool needs_quotes = field.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!needs_quotes) {
        return std::string{field};
    }

    std::string quoted;
    quoted.reserve(field.size() + 2);
    quoted.push_back('"');
    for (const char ch : field) {
        if (ch == '"') {
            quoted.push_back('"');
        }
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
}

std::string join_fields(const std::vector<std::string>& fields) {
    std::string line;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            line.push_back(',');
        }
        line += quote_field(fields[i]);
    }
    return line;
}

}  // namespace ordo::csv
