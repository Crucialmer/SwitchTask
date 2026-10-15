#include "utils/setWindowBlur.h"
#include <dwmapi.h>

void setWindowBlur(HWND hWnd) {
    // 1. 优先尝试 Windows 11 (22H2+) 官方硬件加速瞬态亚克力材质 (DWMSBT_TRANSIENTWINDOW)
    // 专为切换器 HUD 与上下文浮窗优化，具备平滑高帧率与通透折射感
    bool appliedModern = false;
    HMODULE hDwm = GetModuleHandleW(L"dwmapi.dll");
    if (hDwm) {
        auto pfnDwmSetWindowAttribute = (decltype(&DwmSetWindowAttribute)) GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pfnDwmSetWindowAttribute) {
            constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE_VAL = 38;
            constexpr DWORD DWMSBT_TRANSIENTWINDOW_VAL = 3; // Acrylic for transient popups
            DWORD backdrop = DWMSBT_TRANSIENTWINDOW_VAL;
            if (SUCCEEDED(pfnDwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE_VAL, &backdrop, sizeof(backdrop)))) {
                appliedModern = true;
            }
        }
    }

    // 2. Windows 10 回退：SetWindowCompositionAttribute (BLURBEHIND)
    if (!appliedModern) {
        HMODULE hUser = GetModuleHandleW(L"user32.dll");
        if (hUser) {
            auto setWindowCompositionAttribute = (pfnSetWindowCompositionAttribute) GetProcAddress(hUser, "SetWindowCompositionAttribute");
            if (setWindowCompositionAttribute) {
                ACCENT_POLICY accent = {ACCENT_ENABLE_BLURBEHIND, 0, 0, 0};
                WINDOWCOMPOSITIONATTRIBDATA data;
                data.Attrib = WCA_ACCENT_POLICY;
                data.pvData = &accent;
                data.cbData = sizeof(accent);
                setWindowCompositionAttribute(hWnd, &data);
            }
        }
    }
}

