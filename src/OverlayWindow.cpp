#include "OverlayWindow.h"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>
#include <cstdio>

// ============================================================================
// D2D / DWrite state — all accessed on the overlay thread only
// ============================================================================

static ID2D1Factory*          g_d2dFactory = nullptr;
static IDWriteFactory*        g_dwFactory  = nullptr;
static IDWriteTextFormat*     g_textFmt    = nullptr;
static ID2D1HwndRenderTarget* g_rt         = nullptr;
static ID2D1SolidColorBrush*  g_textBrush  = nullptr;
static ID2D1SolidColorBrush*  g_scanBrush  = nullptr;

static volatile HWND g_hwnd   = nullptr;
static HANDLE        g_thread = nullptr;
static HANDLE        g_ready  = nullptr;
static int           g_tick   = 0;

// ============================================================================

template <class T>
static inline void SafeRelease(T** pp) noexcept
{ if (*pp) { (*pp)->Release(); *pp = nullptr; } }

static HRESULT CreateDeviceResources(HWND hwnd) noexcept
{
    if (g_rt) return S_OK;

    RECT rc{};
    GetClientRect(hwnd, &rc);

    HRESULT hr = g_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(rc.right, rc.bottom)),
        &g_rt);
    if (FAILED(hr)) return hr;

    hr = g_rt->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 0.25f), &g_textBrush);
    if (FAILED(hr)) return hr;

    // Semi-transparent black scanlines drawn over everything
    return g_rt->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.45f), &g_scanBrush);
}

static void DiscardDeviceResources() noexcept
{
    SafeRelease(&g_scanBrush);
    SafeRelease(&g_textBrush);
    SafeRelease(&g_rt);
}

// ============================================================================
// Render one frame — called from WM_PAINT on the overlay thread
// ============================================================================

static void RenderFrame(HWND hwnd) noexcept
{
    if (!g_rt && FAILED(CreateDeviceResources(hwnd))) return;

    // Animated dots: 0..3, step every 4 ticks × 150 ms = 600 ms per step
    static const wchar_t* kDots[] = { L"", L".", L"..", L"..." };
    const int dotIdx = (g_tick / 4) % 4;

    // Green pulse: full sine cycle every 40 ticks × 150 ms ≈ 6 s
    const float phase  = static_cast<float>(g_tick % 40) / 40.f;
    const float bright = 0.50f + 0.50f * sinf(phase * 6.28318f);

    const D2D1_SIZE_F sz = g_rt->GetSize();

    g_rt->BeginDraw();
    g_rt->Clear(D2D1::ColorF(0.f, 0.f, 0.f));

    g_textBrush->SetColor(D2D1::ColorF(0.f, bright * 0.90f, bright * 0.20f));

    wchar_t buf[32];
    swprintf_s(buf, L"PLEASE WAIT%s", kDots[dotIdx]);

    g_rt->DrawText(buf, static_cast<UINT32>(wcslen(buf)), g_textFmt,
                   D2D1::RectF(0.f, 0.f, sz.width, sz.height), g_textBrush);

    // CRT scanlines: 1.5px dark horizontal line every 3 pixels
    for (float y = 0.f; y < sz.height; y += 3.f)
        g_rt->DrawLine(D2D1::Point2F(0.f, y), D2D1::Point2F(sz.width, y),
                       g_scanBrush, 1.5f);

    if (g_rt->EndDraw() == D2DERR_RECREATE_TARGET)
        DiscardDeviceResources();
}

// ============================================================================
// Window procedure
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        ++g_tick;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        RenderFrame(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        if (g_rt && LOWORD(lp) > 0 && HIWORD(lp) > 0)
            g_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_CLOSE:
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ============================================================================
// Overlay thread — owns all D2D objects and the message loop
// ============================================================================

static DWORD WINAPI OverlayThread(LPVOID) noexcept
{
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory)) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&g_dwFactory))) ||
        FAILED(g_dwFactory->CreateTextFormat(
                   L"Consolas", nullptr,
                   DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                   DWRITE_FONT_STRETCH_NORMAL, 80.f, L"en-us", &g_textFmt)))
    {
        SetEvent(g_ready);
        return 1;
    }

    g_textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_textFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = L"WDCOverlay";
    RegisterClassExW(&wc);

    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"WDCOverlay", nullptr, WS_POPUP,
        0, 0, sw, sh, nullptr, nullptr, hInst, nullptr);

    if (!hwnd) { SetEvent(g_ready); return 1; }

    SetLayeredWindowAttributes(hwnd, 0, 245, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetTimer(hwnd, 1, 150, nullptr);

    g_hwnd = hwnd;
    SetEvent(g_ready);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DiscardDeviceResources();
    SafeRelease(&g_textFmt);
    SafeRelease(&g_dwFactory);
    SafeRelease(&g_d2dFactory);

    g_hwnd = nullptr;
    UnregisterClassW(L"WDCOverlay", hInst);
    return 0;
}

// ============================================================================
// Public API
// ============================================================================

bool OverlayWindow::Show() noexcept
{
    if (g_hwnd) return true;

    g_tick  = 0;
    g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_ready) return false;

    g_thread = CreateThread(nullptr, 0, OverlayThread, nullptr, 0, nullptr);
    if (!g_thread) { CloseHandle(g_ready); g_ready = nullptr; return false; }

    WaitForSingleObject(g_ready, 2000);
    CloseHandle(g_ready);
    g_ready = nullptr;
    return g_hwnd != nullptr;
}

void OverlayWindow::Hide() noexcept
{
    const HWND hwnd = g_hwnd;
    if (hwnd) PostMessageW(hwnd, WM_CLOSE, 0, 0);

    if (g_thread) {
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}
