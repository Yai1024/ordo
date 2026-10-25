#include "ordo/algo/sm2.h"

#include <algorithm>
#include <cmath>

namespace ordo {
namespace {

double clamp_ease(double value) {
    return std::clamp(value, kMinEase, kMaxEase);
}

int clamp_interval(int days) {
    return std::clamp(days, 1, kMaxIntervalDays);
}

}  // namespace

Result<Sm2State> sm2_update(const Sm2State& current, int quality) {
    if (quality < 0 || quality > 5) {
        return Result<Sm2State>::fail(ErrorCode::InvalidValue,
                                      "复习质量必须在 0 到 5 之间");
    }

    Sm2State next;
    next.stage = std::max(0, current.stage);
    next.ease = clamp_ease(current.ease);
    next.interval_days = std::max(0, current.interval_days);

    const double ease_before = next.ease;
    const double delta = 5.0 - static_cast<double>(quality);
    next.ease = clamp_ease(ease_before + 0.1 - delta * (0.08 + delta * 0.02));

    if (quality < 3) {
        // 没想起来：重复次数清零，明天再来；难度系数仍按本次表现下调。
        next.stage = 0;
        next.interval_days = 1;
        return Result<Sm2State>::ok(next);
    }

    if (next.stage == 0) {
        next.interval_days = 1;
    } else if (next.stage == 1) {
        next.interval_days = 6;
    } else {
        // 标准 SM-2：用更新前的 ease 计算间隔，再推进重复次数。
        const int scaled = static_cast<int>(std::lround(next.interval_days * ease_before));
        next.interval_days = clamp_interval(scaled);
    }
    next.stage += 1;
    return Result<Sm2State>::ok(next);
}

bool review_due(const Task& task, TimePoint now) {
    if (task.kind != TaskKind::Review) {
        return false;
    }
    if (!task.last_review.has_value()) {
        return true;
    }
    return now >= add_minutes(*task.last_review, task.interval_days * kMinutesPerDay);
}

int review_days_until_due(const Task& task, TimePoint now) {
    if (task.kind != TaskKind::Review || !task.last_review.has_value()) {
        return 0;
    }
    const Minutes remaining =
        minutes_between(now, add_minutes(*task.last_review, task.interval_days * kMinutesPerDay));
    if (remaining <= 0) {
        return 0;
    }
    return (remaining + kMinutesPerDay - 1) / kMinutesPerDay;
}

}  // namespace ordo
