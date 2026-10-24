// M0 冒烟测试：验证测试框架本身可用，并锁定版本宏。

#include <string_view>

#include "ordo/version.h"
#include "test_framework.h"

ORDO_TEST(version_string_is_well_formed) {
    constexpr std::string_view version = ORDO_VERSION_STRING;
    ORDO_CHECK(!version.empty());
    ORDO_CHECK_EQ(version.find('.'), std::size_t{1});
}
