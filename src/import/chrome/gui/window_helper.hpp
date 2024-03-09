// Copyright ?019 Domagoj "oberth" Pand�a
// MIT license | Read LICENSE.txt for details.

#pragma once
#pragma comment(lib, "uxtheme.lib") 
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "Dcomp.lib")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Windowscodecs.lib")
#pragma comment(lib, "Shcore.lib")

#include <cmath>
#include <cstdint>
#include <Windows.h>
#include <windowsx.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <Uxtheme.h>

namespace chrome::gui::helper {

    // Simplest variant, just to get you started.
    inline auto compute_sector_of_window(HWND window_handle, WPARAM, LPARAM lparam, int caption_height) -> LRESULT {

        // Acquire the window rect
        RECT window_rectangle;
        GetWindowRect(window_handle, &window_rectangle);

        auto offset = 10;

        POINT cursor_position{
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam)
        };

        if (cursor_position.y < window_rectangle.top + offset && cursor_position.x < window_rectangle.left + offset) return HTTOPLEFT;
        if (cursor_position.y < window_rectangle.top + offset && cursor_position.x > window_rectangle.right - offset) return HTTOPRIGHT;
        if (cursor_position.y > window_rectangle.bottom - offset && cursor_position.x > window_rectangle.right - offset) return HTBOTTOMRIGHT;
        if (cursor_position.y > window_rectangle.bottom - offset && cursor_position.x < window_rectangle.left + offset) return HTBOTTOMLEFT;

        if (cursor_position.x > window_rectangle.left && cursor_position.x < window_rectangle.right) {
            if (cursor_position.y < window_rectangle.top + offset) return HTTOP;
            else if (cursor_position.y > window_rectangle.bottom - offset) return HTBOTTOM;
        }
        if (cursor_position.y > window_rectangle.top && cursor_position.y < window_rectangle.bottom) {
            if (cursor_position.x < window_rectangle.left + offset) return HTLEFT;
            else if (cursor_position.x > window_rectangle.right - offset) return HTRIGHT;
        }

        if (cursor_position.x > window_rectangle.left && cursor_position.x < window_rectangle.right) {
            if (cursor_position.y < window_rectangle.top + caption_height) return HTCAPTION;
        }

        return HTNOWHERE;

    }

    inline auto compute_standard_caption_height_for_window(HWND window_handle) {

        SIZE caption_size {};
        auto const accounting_for_borders = 2;
        auto theme = OpenThemeData(window_handle, L"WINDOW");
        auto dpi = GetDpiForWindow(window_handle);
        
        /*GetThemePartSize(theme, nullptr, WP_CLOSEBUTTON, CS_ACTIVE, nullptr, TS_TRUE, &caption_size);
        CloseThemeData(theme);

		auto height = static_cast<float>(caption_size.cy * dpi) / 96.0f;
        return static_cast<int>(height) + accounting_for_borders;*/

        RECT rcFrame = { 0 };
        AdjustWindowRectExForDpi(&rcFrame, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
        // abs(rcFrame.top) will contain the caption bar height
        return -rcFrame.top - accounting_for_borders;

    }

}