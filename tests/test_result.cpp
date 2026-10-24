// M1：Result / Status 的行为（OUTLINE §7.2）。

#include <string>

#include "ordo/core/result.h"
#include "test_framework.h"

ORDO_TEST(status_success_is_ok) {
    const ordo::Status status = ordo::Status::success();
    ORDO_CHECK(status.ok());
    ORDO_CHECK(status.code == ordo::ErrorCode::Ok);
    ORDO_CHECK(status.message.empty());
}

ORDO_TEST(status_fail_carries_code_and_message) {
    const ordo::Status status = ordo::Status::fail(ordo::ErrorCode::NotFound, "找不到记录");
    ORDO_CHECK(!status.ok());
    ORDO_CHECK(status.code == ordo::ErrorCode::NotFound);
    ORDO_CHECK_EQ(status.message, std::string{"找不到记录"});
}

ORDO_TEST(result_ok_exposes_value) {
    const auto result = ordo::Result<int>::ok(42);
    ORDO_CHECK(result.has_value());
    ORDO_CHECK(static_cast<bool>(result));
    ORDO_CHECK_EQ(result.value(), 42);
    ORDO_CHECK(result.status().ok());
}

ORDO_TEST(result_fail_has_no_value_and_keeps_message) {
    const auto result = ordo::Result<int>::fail(ordo::ErrorCode::Corrupted, "文件损坏");
    ORDO_CHECK(!result.has_value());
    ORDO_CHECK(!static_cast<bool>(result));
    ORDO_CHECK(result.status().code == ordo::ErrorCode::Corrupted);
    ORDO_CHECK_EQ(result.status().message, std::string{"文件损坏"});
}

ORDO_TEST(error_code_name_is_stable) {
    ORDO_CHECK(ordo::error_code_name(ordo::ErrorCode::Ok) == "ok");
    ORDO_CHECK(ordo::error_code_name(ordo::ErrorCode::Duplicate) == "duplicate");
    ORDO_CHECK(ordo::error_code_name(ordo::ErrorCode::IoError) == "io_error");
}
