#include "config.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>

namespace {
constexpr wchar_t kAppName[] = L"KeyboardMouseMode";
constexpr wchar_t kRunRegistryPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}

std::wstring ConfigManager::ExePath() {
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return buffer;
}

std::wstring ConfigManager::ConfigPath() {
    PWSTR app_data = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &app_data))) {
        dir = app_data;
        CoTaskMemFree(app_data);
        dir += L"\\KeyboardMouseMode";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\config.ini";
    }
    return L"config.ini";
}

bool ConfigManager::IsStartupEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunRegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t value[MAX_PATH]{};
    DWORD type = REG_SZ;
    DWORD size = sizeof(value);
    const LONG status = RegQueryValueExW(key, kAppName, nullptr, &type, reinterpret_cast<BYTE*>(value), &size);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_SZ) {
        return false;
    }

    return _wcsicmp(value, ExePath().c_str()) == 0;
}

bool ConfigManager::SetStartupEnabled(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunRegistryPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG status = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring path = ExePath();
        status = RegSetValueExW(key, kAppName, 0, REG_SZ, reinterpret_cast<const BYTE*>(path.c_str()),
                                static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, kAppName);
        if (status == ERROR_FILE_NOT_FOUND) {
            status = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

void ConfigManager::Save(const AppConfig& config) {
    const std::wstring path = ConfigPath();
    WritePrivateProfileStringW(L"settings", L"move_speed_px_per_sec",
                               std::to_wstring(config.move_speed_px_per_sec).c_str(), path.c_str());
    WritePrivateProfileStringW(L"settings", L"startup_enabled",
                               config.startup_enabled ? L"true" : L"false", path.c_str());
    WritePrivateProfileStringW(L"settings", L"language", Localization::ConfigValue(config.language), path.c_str());
}

AppConfig ConfigManager::Load() {
    const std::wstring path = ConfigPath();
    AppConfig config;
    config.move_speed_px_per_sec = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"settings", L"move_speed_px_per_sec", 800, path.c_str())),
        100,
        3000);

    wchar_t language[32]{};
    GetPrivateProfileStringW(L"settings", L"language", L"", language, 32, path.c_str());
    config.language = Localization::FromConfigValue(language);
    config.startup_enabled = IsStartupEnabled();
    Save(config);
    return config;
}
