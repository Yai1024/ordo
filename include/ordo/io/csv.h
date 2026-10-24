#pragma once

// CSV 行级纯函数（OUTLINE §5.2、§9.4）。
// 只处理"一行文本 ↔ 一组字段"，不碰文件、不认识 Task。

#include <string>
#include <string_view>
#include <vector>

#include "ordo/core/result.h"

namespace ordo::csv {

// 去掉 UTF-8 BOM（EF BB BF）。文件可能由 Excel 保存，读取时务必先调用。
std::string_view strip_bom(std::string_view text);

// 按 RFC 4180 切分一行：
//   - 含逗号的字段用双引号包住；
//   - 字段内的双引号写成两个双引号；
//   - 空字段合法（相邻逗号）。
// 未闭合的引号返回 InvalidFormat，由调用方转成 Corrupted。
Result<std::vector<std::string>> parse_line(std::string_view line);

// 反向操作：字段按需加引号后用逗号连接，不含换行。
std::string join_fields(const std::vector<std::string>& fields);

// 单个字段的转义；不含 , " \r \n 时原样返回。
std::string quote_field(std::string_view field);

}  // namespace ordo::csv
