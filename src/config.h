#pragma once

#include "localization.h"

#include <string>

struct AppConfig {
    int move_speed_px_per_sec = 800;
    bool startup_enabled = false;
    Language language = Language::SimplifiedChinese;
};

class ConfigManager {
public:
    static AppConfig Load();
    static void Save(const AppConfig& config);
    static bool IsStartupEnabled();
    static bool SetStartupEnabled(bool enabled);

private:
    static std::wstring ExePath();
    static std::wstring ConfigPath();
};
