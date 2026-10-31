#pragma once

#include <windows.h>
#include <vector>
#include <string>

// Scans for available Bluetooth devices using PowerShell
std::vector<std::string> ScanAvailableDevices();

// Opens a modal Win32 dialog letting the user pick a device from the scan list
std::string ShowDeviceSelectionDialog(HWND hwndParent);
