#pragma once

// 跨层返回类型（OUTLINE §7.2）。
// 约定：可预期的失败（空数据、格式错、重复 id、文件不存在）一律用返回值表达，
// 不抛异常；异常只在穿越 main() 时统一捕获。

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ordo {

enum class ErrorCode {
    Ok = 0,
    NotFound,       // 记录或文件不存在
    Duplicate,      // id 重复
    InvalidFormat,  // 字段格式错误
    InvalidValue,   // 数值越界（如 importance 不在 1..5）
    EmptyInput,     // 输入为空
    IoError,        // 读写失败
    Corrupted,      // 数据文件结构损坏
};

// 便于日志与测试输出；不要把返回值当作面向用户的错误消息。
std::string_view error_code_name(ErrorCode code) noexcept;

struct Status {
    ErrorCode code{ErrorCode::Ok};
    std::string message;  // 面向用户的中文说明

    bool ok() const noexcept { return code == ErrorCode::Ok; }

    static Status success() { return Status{}; }

    static Status fail(ErrorCode code, std::string message) {
        return Status{code, std::move(message)};
    }
};

// Result<T>：成功时必然带值，失败时必然带错误码。
// value() 只允许在 has_value() 为真时调用（README 与测试都按此约定使用）。
template <typename T>
class Result {
public:
    static Result ok(T value) {
        Result result;
        result.value_.emplace(std::move(value));
        return result;
    }

    static Result fail(ErrorCode code, std::string message) {
        Result result;
        result.status_ = Status::fail(code, std::move(message));
        return result;
    }

    bool has_value() const noexcept { return value_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() {
        assert(value_.has_value());
        return *value_;
    }

    const T& value() const {
        assert(value_.has_value());
        return *value_;
    }

    const Status& status() const noexcept { return status_; }

private:
    std::optional<T> value_;
    Status status_;
};

}  // namespace ordo
