#include "utils/setWindowBlur.h"

void setWindowBlur(HWND hWnd) {
    // 采用 SetWindowCompositionAttribute(ACCENT_ENABLE_BLURBEHIND) 磨砂模糊：
    // DWM 仅严格对 Qt 实际绘制的非零 alpha 像素施加模糊，窗口外围透明区域彻底透穿，
    // 杜绝 Windows 11 DWMWA_SYSTEMBACKDROP 在宿主外接矩形绘制多余独立透明底框的问题。
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

