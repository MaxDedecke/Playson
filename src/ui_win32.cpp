#include "ui_win32.h"
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace playson {

static ULONG_PTR g_gdiplusToken = 0;
static int g_instanceCount = 0;

static const wchar_t* WINDOW_CLASS_NAME = L"PlaysonPluginWindowClass";

Win32Gui::Win32Gui() {
    if (g_instanceCount == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    }
    g_instanceCount++;
}

Win32Gui::~Win32Gui() {
    destroy();
    g_instanceCount--;
    if (g_instanceCount == 0 && g_gdiplusToken != 0) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

bool Win32Gui::create(HWND parent, uint32_t width, uint32_t height) {
    m_parent = parent;
    m_width = width;
    m_height = height;

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"Playson Sonos Bridge",
        WS_CHILD | WS_VISIBLE,
        0, 0, width, height,
        parent,
        NULL,
        hInstance,
        this
    );

    return m_hwnd != NULL;
}

void Win32Gui::destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
}

void Win32Gui::setVisible(bool visible) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

void Win32Gui::setStatus(PluginState state, const std::string& message) {
    m_state = state;
    m_statusMessage = message;
    if (m_hwnd) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void Win32Gui::setRecordedSeconds(float seconds) {
    m_recordedSeconds = seconds;
    if (m_hwnd && m_state == PluginState::RECORDING) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

LRESULT CALLBACK Win32Gui::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32Gui* pThis = (Win32Gui*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent flicker

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pThis) {
                pThis->paint(hdc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!pThis) return 0;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Button area: x in [30, m_width - 30], y in [95, 155]
            bool inside = (x >= 30 && x <= (int)(pThis->m_width - 30) && y >= 95 && y <= 155);
            if (inside != pThis->m_btnHovered) {
                pThis->m_btnHovered = inside;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (pThis && pThis->m_btnHovered) {
                pThis->m_btnHovered = false;
                pThis->m_btnPressed = false;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (!pThis) return 0;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (x >= 30 && x <= (int)(pThis->m_width - 30) && y >= 95 && y <= 155) {
                pThis->m_btnPressed = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (!pThis) return 0;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (pThis->m_btnPressed) {
                pThis->m_btnPressed = false;
                InvalidateRect(hwnd, NULL, FALSE);
                if (x >= 30 && x <= (int)(pThis->m_width - 30) && y >= 95 && y <= 155) {
                    if (pThis->onTriggerBounce) {
                        pThis->onTriggerBounce();
                    }
                }
            }
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void Win32Gui::paint(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // Double-buffered drawing
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // 1. Dark background
    SolidBrush bgBrush(Color(255, 21, 22, 28)); // #15161c
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Subtle border
    Pen borderPen(Color(40, 255, 255, 255), 1.0f);
    g.DrawRectangle(&borderPen, 0, 0, width - 1, height - 1);

    // 2. Header Brand
    FontFamily fontFamily(L"Segoe UI");
    Font fontTitle(&fontFamily, 13, FontStyleBold, UnitPixel);
    Font fontSubtitle(&fontFamily, 10, FontStyleRegular, UnitPixel);
    Font fontButton(&fontFamily, 13, FontStyleBold, UnitPixel);
    Font fontStatus(&fontFamily, 10, FontStyleRegular, UnitPixel);

    SolidBrush whiteBrush(Color(255, 240, 241, 245));
    SolidBrush grayBrush(Color(255, 145, 150, 160));
    SolidBrush orangeBrush(Color(255, 255, 92, 0));

    g.DrawString(L"PLAYSON", -1, &fontTitle, PointF(24.0f, 20.0f), &whiteBrush);
    g.DrawString(L"•  SONOS MASTER BRIDGE", -1, &fontSubtitle, PointF(86.0f, 22.0f), &orangeBrush);

    // 3. Status Badge with Indicator Dot
    Color dotColor = Color(255, 16, 185, 129); // Green by default
    std::wstring statusTextW;

    if (m_state == PluginState::RECORDING) {
        dotColor = Color(255, 239, 68, 68); // Red
        std::wstringstream ss;
        ss << L"Aufnahme laeuft (" << std::fixed << std::setprecision(1) << m_recordedSeconds << L"s)...";
        statusTextW = ss.str();
    } else if (m_state == PluginState::WAITING_FOR_LOOP_START) {
        dotColor = Color(255, 245, 158, 11); // Amber
        statusTextW = L"Warte auf Loop-Start / Play...";
    } else if (m_state == PluginState::UPLOADING) {
        dotColor = Color(255, 255, 92, 0); // Orange
        statusTextW = L"Sende an Sonos Speaker...";
    } else if (m_state == PluginState::SUCCESS) {
        dotColor = Color(255, 16, 185, 129); // Green
        statusTextW = L"Erfolgreich abgespielt!";
    } else {
        std::wstring msg(m_statusMessage.begin(), m_statusMessage.end());
        statusTextW = msg;
    }

    SolidBrush dotBrush(dotColor);
    g.FillEllipse(&dotBrush, 24, 52, 8, 8);
    g.DrawString(statusTextW.c_str(), -1, &fontStatus, PointF(38.0f, 49.0f), &grayBrush);

    // 4. Main Action Button: "BOUNCE TO SONOS"
    int btnX = 24;
    int btnY = 82;
    int btnW = width - 48;
    int btnH = 58;

    Color btnColorTop = Color(255, 255, 92, 0);
    Color btnColorBottom = Color(255, 235, 75, 0);

    if (m_state == PluginState::RECORDING) {
        btnColorTop = Color(255, 220, 38, 38);
        btnColorBottom = Color(255, 185, 28, 28);
    } else if (m_btnPressed) {
        btnColorTop = Color(255, 215, 70, 0);
        btnColorBottom = Color(255, 195, 55, 0);
    } else if (m_btnHovered) {
        btnColorTop = Color(255, 255, 115, 30);
        btnColorBottom = Color(255, 255, 92, 0);
    }

    // Button background with rounded corners
    GraphicsPath btnPath;
    int radius = 10;
    btnPath.AddArc(btnX, btnY, radius * 2, radius * 2, 180, 90);
    btnPath.AddArc(btnX + btnW - radius * 2, btnY, radius * 2, radius * 2, 270, 90);
    btnPath.AddArc(btnX + btnW - radius * 2, btnY + btnH - radius * 2, radius * 2, radius * 2, 0, 90);
    btnPath.AddArc(btnX, btnY + btnH - radius * 2, radius * 2, radius * 2, 90, 90);
    btnPath.CloseFigure();

    LinearGradientBrush btnGradient(Point(btnX, btnY), Point(btnX, btnY + btnH), btnColorTop, btnColorBottom);
    g.FillPath(&btnGradient, &btnPath);

    // Button label
    std::wstring btnLabel = (m_state == PluginState::RECORDING) ? L"■  STOP & SENDEN" : L"▶  BOUNCE TO SONOS";
    if (m_state == PluginState::WAITING_FOR_LOOP_START) {
        btnLabel = L"WAITING FOR PLAY...";
    } else if (m_state == PluginState::UPLOADING) {
        btnLabel = L"STREAMING...";
    }

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF btnRectF((float)btnX, (float)btnY, (float)btnW, (float)btnH);
    g.DrawString(btnLabel.c_str(), -1, &fontButton, btnRectF, &sf, &whiteBrush);

    // 5. Footer info
    Font fontFooter(&fontFamily, 9, FontStyleRegular, UnitPixel);
    SolidBrush mutedBrush(Color(255, 95, 100, 110));
    g.DrawString(L"Ziel: Sonos Play:1 (Buero) • 1-Click Stream", -1, &fontFooter, PointF(24.0f, (float)(height - 24)), &mutedBrush);

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

} // namespace playson
