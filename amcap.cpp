#include "amcap.h"
#include "resource.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <d3d9.h>

// Link necessary multimedia libraries automatically
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

HINSTANCE       g_hInstance = NULL;
HWND            g_hwndApp = NULL;
bool            g_bFullScreen = false;
WINDOWPLACEMENT g_wpPrev = { sizeof(WINDOWPLACEMENT) };
HMENU           g_hMainMenu = NULL;

// Media Foundation capture variables
IMFMediaSource* g_pSource = NULL;
IMFSourceReader* g_pReader = NULL;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ToggleFullScreen(HWND hwnd);
HRESULT InitializeVideoCapture(HWND hwnd);
void CloseVideoCapture();

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

        SetWindowPos(hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        g_bFullScreen = true;
    } 
    else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
        
        if (g_hMainMenu != NULL) {
            SetMenu(hwnd, g_hMainMenu);
        }

        SetWindowPlacement(hwnd, &g_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        g_bFullScreen = false;
    }
}

// Automatically look up your USB capture card and start the stream
HRESULT InitializeVideoCapture(HWND hwnd)
{
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return hr;

    IMFAttributes* pAttributes = NULL;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) { pAttributes->Release(); return hr; }

    IMFActivate** ppDevices = NULL;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->Release();
    if (FAILED(hr)) return hr;

    if (count == 0) {
        // No capture card found yet
        return E_FAIL;
    }

    // Default to the first video capture hardware device found (your USB capture card)
    hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&g_pSource));
    
    for (UINT32 i = 0; i < count; i++) {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    if (FAILED(hr)) return hr;

    IMFAttributes* pReaderAttributes = NULL;
    hr = MFCreateAttributes(&pReaderAttributes, 2);
    if (SUCCEEDED(hr)) {
        pReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        // Tell it to render the output straight onto our app window frame
        pReaderAttributes->SetUNKNOWN(MF_SOURCE_READER_ASYNC_CALLBACK, NULL);
        hr = MFCreateSourceReaderFromMediaSource(g_pSource, pReaderAttributes, &g_pReader);
        pReaderAttributes->Release();
    }

    return hr;
}

void CloseVideoCapture()
{
    if (g_pReader) { g_pReader->Release(); g_pReader = NULL; }
    if (g_pSource) { g_pSource->Shutdown(); g_pSource->Release(); g_pSource = NULL; }
    MFShutdown();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_LBUTTONDBLCLK:
            ToggleFullScreen(hwnd);
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case ID_VIEW_FULLSCREEN:
                    ToggleFullScreen(hwnd);
                    return 0;

                case ID_FILE_EXIT:
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && g_bFullScreen) {
                ToggleFullScreen(hwnd);
                return 0;
            }
            break;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                // If video hasn't loaded or target is disconnected, display status message text
                if (!g_pReader) {
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    SetTextColor(hdc, RGB(128, 128, 128));
                    SetBkMode(hdc, TRANSPARENT);
                    DrawTextW(hdc, L"KVM Signal Offline - Check Video USB Capture Card Connection", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                } else {
                    // Let Windows Media Foundation draw the video payload directly here
                    IMFSample* pSample = NULL;
                    DWORD streamIndex, flags;
                    LONGLONG timestamp;
                    g_pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
                    if (pSample) { pSample->Release(); }
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                
                EndPaint(hwnd, &ps);
            }
            return 0;

        case WM_DESTROY:
            CloseVideoCapture();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;
    const wchar_t szClassName[] = L"AMCAP_WINDOW";

    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; 
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Set background to clean black
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU);
    wc.lpszClassName = szClassName;

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    g_hwndApp = CreateWindowEx(
        0,
        szClassName,
        L"NextCore", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, // Upgraded standard resolution block
        NULL, NULL, hInstance, NULL);

    if (g_hwndApp == NULL) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Try starting the physical video pipeline stream hook
    InitializeVideoCapture(g_hwndApp);

    ShowWindow(g_hwndApp, nCmdShow);
    UpdateWindow(g_hwndApp);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
