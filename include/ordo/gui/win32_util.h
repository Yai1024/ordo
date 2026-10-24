#pragma once

// Win32 适配器的公共小工具：字符串、DPI、字体。

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <string_view>

namespace ordo::gui {

// 与 ui::AppModel 的边界统一用 UTF-8；窗口控件统一用 UTF-16。
std::wstring to_wide(std::string_view text);
std::string to_utf8(std::wstring_view text);

// 96 DPI 下的逻辑尺寸 → 当前 DPI 下的像素。
int scale_for_dpi(int dpi, int value);
int system_dpi();

HFONT create_ui_font(int dpi, int point_size, int weight = FW_NORMAL);

}  // namespace ordo::gui
