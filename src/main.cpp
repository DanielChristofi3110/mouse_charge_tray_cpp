#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define DEBUG

#include <windows.h>
#include <windef.h>
#include <wingdi.h>
#include <shellapi.h>
#include <string>
#include <iostream>
#include <cstdio>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <vector>
#include <chrono>
#include <future>
#undef max
#undef min
#include "Config/parameters.hpp"

#include <lunasvg.h>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

constexpr UINT WM_TRAY_ICON = WM_APP + 1;
constexpr UINT_PTR TIMER_UPDATE = 1;

constexpr UINT ID_UPDATE = 1001;
constexpr UINT ID_ABOUT  = 1002;
constexpr UINT ID_EXIT   = 1003;

NOTIFYICONDATAW trayData{};
HICON hCustomTrayIcon = nullptr;

static int lastBattery = -1;
static int64_t lastTimestamp = -1;
static double rateMinutesPerPercent = -1.0;
static std::string lastDirection = "none";

static std::future<int> g_batteryFuture;
static bool g_isQuerying = false;

std::wstring FormatMinutes(double minutes)
{
    if (minutes < 1.0) return L"<1m";
    int m = static_cast<int>(minutes);
    int h = m / 60;
    int min = m % 60;

    if (h > 0) return std::to_wstring(h) + L"h " + std::to_wstring(min) + L"m";
    return std::to_wstring(min) + L"m";
}

int64_t GetCurrentTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

std::wstring EstimateTime(int current)
{
    int64_t now = GetCurrentTimeMs();

    if (lastBattery == -1)
    {
        lastBattery = current;
        lastTimestamp = now;
        return L"Calculating…";
    }

    if (current == lastBattery)
    {
        if (rateMinutesPerPercent <= 0) return L"Calculating…";
        if (lastDirection == "down")
            return L"Discharge: " + FormatMinutes(current * rateMinutesPerPercent);
        else if (lastDirection == "up")
            return L"Charge: " + FormatMinutes((100 - current) * rateMinutesPerPercent);
        else
            return L"Calculating…";
    }

    int64_t deltaMs = now - lastTimestamp;
    if (deltaMs <= 0) deltaMs = 1;
    double deltaMinutes = static_cast<double>(deltaMs) / 60000.0;
    int deltaPercent = current - lastBattery;

    std::string direction = (deltaPercent < 0) ? "down" : "up";

    // Filter jumps
    if (std::abs(deltaPercent) > 10)
    {
        spdlog::info("Battery jump detected -> reset tracking");
        lastBattery = current;
        lastTimestamp = now;
        return L"Calculating…";
    }

    double minutesPerPercent = deltaMinutes / std::abs(deltaPercent);

    // Smooth rate
    if (rateMinutesPerPercent < 0)
        rateMinutesPerPercent = minutesPerPercent;
    else
        rateMinutesPerPercent = (rateMinutesPerPercent * 0.7) + (minutesPerPercent * 0.3);

    lastDirection = direction;
    lastBattery = current;
    lastTimestamp = now;

    if (direction == "down")
    {
        double remaining = current * rateMinutesPerPercent;
        return L"Discharge: " + FormatMinutes(remaining);
    }
    else
    {
        double remaining = (100 - current) * rateMinutesPerPercent;
        return L"Charge: " + FormatMinutes(remaining);
    }
}

int QueryBluetoothMouseBattery()
{
    std::wstring cmdLine = L"powershell.exe -NoProfile -NonInteractive -Command \"[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; Get-PnpDevice -Class 'Bluetooth' | ForEach-Object { $test = $_ | Get-PnpDeviceProperty -KeyName '{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2' | Where-Object Type -ne 'Empty'; if ($test) { Write-Output ('Battery percentage of ' + $_.FriendlyName + ' is:'); Get-PnpDeviceProperty -InstanceId $test.InstanceId -KeyName '{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2' | ForEach-Object { $_.Data }; } }\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        spdlog::error("Failed to create pipe for PowerShell");
        return -1;
    }

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    BOOL success = CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    CloseHandle(hWritePipe);

    if (!success)
    {
        CloseHandle(hReadPipe);
        spdlog::error("Failed to create hidden PowerShell process");
        return -1;
    }

    std::string resultStream = "";
    char buffer[256];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        resultStream += buffer;
    }

    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::string targetName = Parameters::get().DeviceName;

    std::stringstream ss(resultStream);
    std::string line;
    bool isTarget = false;

    while (std::getline(ss, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        line.erase(0, line.find_first_not_of(" \n\r\t"));

        if (line.find(targetName) != std::string::npos)
        {
            isTarget = true;
            continue;
        }

        if (isTarget)
        {
            if (!line.empty() && line.find_first_not_of("0123456789") == std::string::npos)
            {
                try
                {
                    int battery = std::stoi(line);
                    if (battery >= 0 && battery <= 100)
                    {
                        spdlog::info("Successful battery query for {}: {}", targetName, battery);
                        return battery;
                    }
                }
                catch (...) {}
            }
            isTarget = false;
        }
    }

    spdlog::error("No valid battery data found for target device: {}", targetName);
    return -1;
}

HICON CreateIconFromSVGFileWithText(const std::wstring& filePath, int width, int height, const std::wstring& overlayText)
{
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

    auto document = lunasvg::Document::loadFromFile(utf8Path);
    if (!document)
        return nullptr;

    lunasvg::Bitmap bitmap = document->renderToBitmap(width, height);
    if (bitmap.isNull())
        return nullptr;

    bitmap.convertToRGBA();

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* ppvBits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &ppvBits, nullptr, 0);

    if (!hBitmap || !ppvBits)
    {
        if (hBitmap) DeleteObject(hBitmap);
        ReleaseDC(nullptr, hdc);
        return nullptr;
    }

    memcpy(ppvBits, bitmap.data(), width * height * 4);

    if (!overlayText.empty())
    {
        std::vector<uint32_t> originalPixels(width * height);
        memcpy(originalPixels.data(), ppvBits, width * height * 4);

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);

        SetBkMode(hdcMem, TRANSPARENT);
        int fontHeight = 32;

        HFONT hFont = CreateFontW(
            fontHeight,
            0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"YourCustomFontFamilyName"
        );
        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

        RECT rect = { 0, 0, width, height };

        SetTextColor(hdcMem, RGB(0, 0, 0));
        for (int dx = -1; dx <= 1; ++dx)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                if (dx == 0 && dy == 0) continue;
                RECT offsetRect = rect;
                OffsetRect(&offsetRect, dx, dy);
                DrawTextW(hdcMem, overlayText.c_str(), -1, &offsetRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        SetTextColor(hdcMem, RGB(255, 255, 255));
        DrawTextW(hdcMem, overlayText.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdcMem, hOldFont);
        DeleteObject(hFont);
        SelectObject(hdcMem, hOldBmp);
        DeleteDC(hdcMem);

        uint32_t* pixels = static_cast<uint32_t*>(ppvBits);
        for (int i = 0; i < width * height; ++i)
        {
            if (pixels[i] != originalPixels[i])
            {
                uint32_t p = pixels[i];
                uint8_t r = (p >> 16) & 0xFF;
                uint8_t g = (p >> 8) & 0xFF;
                uint8_t b = p & 0xFF;
                pixels[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }

    ReleaseDC(nullptr, hdc);

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

void TriggerAsyncBatteryUpdate()
{
    if (g_isQuerying)
    {
        spdlog::info("Previous battery query still running in background, skipping tick.");
        return;
    }

    g_isQuerying = true;
    spdlog::info("Launching background battery query...");

    g_batteryFuture = std::async(std::launch::async, []() {
        return QueryBluetoothMouseBattery();
    });
}

void UpdateBattery()
{
    if (g_isQuerying)
    {
        if (g_batteryFuture.valid())
        {
            auto status = g_batteryFuture.wait_for(std::chrono::milliseconds(0));
            if (status == std::future_status::ready)
            {
                int batteryPercentage = g_batteryFuture.get();
                g_isQuerying = false;

                std::wstring batteryText = L"?";
                std::wstring tipText = L"Mouse Battery - Searching...";

                if (batteryPercentage >= 0)
                {
                    batteryText = std::to_wstring(batteryPercentage);
                    std::wstring estimate = EstimateTime(batteryPercentage);

                    std::string devName = Parameters::get().DeviceName;
                    std::wstring wDevName(devName.begin(), devName.end());

                    tipText = wDevName + L"\nBattery: " + batteryText + L"%\n" + estimate;
                }
                else
                {
                    tipText = L"Mouse Battery - Not Connected / No Data";
                }

                std::wstring svgPathW(Parameters::get().svgPath.begin(), Parameters::get().svgPath.end());
                HICON newIcon = CreateIconFromSVGFileWithText(L"res/" + svgPathW, 32, 32, batteryText);
                if (newIcon)
                {
                    if (hCustomTrayIcon)
                    {
                        DestroyIcon(hCustomTrayIcon);
                    }
                    hCustomTrayIcon = newIcon;
                    trayData.hIcon = hCustomTrayIcon;
                }

                trayData.uFlags = NIF_ICON | NIF_TIP;
                lstrcpynW(trayData.szTip, tipText.c_str(), ARRAYSIZE(trayData.szTip));
                Shell_NotifyIconW(NIM_MODIFY, &trayData);
            }
        }
    }
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
        std::wstring svgPathW(Parameters::get().svgPath.begin(), Parameters::get().svgPath.end());
        hCustomTrayIcon = CreateIconFromSVGFileWithText(L"res/" + svgPathW, 32, 32, std::to_wstring(99));

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

        SetTimer(hwnd, TIMER_UPDATE, Parameters::get().updateMs, nullptr);
        UpdateBattery();
        return 0;
    }

    case WM_TIMER:
            if (wParam == TIMER_UPDATE)
            {
                UpdateBattery();
                TriggerAsyncBatteryUpdate();
            }
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
                TriggerAsyncBatteryUpdate();
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
    PWSTR pCmdLine,
    int)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool isDebug = false;

    if (argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"Debug") == 0)
            {
                isDebug = true;
                break;
            }
        }
        LocalFree(argv);
    }

    if (isDebug)
    {
        if (AllocConsole())
        {
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
            std::wcout.clear();
            std::cout.clear();
        }
        spdlog::warn("LOG STARTED FOR DEBUG BUILD");
    }

    Parameters::get().evaluate();
    TriggerAsyncBatteryUpdate();
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
    using namespace lunasvg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
