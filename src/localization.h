#pragma once

#include <windows.h>

enum class Language {
    SimplifiedChinese = 0,
    TraditionalChinese = 1,
    English = 2,
};

struct LocalizedStrings {
    const wchar_t* toggle_on;
    const wchar_t* toggle_off;
    const wchar_t* settings;
    const wchar_t* startup;
    const wchar_t* language;
    const wchar_t* exit;
    const wchar_t* tip_on;
    const wchar_t* tip_off;
    const wchar_t* toast_on;
    const wchar_t* toast_off;
    const wchar_t* settings_title;
    const wchar_t* speed_label;
    const wchar_t* startup_label;
    const wchar_t* version_label;
    const wchar_t* usage_text;
    const wchar_t* save;
    const wchar_t* cancel;
};

class Localization {
public:
    static const LocalizedStrings& Text(Language language);
    static Language DetectDefaultLanguage();
    static const wchar_t* ConfigValue(Language language);
    static Language FromConfigValue(const wchar_t* value);
};
