// M1：CSV 切分、转义与 BOM（OUTLINE §9）。

#include <string>
#include <vector>

#include "ordo/io/csv.h"
#include "test_framework.h"

ORDO_TEST(csv_splits_plain_fields) {
    const auto parsed = ordo::csv::parse_line("1,高数作业,高等数学");
    ORDO_CHECK(parsed.has_value());
    const std::vector<std::string> expected{"1", "高数作业", "高等数学"};
    ORDO_CHECK(parsed.value() == expected);
}

ORDO_TEST(csv_keeps_commas_inside_quotes) {
    const auto parsed = ordo::csv::parse_line("2,\"读书,第3章\",数学");
    ORDO_CHECK(parsed.has_value());
    const std::vector<std::string> expected{"2", "读书,第3章", "数学"};
    ORDO_CHECK(parsed.value() == expected);
}

ORDO_TEST(csv_unescapes_double_quotes) {
    const auto parsed = ordo::csv::parse_line("\"他说\"\"好\"\"\",x");
    ORDO_CHECK(parsed.has_value());
    const std::vector<std::string> expected{"他说\"好\"", "x"};
    ORDO_CHECK(parsed.value() == expected);
}

ORDO_TEST(csv_handles_empty_and_trailing_fields) {
    const auto middle = ordo::csv::parse_line("a,,c");
    ORDO_CHECK(middle.has_value());
    ORDO_CHECK(middle.value().size() == 3);
    ORDO_CHECK(middle.value()[1].empty());

    const auto trailing = ordo::csv::parse_line("a,b,");
    ORDO_CHECK(trailing.has_value());
    ORDO_CHECK(trailing.value().size() == 3);
    ORDO_CHECK(trailing.value()[2].empty());

    const auto blank = ordo::csv::parse_line("");
    ORDO_CHECK(blank.has_value());
    ORDO_CHECK(blank.value().size() == 1);
    ORDO_CHECK(blank.value()[0].empty());
}

ORDO_TEST(csv_rejects_unterminated_quote) {
    const auto parsed = ordo::csv::parse_line("\"没有闭合");
    ORDO_CHECK(!parsed.has_value());
    ORDO_CHECK(parsed.status().code == ordo::ErrorCode::InvalidFormat);
}

ORDO_TEST(csv_join_quotes_only_when_needed) {
    ORDO_CHECK_EQ(ordo::csv::join_fields({"a", "b"}), std::string{"a,b"});
    ORDO_CHECK_EQ(ordo::csv::join_fields({"a", "b,c"}), std::string{"a,\"b,c\""});
    ORDO_CHECK_EQ(ordo::csv::join_fields({"a\"b"}), std::string{"\"a\"\"b\""});
}

ORDO_TEST(csv_round_trip_preserves_fields) {
    const std::vector<std::string> fields{"1", "读书,第3章", "他说\"好\"", "", "数学"};
    const auto parsed = ordo::csv::parse_line(ordo::csv::join_fields(fields));
    ORDO_CHECK(parsed.has_value());
    ORDO_CHECK(parsed.value() == fields);
}

ORDO_TEST(csv_strips_utf8_bom) {
    const std::string with_bom = "\xEF\xBB\xBFid,title";
    ORDO_CHECK_EQ(ordo::csv::strip_bom(with_bom), std::string_view{"id,title"});
    ORDO_CHECK_EQ(ordo::csv::strip_bom("id,title"), std::string_view{"id,title"});
    ORDO_CHECK(ordo::csv::strip_bom("\xEF\xBB").size() == 2);
}
