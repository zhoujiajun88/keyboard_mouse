#pragma once

#include <windows.h>

class Dpi {
public:
    static void EnableAwareness();
    static UINT ForWindowOrSystem(HWND hwnd);
    static int Scale(int value, UINT dpi);
    static HFONT CreateUiFont(UINT dpi);
};
