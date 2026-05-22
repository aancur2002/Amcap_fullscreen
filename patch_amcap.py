import os

cpp_path = r"Samples/Win7Samples/multimedia/directshow/capture/amcap/amcap.cpp"

if os.path.exists(cpp_path):
    with open(cpp_path, "r", encoding="utf-8", errors="ignore") as f:
        code = f.read()

    # 1. Inject Globals at the top
    globals_code = """
// --- KVM Fullscreen Extension Globals ---
bool            g_bFullScreen = false;
WINDOWPLACEMENT g_wpPrev = { sizeof(WINDOWPLACEMENT) };
HMENU           g_hMainMenu = NULL;

void ToggleFullScreen(HWND hwnd)
{
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (!g_bFullScreen) {
        GetWindowPlacement(hwnd, &g_wpPrev);
        HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        GetMonitorInfo(hMonitor, &mi);
        g_hMainMenu = GetMenu(hwnd);
        SetMenu(hwnd, NULL);
        SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_bFullScreen = true;
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
        if (g_hMainMenu) SetMenu(hwnd, g_hMainMenu);
        SetWindowPlacement(hwnd, &g_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_bFullScreen = false;
    }
    extern IVideoWindow *g_pVW;
    if (g_pVW) {
        RECT rc; GetClientRect(hwnd, &rc);
        g_pVW->SetWindowPosition(0, 0, rc.right, rc.bottom);
    }
}
"""
    code = code.replace("#include <streams.h>", "#include <streams.h>\n" + globals_code)

    # 2. Add Double-Click and Escape inputs into the main window router (WndProc)
    input_hooks = """    switch (message)
    {
        case WM_LBUTTONDBLCLK:
            ToggleFullScreen(hwnd);
            return 0;
        case WM_KEYDOWN:
            if ((wParam == VK_ESCAPE || wParam == VK_RETURN) && g_bFullScreen) {
                ToggleFullScreen(hwnd);
                return 0;
            }
            break;"""
    code = code.replace("    switch (message)\n    {", input_hooks)

    # 3. Allow Windows to accept high-speed mouse double clicks
    code = code.replace("wc.style = 0;", "wc.style = CS_DBLCLKS;")

    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write(code)
    print("[SUCCESS] Microsoft amcap.cpp patched with Fullscreen capabilities.")
else:
    print("[ERROR] Could not find the target amcap.cpp file structure.")
