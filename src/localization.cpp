#include "localization.h"

#include <cwchar>

const LocalizedStrings& Localization::Text(Language language) {
    static const LocalizedStrings zh_hans{
        L"开启鼠标模式",
        L"关闭鼠标模式",
        L"设置...",
        L"开机自启动",
        L"语言",
        L"退出",
        L"键盘鼠标模式：开启",
        L"键盘鼠标模式：关闭",
        L"鼠标模式开启",
        L"鼠标模式关闭",
        L"键盘鼠标模式设置 v1.2",
        L"鼠标移动速度 (px/s)：",
        L"开机自启动",
        L"版本：1.2",
        L"使用说明：\r\n"
        L"Ctrl + Alt + M：开启 / 关闭鼠标模式\r\n"
        L"Esc：关闭鼠标模式\r\n"
        L"W / A / S / D：移动鼠标光标\r\n"
        L"J / K：鼠标左键 / 右键\r\n"
        L"方向键：模拟垂直 / 水平滚轮",
        L"保存",
        L"取消",
    };
    static const LocalizedStrings zh_hant{
        L"開啟滑鼠模式",
        L"關閉滑鼠模式",
        L"設定...",
        L"開機自動啟動",
        L"語言",
        L"結束",
        L"鍵盤滑鼠模式：開啟",
        L"鍵盤滑鼠模式：關閉",
        L"滑鼠模式開啟",
        L"滑鼠模式關閉",
        L"鍵盤滑鼠模式設定 v1.2",
        L"滑鼠移動速度 (px/s)：",
        L"開機自動啟動",
        L"版本：1.2",
        L"使用說明：\r\n"
        L"Ctrl + Alt + M：開啟 / 關閉滑鼠模式\r\n"
        L"Esc：關閉滑鼠模式\r\n"
        L"W / A / S / D：移動滑鼠游標\r\n"
        L"J / K：滑鼠左鍵 / 右鍵\r\n"
        L"方向鍵：模擬垂直 / 水平滾輪",
        L"儲存",
        L"取消",
    };
    static const LocalizedStrings en{
        L"Turn mouse mode on",
        L"Turn mouse mode off",
        L"Settings...",
        L"Start with Windows",
        L"Language",
        L"Exit",
        L"Keyboard mouse mode: on",
        L"Keyboard mouse mode: off",
        L"Mouse mode on",
        L"Mouse mode off",
        L"Keyboard Mouse Mode Settings v1.2",
        L"Mouse speed (px/s):",
        L"Start with Windows",
        L"Version: 1.2",
        L"Usage:\r\n"
        L"Ctrl + Alt + M: turn mouse mode on / off\r\n"
        L"Esc: turn mouse mode off\r\n"
        L"W / A / S / D: move cursor\r\n"
        L"J / K: left / right mouse button\r\n"
        L"Arrow keys: vertical / horizontal scrolling",
        L"Save",
        L"Cancel",
    };

    if (language == Language::TraditionalChinese) {
        return zh_hant;
    }
    if (language == Language::English) {
        return en;
    }
    return zh_hans;
}

Language Localization::DetectDefaultLanguage() {
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
        if (_wcsicmp(locale_name, L"zh-CN") == 0 || _wcsicmp(locale_name, L"zh-SG") == 0) {
            return Language::SimplifiedChinese;
        }
        if (_wcsicmp(locale_name, L"zh-TW") == 0 || _wcsicmp(locale_name, L"zh-HK") == 0 ||
            _wcsicmp(locale_name, L"zh-MO") == 0) {
            return Language::TraditionalChinese;
        }
    }

    const LANGID language_id = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(language_id) == LANG_CHINESE) {
        const WORD sub_language = SUBLANGID(language_id);
        if (sub_language == SUBLANG_CHINESE_TRADITIONAL || sub_language == SUBLANG_CHINESE_HONGKONG ||
            sub_language == SUBLANG_CHINESE_MACAU) {
            return Language::TraditionalChinese;
        }
        return Language::SimplifiedChinese;
    }

    return Language::English;
}

const wchar_t* Localization::ConfigValue(Language language) {
    if (language == Language::TraditionalChinese) {
        return L"zh-Hant";
    }
    if (language == Language::English) {
        return L"en";
    }
    return L"zh-Hans";
}

Language Localization::FromConfigValue(const wchar_t* value) {
    if (_wcsicmp(value, L"zh-Hant") == 0) {
        return Language::TraditionalChinese;
    }
    if (_wcsicmp(value, L"en") == 0) {
        return Language::English;
    }
    if (_wcsicmp(value, L"zh-Hans") == 0) {
        return Language::SimplifiedChinese;
    }
    return DetectDefaultLanguage();
}
