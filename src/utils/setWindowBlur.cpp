#include "utils/setWindowBlur.h"

void setWindowBlur(HWND hWnd) {
    HMODULE hUser = GetModuleHandle(L"user32.dll");
    if (hUser) {
        auto setWindowCompositionAttribute = (pfnSetWindowCompositionAttribute) GetProcAddress(hUser, "SetWindowCompositionAttribute");
        if (setWindowCompositionAttribute) {
            // 磨砂模糊（BLURBEHIND）+ WA_TranslucentBackground = 毛玻璃观感（真机模糊生效；
            // RDP/虚拟机等合成环境由系统降级为纯半透明，面板填充的 alpha 保证观感依然通透）。
            // 注：曾试验 ACCENT_ENABLE_ACRYLICBLURBEHIND（亚克力，配第三参数染色）——2026-10-14 实测
            // 其在部分合成环境会被系统渲染为纯色实心层（忽略染色 alpha），反而遮挡通透感，故弃用。
            ACCENT_POLICY accent = {ACCENT_ENABLE_BLURBEHIND, 0, 0, 0};
            WINDOWCOMPOSITIONATTRIBDATA data;
            data.Attrib = WCA_ACCENT_POLICY;
            data.pvData = &accent;
            data.cbData = sizeof(accent);
            setWindowCompositionAttribute(hWnd, &data);
        }
    }
}
