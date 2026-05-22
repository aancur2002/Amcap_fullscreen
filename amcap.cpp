#include "amcap.h"
#include "resource.h"

// DirectShow core execution pointers
IGraphBuilder* g_pGraph = NULL;
ICaptureGraphBuilder2* g_pCapture = NULL;
IVideoWindow* g_pVideoWindow = NULL;
IMediaControl* g_pControl = NULL;
IBaseFilter* g_pVCap = NULL;

HINSTANCE       g_hInstance = NULL;
HWND            g_hwndApp = NULL;
bool            g_bFullScreen = false;
WINDOWPLACEMENT g_wpPrev = { sizeof(WINDOWPLACEMENT) };
HMENU           g_hMainMenu = NULL;

// Device Discovery tracking
IMoniker* g_rgpVideoMonikers[10] = {0};
UINT            g_uVideoCount = 0;
UINT            g_uActiveVideoIdx = 0;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitDirectShow();
void FreeDirectShow();
HRESULT BuildCaptureGraph();
void EnumerateDevices(HWND hwnd);
void ChangeVideoDevice(UINT index);
void ShowFilterProperties(HWND hwnd, IBaseFilter* pFilter);

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
    if (g_pVideoWindow) {
        RECT rc; GetClientRect(hwnd, &rc);
        g_pVideoWindow->SetWindowPosition(0, 0, rc.right, rc.bottom);
    }
}

HRESULT InitDirectShow()
{
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&g_pGraph);
    if (FAILED(hr)) return hr;
    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&g_pCapture);
    if (FAILED(hr)) return hr;
    g_pCapture->SetFiltergraph(g_pGraph);
    g_pGraph->QueryInterface(IID_IMediaControl, (void**)&g_pControl);
    g_pGraph->QueryInterface(IID_IVideoWindow, (void**)&g_pVideoWindow);
    return S_OK;
}

void FreeDirectShow()
{
    if (g_pControl) g_pControl->Stop();
    if (g_pVideoWindow) {
        g_pVideoWindow->put_Visible(OAFALSE);
        g_pVideoWindow->put_Owner(NULL);
    }
    SAFE_RELEASE(g_pVideoWindow);
    SAFE_RELEASE(g_pControl);
    SAFE_RELEASE(g_pVCap);
    SAFE_RELEASE(g_pCapture);
    SAFE_RELEASE(g_pGraph);
    for(UINT i=0; i<10; i++) {
        SAFE_RELEASE(g_rgpVideoMonikers[i]);
    }
}

void EnumerateDevices(HWND hwnd)
{
    ICreateDevEnum* pDevEnum = NULL;
    IEnumMoniker* pEnum = NULL;
    CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pDevEnum);
    
    if (pDevEnum && pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0) == S_OK) {
        HMENU hMenu = GetMenu(hwnd);
        HMENU hDevMenu = GetSubMenu(hMenu, 1); // Devices Popup index
        if (hDevMenu) {
            DeleteMenu(hDevMenu, 0, MF_BYPOSITION);
            g_uVideoCount = 0;
            IMoniker* pMoniker = NULL;
            while (pEnum->Next(1, &pMoniker, NULL) == S_OK && g_uVideoCount < 10) {
                IPropertyBag* pPropBag = NULL;
                pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
                if (pPropBag) {
                    VARIANT var; VariantInit(&var);
                    pPropBag->Read(L"FriendlyName", &var, 0);
                    AppendMenuW(hDevMenu, MF_STRING | MF_UNCHECKED, MENU_DEVICE_START + g_uVideoCount, var.bstrVal);
                    VariantClear(&var);
                    pPropBag->Release();
                }
                g_rgpVideoMonikers[g_uVideoCount] = pMoniker;
                g_uVideoCount++;
            }
        }
        pEnum->Release();
    }
    if (pDevEnum) pDevEnum->Release();
}

void ChangeVideoDevice(UINT index)
{
    if (index >= g_uVideoCount) return;
    if (g_pControl) g_pControl->Stop();
    if (g_pVCap) { g_pGraph->RemoveFilter(g_pVCap); SAFE_RELEASE(g_pVCap); }
    
    HRESULT hr = g_rgpVideoMonikers[index]->BindToObject(0, 0, IID_IBaseFilter, (void**)&g_pVCap);
    if (SUCCEEDED(hr)) {
        g_pGraph->AddFilter(g_pVCap, L"Video Capture Source");
        g_uActiveVideoIdx = index;
        BuildCaptureGraph();
    }
}

HRESULT BuildCaptureGraph()
{
    if (!g_pVCap) return E_FAIL;
    HRESULT hr = g_pCapture->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, g_pVCap, NULL, NULL);
    if (g_pVideoWindow) {
        g_pVideoWindow->put_Owner((OAHWND)g_hwndApp);
        g_pVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);
        RECT rc; GetClientRect(g_hwndApp, &rc);
        g_pVideoWindow->SetWindowPosition(0, 0, rc.right, rc.bottom);
        g_pVideoWindow->put_Visible(OATRUE);
    }
    if (g_pControl) g_pControl->Run();
    return hr;
}

void ShowFilterProperties(HWND hwnd, IBaseFilter* pFilter)
{
    if (!pFilter) return;
    ISpecifyPropertyPages* pSpecify = NULL;
    pFilter->QueryInterface(IID_ISpecifyPropertyPages, (void**)&pSpecify);
    if (pSpecify) {
        CAUUID caGUID; pSpecify->GetPages(&caGUID);
        pSpecify->Release();
        OleCreatePropertyFrame(hwnd, 0, 0, L"Device Properties", 1, (IUnknown**)&pFilter, caGUID.cElems, caGUID.pElems, 0, 0, NULL);
        CoTaskMemFree(caGUID.pElems);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_LBUTTONDBLCLK:
            ToggleFullScreen(hwnd);
            return 0;
            
        case WM_SIZE:
            if (g_pVideoWindow && !g_bFullScreen) {
                g_pVideoWindow->SetWindowPosition(0, 0, LOWORD(lParam), HIWORD(lParam));
            }
            break;

        case WM_COMMAND:
            {
                UINT id = LOWORD(wParam);
                if (id >= MENU_DEVICE_START && id < MENU_DEVICE_START + 10) {
                    ChangeVideoDevice(id - MENU_DEVICE_START);
                    return 0;
                }
                switch (id)
                {
                    case ID_VIEW_FULLSCREEN:
                        ToggleFullScreen(hwnd);
                        return 0;
                    case ID_OPTIONS_DEVICEPROP:
                        ShowFilterProperties(hwnd, g_pVCap);
                        return 0;
                    case ID_OPTIONS_PINPROP:
                        {
                            IAMStreamConfig* pSC = NULL;
                            g_pCapture->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, g_pVCap, IID_IAMStreamConfig, (void**)&pSC);
                            if(pSC) {
                                ShowFilterProperties(hwnd, (IBaseFilter*)pSC);
                                pSC->Release();
                            }
                        }
                        return 0;
                    case ID_FILE_EXIT:
                        DestroyWindow(hwnd);
                        return 0;
                }
            }
            break;

        case WM_KEYDOWN:
            if ((wParam == VK_ESCAPE || wParam == VK_RETURN) && g_bFullScreen) {
                ToggleFullScreen(hwnd);
                return 0;
            }
            break;

        case WM_DESTROY:
            FreeDirectShow();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    const wchar_t szClassName[] = L"AMCAP";
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; 
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU);
    wc.lpszClassName = szClassName;

    RegisterClassEx(&wc);

    g_hwndApp = CreateWindowEx(0, szClassName, L"Professional Hardware KVM Console", 
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, 
                               NULL, NULL, hInstance, NULL);

    ShowWindow(g_hwndApp, nCmdShow);
    UpdateWindow(g_hwndApp);
    
    if (SUCCEEDED(InitDirectShow())) {
        EnumerateDevices(g_hwndApp);
        if (g_uVideoCount > 0) {
            ChangeVideoDevice(0); 
        }
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    CoUninitialize();
    return (int)msg.wParam;
}
