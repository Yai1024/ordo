#include "ordo/gui/win32_util.h"

#include <cstddef>

namespace ordo::gui {

std::wstring to_wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

std::string to_utf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0, nullptr,
                                           nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

int scale_for_dpi(int dpi, int value) {
    return MulDiv(value, dpi, 96);
}

int system_dpi() {
    HDC dc = GetDC(nullptr);
    if (dc == nullptr) {
        return 96;
    }
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi > 0 ? dpi : 96;
}

HFONT create_ui_font(int dpi, int point_size, int weight) {
    LOGFONTW logfont{};
    logfont.lfHeight = -MulDiv(point_size, dpi, 72);
    logfont.lfWeight = weight;
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfQuality = CLEARTYPE_QUALITY;
    lstrcpynW(logfont.lfFaceName, L"Microsoft YaHei UI", LF_FACESIZE);
    return CreateFontIndirectW(&logfont);
}

}  // namespace ordo::gui
