#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>

#include "config.h"
#include "dpi.h"
#include "localization.h"
#include "resource.h"

#include <algorithm>
#include <cmath>
#include <string>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Ole32.lib")

namespace {

constexpr wchar_t kAppName[] = L"KeyboardMouseMode";
constexpr wchar_t kAppVersion[] = L"1.2";
constexpr wchar_t kMainWindowClass[] = L"KeyboardMouseModeWindow";
constexpr wchar_t kToastWindowClass[] = L"KeyboardMouseModeToast";
constexpr wchar_t kSettingsWindowClass[] = L"KeyboardMouseModeSettings";
constexpr UINT kHotkeyId = 1;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kMoveTimerId = 10;
constexpr UINT kToastTimerId = 11;
constexpr UINT kTrayIconId = 100;

constexpr UINT kMenuToggle = 2001;
constexpr UINT kMenuSettings = 2002;
constexpr UINT kMenuStartup = 2003;
constexpr UINT kMenuExit = 2004;
constexpr UINT kMenuLanguageZhHans = 2010;
constexpr UINT kMenuLanguageZhHant = 2011;
constexpr UINT kMenuLanguageEn = 2012;

constexpr int kEditSpeedId = 3001;
constexpr int kCheckStartupId = 3002;
constexpr int kButtonSaveId = 3003;
constexpr int kButtonCancelId = 3004;

constexpr ULONGLONG kInitialNudgeOnlyMs = 20;

struct SpeedCurvePoint {
    ULONGLONG held_ms;
    double speed_ratio;
};

constexpr SpeedCurvePoint kSpeedCurve[] = {
    {20, 0.0},
    {100, 0.05},
    {200, 0.10},
    {300, 0.20},
    {400, 0.50},
};
constexpr size_t kSpeedCurveCount = sizeof(kSpeedCurve) / sizeof(kSpeedCurve[0]);

struct KeyState {
    bool w = false;
    bool a = false;
    bool s = false;
    bool d = false;
    bool j = false;
    bool k = false;
};

HINSTANCE g_instance = nullptr;
HWND g_main_window = nullptr;
HWND g_toast_window = nullptr;
HWND g_settings_window = nullptr;
HHOOK g_keyboard_hook = nullptr;
HANDLE g_single_instance_mutex = nullptr;
HFONT g_default_gui_font = nullptr;
AppConfig g_config;
KeyState g_keys;
bool g_mouse_mode_enabled = false;
bool g_startup_failed = false;
ULONGLONG g_last_move_tick = 0;
ULONGLONG g_move_started_tick = 0;
double g_remainder_x = 0.0;
double g_remainder_y = 0.0;
int g_last_dir_x = 0;
int g_last_dir_y = 0;
UINT g_toast_dpi = 96;
std::wstring g_toast_text;

const LocalizedStrings& Text() {
    return Localization::Text(g_config.language);
}

HMENU ControlId(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

HICON LoadAppIcon(int resource_id, int size) {
    return reinterpret_cast<HICON>(LoadImageW(
        g_instance,
        MAKEINTRESOURCEW(resource_id),
        IMAGE_ICON,
        size,
        size,
        LR_DEFAULTCOLOR | LR_SHARED));
}

HWND CreateSettingsControl(
    DWORD ex_style,
    const wchar_t* class_name,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    HWND parent,
    int id = 0) {
    HWND control = CreateWindowExW(
        ex_style,
        class_name,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        width,
        height,
        parent,
        id == 0 ? nullptr : ControlId(id),
        g_instance,
        nullptr);
    if (control && g_default_gui_font) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_default_gui_font), TRUE);
    }
    return control;
}

void ResetInputState() {
    g_keys = {};
    g_remainder_x = 0.0;
    g_remainder_y = 0.0;
    g_last_move_tick = GetTickCount64();
    g_move_started_tick = 0;
    g_last_dir_x = 0;
    g_last_dir_y = 0;
}

void SendMouseFlags(DWORD flags, DWORD data = 0) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.mouseData = data;
    SendInput(1, &input, sizeof(INPUT));
}

void MoveMouse(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

double GetMovementSpeedPxPerSec(ULONGLONG held_ms) {
    if (held_ms <= kInitialNudgeOnlyMs) {
        return 0.0;
    }

    for (size_t index = 1; index < kSpeedCurveCount; ++index) {
        const SpeedCurvePoint previous = kSpeedCurve[index - 1];
        const SpeedCurvePoint current = kSpeedCurve[index];
        if (held_ms <= current.held_ms) {
            const double t = static_cast<double>(held_ms - previous.held_ms) /
                             static_cast<double>(current.held_ms - previous.held_ms);
            const double ratio = previous.speed_ratio + (current.speed_ratio - previous.speed_ratio) * t;
            return g_config.move_speed_px_per_sec * ratio;
        }
    }

    return static_cast<double>(g_config.move_speed_px_per_sec);
}

void GetMoveDirection(int& dir_x, int& dir_y) {
    dir_x = 0;
    dir_y = 0;
    if (g_keys.a) --dir_x;
    if (g_keys.d) ++dir_x;
    if (g_keys.w) --dir_y;
    if (g_keys.s) ++dir_y;
}

void NudgeMouseForNewMoveGesture() {
    int dir_x = 0;
    int dir_y = 0;
    GetMoveDirection(dir_x, dir_y);
    if (dir_x == 0 && dir_y == 0) {
        return;
    }

    MoveMouse(dir_x, dir_y);
    g_remainder_x = 0.0;
    g_remainder_y = 0.0;
    g_move_started_tick = GetTickCount64();
    g_last_move_tick = g_move_started_tick;
    g_last_dir_x = dir_x;
    g_last_dir_y = dir_y;
}

void ShowToast(const wchar_t* text);
void UpdateTrayIcon();
void EnsureSettingsWindow();

void ReleasePressedMouseButtons() {
    if (g_keys.j) {
        SendMouseFlags(MOUSEEVENTF_LEFTUP);
    }
    if (g_keys.k) {
        SendMouseFlags(MOUSEEVENTF_RIGHTUP);
    }
}

void SetMouseMode(bool enabled, bool show_toast = true) {
    if (g_mouse_mode_enabled == enabled) {
        return;
    }

    if (!enabled) {
        ReleasePressedMouseButtons();
    }
    g_mouse_mode_enabled = enabled;
    ResetInputState();
    UpdateTrayIcon();
    if (show_toast) {
        ShowToast(enabled ? Text().toast_on : Text().toast_off);
    }
}

void ToggleMouseMode() {
    SetMouseMode(!g_mouse_mode_enabled);
}

void SetLanguage(Language language) {
    if (g_config.language == language) {
        return;
    }

    g_config.language = language;
    ConfigManager::Save(g_config);
    UpdateTrayIcon();
    if (g_settings_window) {
        DestroyWindow(g_settings_window);
        EnsureSettingsWindow();
    }
}

bool IsMappedKey(DWORD vk) {
    switch (vk) {
    case 'W':
    case 'A':
    case 'S':
    case 'D':
    case 'J':
    case 'K':
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_ESCAPE:
        return true;
    default:
        return false;
    }
}

void HandleMappedKey(DWORD vk, bool down) {
    int before_x = 0;
    int before_y = 0;
    GetMoveDirection(before_x, before_y);
    bool direction_key = false;

    switch (vk) {
    case 'W':
        g_keys.w = down;
        direction_key = true;
        break;
    case 'A':
        g_keys.a = down;
        direction_key = true;
        break;
    case 'S':
        g_keys.s = down;
        direction_key = true;
        break;
    case 'D':
        g_keys.d = down;
        direction_key = true;
        break;
    case 'J':
        if (g_keys.j != down) {
            g_keys.j = down;
            SendMouseFlags(down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        }
        break;
    case 'K':
        if (g_keys.k != down) {
            g_keys.k = down;
            SendMouseFlags(down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        }
        break;
    case VK_UP:
        if (down) {
            SendMouseFlags(MOUSEEVENTF_WHEEL, WHEEL_DELTA);
        }
        break;
    case VK_DOWN:
        if (down) {
            SendMouseFlags(MOUSEEVENTF_WHEEL, static_cast<DWORD>(-WHEEL_DELTA));
        }
        break;
    case VK_LEFT:
        if (down) {
            SendMouseFlags(MOUSEEVENTF_HWHEEL, static_cast<DWORD>(-WHEEL_DELTA));
        }
        break;
    case VK_RIGHT:
        if (down) {
            SendMouseFlags(MOUSEEVENTF_HWHEEL, WHEEL_DELTA);
        }
        break;
    case VK_ESCAPE:
        if (down) {
            SetMouseMode(false);
        }
        break;
    default:
        break;
    }

    if (direction_key) {
        int after_x = 0;
        int after_y = 0;
        GetMoveDirection(after_x, after_y);
        if (after_x == 0 && after_y == 0) {
            g_move_started_tick = 0;
            g_last_dir_x = 0;
            g_last_dir_y = 0;
            g_remainder_x = 0.0;
            g_remainder_y = 0.0;
        } else if (down && before_x == 0 && before_y == 0) {
            NudgeMouseForNewMoveGesture();
        }
    }
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && g_mouse_mode_enabled) {
        const auto* event = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
        const DWORD vk = event->vkCode;
        if (IsMappedKey(vk)) {
            const bool down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
            const bool up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
            if (down || up) {
                HandleMappedKey(vk, down);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
}

void TickMove() {
    if (!g_mouse_mode_enabled) {
        return;
    }

    const ULONGLONG now = GetTickCount64();
    const double elapsed = std::max<double>((now - g_last_move_tick) / 1000.0, 0.001);
    g_last_move_tick = now;

    int dir_x = 0;
    int dir_y = 0;
    GetMoveDirection(dir_x, dir_y);

    if (dir_x == 0 && dir_y == 0) {
        g_move_started_tick = 0;
        g_last_dir_x = 0;
        g_last_dir_y = 0;
        return;
    }

    if (g_move_started_tick == 0 || dir_x != g_last_dir_x || dir_y != g_last_dir_y) {
        g_move_started_tick = now;
        g_last_dir_x = dir_x;
        g_last_dir_y = dir_y;
        g_remainder_x = 0.0;
        g_remainder_y = 0.0;
    }

    const ULONGLONG held_ms = now - g_move_started_tick;
    const double speed = GetMovementSpeedPxPerSec(held_ms);

    const double length = std::sqrt(static_cast<double>(dir_x * dir_x + dir_y * dir_y));
    const double raw_dx = (dir_x / length) * speed * elapsed + g_remainder_x;
    const double raw_dy = (dir_y / length) * speed * elapsed + g_remainder_y;
    const int move_x = static_cast<int>(raw_dx);
    const int move_y = static_cast<int>(raw_dy);
    g_remainder_x = raw_dx - move_x;
    g_remainder_y = raw_dy - move_y;
    MoveMouse(move_x, move_y);
}

void UpdateTrayIcon() {
    if (!g_main_window) {
        return;
    }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_main_window;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = kTrayMessage;
    nid.hIcon = LoadAppIcon(g_mouse_mode_enabled ? IDI_TRAY_ON : IDI_TRAY_OFF, GetSystemMetrics(SM_CXSMICON));
    wcscpy_s(nid.szTip, g_mouse_mode_enabled ? Text().tip_on : Text().tip_off);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void AddTrayIcon() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_main_window;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = kTrayMessage;
    nid.hIcon = LoadAppIcon(IDI_TRAY_OFF, GetSystemMetrics(SM_CXSMICON));
    wcscpy_s(nid.szTip, Text().tip_off);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_main_window;
    nid.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    HMENU language_menu = CreatePopupMenu();
    if (!language_menu) {
        DestroyMenu(menu);
        return;
    }

    AppendMenuW(menu, MF_STRING, kMenuToggle, g_mouse_mode_enabled ? Text().toggle_off : Text().toggle_on);
    AppendMenuW(menu, MF_STRING, kMenuSettings, Text().settings);
    AppendMenuW(menu, MF_STRING | (g_config.startup_enabled ? MF_CHECKED : MF_UNCHECKED), kMenuStartup, Text().startup);
    AppendMenuW(language_menu, MF_STRING | (g_config.language == Language::SimplifiedChinese ? MF_CHECKED : MF_UNCHECKED), kMenuLanguageZhHans, L"简体中文");
    AppendMenuW(language_menu, MF_STRING | (g_config.language == Language::TraditionalChinese ? MF_CHECKED : MF_UNCHECKED), kMenuLanguageZhHant, L"繁體中文");
    AppendMenuW(language_menu, MF_STRING | (g_config.language == Language::English ? MF_CHECKED : MF_UNCHECKED), kMenuLanguageEn, L"English");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(language_menu), Text().language);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, Text().exit);

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(g_main_window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, g_main_window, nullptr);
    DestroyMenu(menu);
}

LRESULT CALLBACK ToastWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect{};
        GetClientRect(hwnd, &rect);

        HBRUSH brush = CreateSolidBrush(RGB(40, 40, 40));
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT font = CreateFontW(Dpi::Scale(28, g_toast_dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        HFONT old_font = reinterpret_cast<HFONT>(SelectObject(hdc, font));
        DrawTextW(hdc, g_toast_text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old_font);
        DeleteObject(font);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void ShowToast(const wchar_t* text) {
    g_toast_text = text;
    if (!g_toast_window) {
        g_toast_window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kToastWindowClass, L"",
                                         WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 280, 88,
                                         nullptr, nullptr, g_instance, nullptr);
    }

    HMONITOR monitor = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONEAREST);
    g_toast_dpi = Dpi::ForWindowOrSystem(GetForegroundWindow());
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    const int width = Dpi::Scale(280, g_toast_dpi);
    const int height = Dpi::Scale(88, g_toast_dpi);
    const int x = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - width) / 2;
    const int y = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - height) / 2;
    SetWindowPos(g_toast_window, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g_toast_window, nullptr, TRUE);
    SetTimer(g_main_window, kToastTimerId, 1000, nullptr);
}

void EnsureSettingsWindow();

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        const UINT dpi = Dpi::ForWindowOrSystem(hwnd);
        CreateSettingsControl(0, L"STATIC", Text().speed_label, 0,
                              Dpi::Scale(24, dpi), Dpi::Scale(24, dpi), Dpi::Scale(250, dpi), Dpi::Scale(28, dpi), hwnd);

        HWND edit = CreateSettingsControl(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_NUMBER | ES_AUTOHSCROLL,
                                          Dpi::Scale(300, dpi), Dpi::Scale(20, dpi), Dpi::Scale(150, dpi), Dpi::Scale(32, dpi), hwnd, kEditSpeedId);
        std::wstring speed = std::to_wstring(g_config.move_speed_px_per_sec);
        SetWindowTextW(edit, speed.c_str());

        HWND check = CreateSettingsControl(0, L"BUTTON", Text().startup_label, BS_AUTOCHECKBOX,
                                           Dpi::Scale(24, dpi), Dpi::Scale(72, dpi), Dpi::Scale(320, dpi), Dpi::Scale(30, dpi), hwnd, kCheckStartupId);
        SendMessageW(check, BM_SETCHECK, g_config.startup_enabled ? BST_CHECKED : BST_UNCHECKED, 0);

        CreateSettingsControl(0, L"STATIC", Text().version_label, 0,
                              Dpi::Scale(24, dpi), Dpi::Scale(118, dpi), Dpi::Scale(420, dpi), Dpi::Scale(28, dpi), hwnd);

        CreateSettingsControl(
            0,
            L"STATIC",
            Text().usage_text,
            SS_LEFT,
            Dpi::Scale(24, dpi),
            Dpi::Scale(158, dpi),
            Dpi::Scale(470, dpi),
            Dpi::Scale(170, dpi),
            hwnd);

        CreateSettingsControl(0, L"BUTTON", Text().save, BS_DEFPUSHBUTTON,
                              Dpi::Scale(305, dpi), Dpi::Scale(360, dpi), Dpi::Scale(90, dpi), Dpi::Scale(34, dpi), hwnd, kButtonSaveId);
        CreateSettingsControl(0, L"BUTTON", Text().cancel, 0,
                              Dpi::Scale(410, dpi), Dpi::Scale(360, dpi), Dpi::Scale(90, dpi), Dpi::Scale(34, dpi), hwnd, kButtonCancelId);
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        if (id == kButtonSaveId) {
            wchar_t buffer[32]{};
            GetDlgItemTextW(hwnd, kEditSpeedId, buffer, 32);
            int speed = _wtoi(buffer);
            speed = std::clamp(speed, 100, 3000);
            const bool startup = SendDlgItemMessageW(hwnd, kCheckStartupId, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_config.move_speed_px_per_sec = speed;
            if (ConfigManager::SetStartupEnabled(startup)) {
                g_config.startup_enabled = startup;
            }
            ConfigManager::Save(g_config);
            UpdateTrayIcon();
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == kButtonCancelId) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wparam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DPICHANGED: {
        RECT* suggested = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_DESTROY:
        g_settings_window = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void EnsureSettingsWindow() {
    if (g_settings_window) {
        ShowWindow(g_settings_window, SW_SHOW);
        SetForegroundWindow(g_settings_window);
        return;
    }

    const UINT dpi = Dpi::ForWindowOrSystem(nullptr);
    const int window_width = Dpi::Scale(540, dpi);
    const int window_height = Dpi::Scale(455, dpi);
    g_settings_window = CreateWindowExW(WS_EX_TOOLWINDOW, kSettingsWindowClass, Text().settings_title,
                                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                        CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height,
                                        nullptr, nullptr, g_instance, nullptr);
    if (!g_settings_window) {
        return;
    }

    RECT rect{};
    GetWindowRect(g_settings_window, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int x = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
    const int y = GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2;
    SetWindowPos(g_settings_window, HWND_TOP, x, y, width, height, SWP_SHOWWINDOW);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        g_main_window = hwnd;
        AddTrayIcon();
        if (!RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M')) {
            MessageBoxW(hwnd, L"注册 Ctrl + Alt + M 失败，可能已被其他程序占用。", kAppName, MB_OK | MB_ICONERROR);
            g_startup_failed = true;
            DestroyWindow(hwnd);
            return 0;
        }
        g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, g_instance, 0);
        if (!g_keyboard_hook) {
            MessageBoxW(hwnd, L"安装键盘监听失败。", kAppName, MB_OK | MB_ICONERROR);
            g_startup_failed = true;
            DestroyWindow(hwnd);
            return 0;
        }
        SetTimer(hwnd, kMoveTimerId, 10, nullptr);
        return 0;
    case WM_HOTKEY:
        if (wparam == kHotkeyId) {
            ToggleMouseMode();
        }
        return 0;
    case WM_TIMER:
        if (wparam == kMoveTimerId) {
            TickMove();
        } else if (wparam == kToastTimerId) {
            KillTimer(hwnd, kToastTimerId);
            if (g_toast_window) {
                ShowWindow(g_toast_window, SW_HIDE);
            }
        }
        return 0;
    case kTrayMessage:
        if (LOWORD(lparam) == WM_LBUTTONUP) {
            ToggleMouseMode();
        } else if (LOWORD(lparam) == WM_RBUTTONUP) {
            ShowTrayMenu();
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case kMenuToggle:
            ToggleMouseMode();
            return 0;
        case kMenuSettings:
            EnsureSettingsWindow();
            return 0;
        case kMenuStartup: {
            const bool next = !g_config.startup_enabled;
            if (ConfigManager::SetStartupEnabled(next)) {
                g_config.startup_enabled = next;
                ConfigManager::Save(g_config);
            }
            return 0;
        }
        case kMenuLanguageZhHans:
            SetLanguage(Language::SimplifiedChinese);
            return 0;
        case kMenuLanguageZhHant:
            SetLanguage(Language::TraditionalChinese);
            return 0;
        case kMenuLanguageEn:
            SetLanguage(Language::English);
            return 0;
        case kMenuExit:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_DESTROY:
        SetMouseMode(false, false);
        KillTimer(hwnd, kMoveTimerId);
        KillTimer(hwnd, kToastTimerId);
        if (g_keyboard_hook) {
            UnhookWindowsHookEx(g_keyboard_hook);
            g_keyboard_hook = nullptr;
        }
        UnregisterHotKey(hwnd, kHotkeyId);
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool RegisterWindowClasses() {
    WNDCLASSW main_class{};
    main_class.lpfnWndProc = MainWndProc;
    main_class.hInstance = g_instance;
    main_class.lpszClassName = kMainWindowClass;
    main_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    main_class.hIcon = LoadAppIcon(IDI_APP, GetSystemMetrics(SM_CXICON));
    if (!RegisterClassW(&main_class)) {
        return false;
    }

    WNDCLASSW toast_class{};
    toast_class.lpfnWndProc = ToastWndProc;
    toast_class.hInstance = g_instance;
    toast_class.lpszClassName = kToastWindowClass;
    toast_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&toast_class)) {
        return false;
    }

    WNDCLASSW settings_class{};
    settings_class.lpfnWndProc = SettingsWndProc;
    settings_class.hInstance = g_instance;
    settings_class.lpszClassName = kSettingsWindowClass;
    settings_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settings_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&settings_class)) {
        return false;
    }

    return true;
}

bool IsAlreadyRunning() {
    g_single_instance_mutex = CreateMutexW(nullptr, TRUE, L"KeyboardMouseMode.SingleInstance");
    if (!g_single_instance_mutex) {
        return false;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    g_instance = instance;
    Dpi::EnableAwareness();

    if (IsAlreadyRunning()) {
        MessageBoxW(nullptr, L"键盘鼠标模式已经在运行。", kAppName, MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    g_default_gui_font = Dpi::CreateUiFont(Dpi::ForWindowOrSystem(nullptr));

    g_config = ConfigManager::Load();

    if (!RegisterWindowClasses()) {
        MessageBoxW(nullptr, L"窗口类注册失败。", kAppName, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_main_window = CreateWindowExW(0, kMainWindowClass, kAppName, WS_OVERLAPPED,
                                    0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!g_main_window) {
        MessageBoxW(nullptr, L"主窗口创建失败。", kAppName, MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_single_instance_mutex) {
        CloseHandle(g_single_instance_mutex);
        g_single_instance_mutex = nullptr;
    }

    return static_cast<int>(msg.wParam);
}
