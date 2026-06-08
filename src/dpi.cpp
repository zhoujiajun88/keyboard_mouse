#include "dpi.h"

void Dpi::EnableAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto set_context = reinterpret_cast<SetDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_context && set_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }
    SetProcessDPIAware();
}

UINT Dpi::ForWindowOrSystem(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto get_dpi_for_window = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (get_dpi_for_window && hwnd) {
            return get_dpi_for_window(hwnd);
        }

        using GetDpiForSystemFn = UINT(WINAPI*)();
        auto get_dpi_for_system = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(user32, "GetDpiForSystem"));
        if (get_dpi_for_system) {
            return get_dpi_for_system();
        }
    }
    return 96;
}

int Dpi::Scale(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

HFONT Dpi::CreateUiFont(UINT dpi) {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0)) {
        metrics.lfMessageFont.lfHeight = -MulDiv(11, static_cast<int>(dpi), 72);
        metrics.lfMessageFont.lfWeight = FW_MEDIUM;
        return CreateFontIndirectW(&metrics.lfMessageFont);
    }
    return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}
