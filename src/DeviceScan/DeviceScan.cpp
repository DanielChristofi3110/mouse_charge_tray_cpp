#include "DeviceScan.hpp"
#include <cstddef>
#include <sstream>
#include <spdlog/spdlog.h>

struct DeviceSelectionContext {
    std::vector<std::string> devices;
    std::string selectedDevice;
    bool confirmed = false;
    HWND hwndList = nullptr;
};

std::vector<std::string> ScanAvailableDevices()
{
    std::vector<std::string> deviceList;
    std::wstring cmdLine = L"powershell.exe -NoProfile -NonInteractive -Command \"[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; Get-PnpDevice -Class 'Bluetooth' | ForEach-Object { $test = $_ | Get-PnpDeviceProperty -KeyName '{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2' | Where-Object Type -ne 'Empty'; if ($test) { Write-Output ('Battery percentage of ' + $_.FriendlyName + ' is:'); Get-PnpDeviceProperty -InstanceId $test.InstanceId -KeyName '{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2' | ForEach-Object { $_.Data }; } }\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        spdlog::error("Failed to create pipe for PowerShell device scan");
        return deviceList;
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
        spdlog::error("Failed to create hidden PowerShell process for device scan");
        return deviceList;
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

    std::stringstream ss(resultStream);
    std::string line;

    std::string prefix = "Battery percentage of ";
    std::string suffix = " is:";

    while (std::getline(ss, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        line.erase(0, line.find_first_not_of(" \n\r\t"));

        if (line.rfind(prefix, 0) == 0)
        {
            size_t startPos = prefix.length();
            size_t endPos = line.find(suffix);

            if (endPos != std::string::npos)
            {
                std::string deviceName = line.substr(startPos, endPos - startPos);
                deviceName.erase(deviceName.find_last_not_of(" \n\r\t") + 1);
                deviceName.erase(0, deviceName.find_first_not_of(" \n\r\t"));

                if (!deviceName.empty())
                {
                    deviceList.push_back(deviceName);
                }
            }
        }
    }

    return deviceList;
}

static LRESULT CALLBACK DevicePickerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    DeviceSelectionContext* pContext = reinterpret_cast<DeviceSelectionContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pContext = reinterpret_cast<DeviceSelectionContext*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pContext));

        pContext->hwndList = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_HASSTRINGS,
            15, 15, 310, 180,
            hwnd, (HMENU)101, GetModuleHandle(nullptr), nullptr
        );

        for (const auto& dev : pContext->devices) {
            std::wstring wdev(dev.begin(), dev.end());
            SendMessageW(pContext->hwndList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wdev.c_str()));
        }
        if (!pContext->devices.empty()) {
            SendMessageW(pContext->hwndList, LB_SETCURSEL, 0, 0);
        }


        CreateWindowExW(
            0, L"BUTTON", L"Select",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            135, 210, 90, 30,
            hwnd, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr
        );


        CreateWindowExW(
            0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            235, 210, 90, 30,
            hwnd, (HMENU)IDCANCEL, GetModuleHandle(nullptr), nullptr
        );
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);


        if (id == IDOK || (id == 101 && code == LBN_DBLCLK)) {
            int selIndex = static_cast<int>(SendMessageW(pContext->hwndList, LB_GETCURSEL, 0, 0));
            if (selIndex != LB_ERR && pContext) {
                pContext->selectedDevice = pContext->devices[static_cast<size_t>(selIndex)];
                pContext->confirmed = true;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        else if (id == IDCANCEL) {
            if (pContext) {
                pContext->confirmed = false;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        if (pContext) pContext->confirmed = false;
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}


std::string ShowDeviceSelectionDialog(HWND hwndParent) {
    DeviceSelectionContext context;
    context.devices = ScanAvailableDevices();

    if (context.devices.empty()) {
        MessageBoxW(hwndParent, L"No Bluetooth devices with battery info found.", L"Scan Results", MB_OK | MB_ICONINFORMATION);
        return "";
    }


    WNDCLASSW wc{};
    wc.lpfnWndProc = DevicePickerProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"MouseBatteryDevicePickerWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);


    if (hwndParent) EnableWindow(hwndParent, FALSE);

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"MouseBatteryDevicePickerWindow",
        L"Select Bluetooth Device",
        WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 355, 290,
        hwndParent, nullptr, GetModuleHandle(nullptr), &context
    );


    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);


        if (!IsWindow(hDlg)) break;
    }


    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
        SetForegroundWindow(hwndParent);
    }

    if (context.confirmed) {
        return context.selectedDevice;
    }
    return "";
}
