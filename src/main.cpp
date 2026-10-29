#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h>
#include <lunasvg.h>
#include <string>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
// #pragma comment(lib, "lunasvg.lib") // Link your lunasvg library here

constexpr UINT WM_TRAY_ICON = WM_APP + 1;
constexpr UINT_PTR TIMER_UPDATE = 1;

constexpr UINT ID_UPDATE = 1001;
constexpr UINT ID_ABOUT  = 1002;
constexpr UINT ID_EXIT   = 1003;

NOTIFYICONDATAW trayData{};
HICON hCustomTrayIcon = nullptr; // Track your custom icon handle for cleanup

// Helper function to create an HICON from an SVG file/string using lunasvg
HICON CreateIconFromSVGFile(const std::wstring& filePath, int width, int height)
{
    // Convert std::wstring (UTF-16) to std::string (UTF-8) for lunasvg
    std::string utf8Path;
    if (!filePath.empty())
    {
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), (int)filePath.length(), nullptr, 0, nullptr, nullptr);
        if (sizeNeeded > 0)
        {
            utf8Path.resize(sizeNeeded);
            WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), (int)filePath.length(), &utf8Path[0], sizeNeeded, nullptr, nullptr);
        }
    }

    // Load SVG document directly from file path
    auto document = lunasvg::Document::loadFromFile(utf8Path);
    if (!document)
        return nullptr;

    // Render SVG to lunasvg Bitmap
    lunasvg::Bitmap bitmap = document->renderToBitmap(width, height);
    if (bitmap.isNull())
        return nullptr;

    // Convert pixel layout to RGBA format expected by Win32/GDI
    bitmap.convertToRGBA();

    // Set up a 32-bit DIB (Device Independent Bitmap) header
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Negative height for top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* ppvBits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &ppvBits, nullptr, 0);
    ReleaseDC(nullptr, hdc);

    if (!hBitmap || !ppvBits)
    {
        if (hBitmap) DeleteObject(hBitmap);
        return nullptr;
    }

    // Copy lunasvg pixel buffer into the DIB section buffer
    memcpy(ppvBits, bitmap.data(), width * height * 4);

    // Create a transparent monochrome mask bitmap
    HBITMAP hMonoBitmap = CreateBitmap(width, height, 1, 1, nullptr);

    ICONINFO iconInfo{};
    iconInfo.fIcon = TRUE;
    iconInfo.xHotspot = 0;
    iconInfo.yHotspot = 0;
    iconInfo.hbmMask = hMonoBitmap;
    iconInfo.hbmColor = hBitmap;

    HICON hIcon = CreateIconIndirect(&iconInfo);

    DeleteObject(hBitmap);
    DeleteObject(hMonoBitmap);

    return hIcon;
}

void UpdateBattery()
{
    // TODO: Query the Bluetooth mouse battery here.
    trayData.uFlags = NIF_TIP;
    lstrcpynW(
        trayData.szTip,
        L"Mouse Battery - Waiting for battery data",
        ARRAYSIZE(trayData.szTip)
    );

    Shell_NotifyIconW(NIM_MODIFY, &trayData);
}

void ShowTrayMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING, ID_UPDATE, L"Update");
    AppendMenuW(menu, MF_STRING, ID_ABOUT, L"About");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_EXIT, L"Exit");

    SetForegroundWindow(hwnd);

    POINT point{};
    GetCursorPos(&point);

    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        point.x,
        point.y,
        0,
        hwnd,
        nullptr
    );

    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {

        hCustomTrayIcon = CreateIconFromSVGFile(L"res/mouse-svgrepo-com.svg", 32, 32);

        trayData.cbSize = sizeof(trayData);
        trayData.hWnd = hwnd;
        trayData.uID = 1;
        trayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        trayData.uCallbackMessage = WM_TRAY_ICON;

        trayData.hIcon = hCustomTrayIcon ? hCustomTrayIcon : LoadIconW(nullptr, IDI_APPLICATION);

        lstrcpynW(
            trayData.szTip,
            L"Mouse Battery",
            ARRAYSIZE(trayData.szTip)
        );

        if (!Shell_NotifyIconW(NIM_ADD, &trayData))
        {
            MessageBoxW(
                hwnd,
                L"Could not create the tray icon.",
                L"Mouse Battery",
                MB_OK | MB_ICONERROR
            );

            return -1;
        }

        SetTimer(hwnd, TIMER_UPDATE, 60'000, nullptr);
        UpdateBattery();
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_UPDATE)
            UpdateBattery();
        return 0;

    case WM_TRAY_ICON:
        switch (LOWORD(lParam))
        {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu(hwnd);
            return 0;

        case WM_LBUTTONDBLCLK:
            UpdateBattery();
            return 0;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_UPDATE:
            UpdateBattery();
            return 0;

        case ID_ABOUT:
            MessageBoxW(
                hwnd,
                L"Mouse Battery Tray\nVersion: 1.1\n"
                L"Shows Bluetooth mouse battery information.",
                L"About",
                MB_OK | MB_ICONINFORMATION
            );
            return 0;

        case ID_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_UPDATE);
        Shell_NotifyIconW(NIM_DELETE, &trayData);

        // Clean up our custom icon resource
        if (hCustomTrayIcon)
        {
            DestroyIcon(hCustomTrayIcon);
        }

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int)
{
    constexpr wchar_t CLASS_NAME[] = L"MouseBatteryTrayWindow";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassW(&wc))
        return 1;

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Mouse Battery Tray",
        0,
        0, 0, 0, 0,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
        return 1;

    MSG msg{};
    using namespace lunasvg; // Optional guard if needed
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
