#include "ordo/algo/conflict.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ordo {

std::vector<Warning> detect_conflicts(const std::vector<Block>& blocks) {
    std::vector<Warning> warnings;

    std::vector<Block> sorted = blocks;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Block& left, const Block& right) {
        if (left.begin != right.begin) {
            return left.begin < right.begin;
        }
        return left.end < right.end;
    });

    for (const Block& block : sorted) {
        if (!is_valid(block)) {
            warnings.push_back(Warning{block.task, "时间块无效：结束时间不晚于开始时间"});
        }
    }

    // 块数量在百级，直接按开始时间做扫描即可；不会漏掉任何重叠对。
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        for (std::size_t j = i + 1; j < sorted.size(); ++j) {
            if (sorted[j].begin >= sorted[i].end) {
                break;  // 后面的块开始得更晚，不会与 i 重叠
            }
            if (!overlaps(sorted[i].begin, sorted[i].end, sorted[j].begin, sorted[j].end)) {
                continue;
            }

            warnings.push_back(Warning{sorted[i].task,
                                       "与任务 #" + std::to_string(sorted[j].task) +
                                           " 的时间块重叠"});
            warnings.push_back(Warning{sorted[j].task,
                                       "与任务 #" + std::to_string(sorted[i].task) +
                                           " 的时间块重叠"});
        }
    }

    return warnings;
}

}  // namespace ordo
