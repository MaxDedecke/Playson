#include "ui_win32.h"
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

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

    HINSTANCE hInstance = GetModuleHandleW(NULL);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
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

    if (m_hwnd) {
        SetTimer(m_hwnd, 1001, 33, NULL); // ~30 fps animation timer
    }

    return m_hwnd != NULL;
}

void Win32Gui::destroy() {
    if (m_hwnd) {
        KillTimer(m_hwnd, 1001);
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
    if (state == PluginState::SUCCESS) {
        m_successCountdown = 0;
    }
    if (m_hwnd) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void Win32Gui::setProgress(float progress, float currentSec, float targetSec) {
    m_progress = progress;
    m_recordedSeconds = currentSec;
    m_targetSeconds = targetSec;
    if (m_hwnd) {
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

        case WM_TIMER: {
            if (pThis && wParam == 1001) {
                pThis->m_animPhase += 0.08f;

                if (pThis->m_state == PluginState::RECORDING || pThis->m_state == PluginState::UPLOADING) {
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (pThis->m_state == PluginState::SUCCESS) {
                    pThis->m_successCountdown++;
                    if (pThis->m_successCountdown > 120) { // ~4 seconds
                        pThis->m_state = PluginState::IDLE;
                        pThis->m_statusMessage = "Bereit zum Abspielen auf Sonos";
                        pThis->m_successCountdown = 0;
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
            }
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

            // Main Button area: Y in [110, 180], X in [24, m_width - 24]
            bool insideBtn = (x >= 24 && x <= (int)(pThis->m_width - 24) && y >= 110 && y <= 180);
            if (insideBtn != pThis->m_btnHovered) {
                pThis->m_btnHovered = insideBtn;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            // Pills area: Y in [72, 98]
            int hoveredPill = -1;
            if (y >= 72 && y <= 98) {
                if (x >= 24 && x <= 96) hoveredPill = 0;
                else if (x >= 104 && x <= 176) hoveredPill = 1;
                else if (x >= 184 && x <= 256) hoveredPill = 2;
                else if (x >= 264 && x <= 336) hoveredPill = 3;
            }
            if (hoveredPill != pThis->m_hoveredPill) {
                pThis->m_hoveredPill = hoveredPill;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (pThis) {
                pThis->m_btnHovered = false;
                pThis->m_btnPressed = false;
                pThis->m_hoveredPill = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (!pThis) return 0;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Click Pill
            if (y >= 72 && y <= 98 && pThis->m_state != PluginState::RECORDING && pThis->m_state != PluginState::UPLOADING) {
                if (x >= 24 && x <= 96) {
                    pThis->m_mode = CaptureMode::BARS_4;
                    if (pThis->onModeChanged) pThis->onModeChanged(CaptureMode::BARS_4);
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (x >= 104 && x <= 176) {
                    pThis->m_mode = CaptureMode::BARS_8;
                    if (pThis->onModeChanged) pThis->onModeChanged(CaptureMode::BARS_8);
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (x >= 184 && x <= 256) {
                    pThis->m_mode = CaptureMode::BARS_16;
                    if (pThis->onModeChanged) pThis->onModeChanged(CaptureMode::BARS_16);
                    InvalidateRect(hwnd, NULL, FALSE);
                } else if (x >= 264 && x <= 336) {
                    pThis->m_mode = CaptureMode::TIME_30S;
                    if (pThis->onModeChanged) pThis->onModeChanged(CaptureMode::TIME_30S);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }

            // Click Main Button
            if (x >= 24 && x <= (int)(pThis->m_width - 24) && y >= 110 && y <= 180) {
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
                if (x >= 24 && x <= (int)(pThis->m_width - 24) && y >= 110 && y <= 180) {
                    if (pThis->onTriggerBounce) {
                        pThis->onTriggerBounce();
                    }
                }
            }
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
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
    SolidBrush bgBrush(Color(255, 18, 19, 24)); // #121318
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Subtle border
    Pen borderPen(Color(35, 255, 255, 255), 1.0f);
    g.DrawRectangle(&borderPen, 0, 0, width - 1, height - 1);

    // 2. Header Brand
    FontFamily fontFamily(L"Segoe UI");
    Font fontTitle(&fontFamily, 13, FontStyleBold, UnitPixel);
    Font fontSubtitle(&fontFamily, 10, FontStyleRegular, UnitPixel);
    Font fontPill(&fontFamily, 10, FontStyleBold, UnitPixel);
    Font fontBtnMain(&fontFamily, 13, FontStyleBold, UnitPixel);
    Font fontBtnSub(&fontFamily, 9, FontStyleRegular, UnitPixel);
    Font fontStatus(&fontFamily, 10, FontStyleRegular, UnitPixel);
    Font fontFooter(&fontFamily, 9, FontStyleRegular, UnitPixel);

    SolidBrush whiteBrush(Color(255, 240, 241, 245));
    SolidBrush grayBrush(Color(255, 145, 150, 160));
    SolidBrush orangeBrush(Color(255, 255, 92, 0));
    SolidBrush mutedBrush(Color(255, 95, 100, 110));

    g.DrawString(L"PLAYSON", -1, &fontTitle, PointF(24.0f, 18.0f), &whiteBrush);
    g.DrawString(L"•  SONOS MASTER BRIDGE", -1, &fontSubtitle, PointF(86.0f, 20.0f), &orangeBrush);

    // 3. Status Badge with Indicator Dot
    Color dotColor = Color(255, 16, 185, 129); // Green
    std::wstring statusTextW;

    if (m_state == PluginState::RECORDING) {
        dotColor = Color(255, 239, 68, 68); // Red
        std::wstringstream ss;
        ss << L"Aufnahme laeuft (" << std::fixed << std::setprecision(1) << m_recordedSeconds << L"s / " 
           << std::fixed << std::setprecision(1) << m_targetSeconds << L"s)...";
        statusTextW = ss.str();
    } else if (m_state == PluginState::UPLOADING) {
        dotColor = Color(255, 255, 92, 0); // Orange
        statusTextW = L"Sende Snapshot an Sonos Play:1...";
    } else if (m_state == PluginState::SUCCESS) {
        dotColor = Color(255, 16, 185, 129); // Green
        statusTextW = L"Wird jetzt auf Sonos Lautsprecher abgespielt!";
    } else {
        std::wstring msg(m_statusMessage.begin(), m_statusMessage.end());
        statusTextW = msg;
    }

    SolidBrush dotBrush(dotColor);
    g.FillEllipse(&dotBrush, 24, 46, 8, 8);
    g.DrawString(statusTextW.c_str(), -1, &fontStatus, PointF(38.0f, 43.0f), &grayBrush);

    // 4. Mode Selector Pills: [4 Takte] [8 Takte] [16 Takte] [30s]
    const wchar_t* pillLabels[] = { L"4 Takte", L"8 Takte", L"16 Takte", L"30s" };
    CaptureMode pillModes[] = { CaptureMode::BARS_4, CaptureMode::BARS_8, CaptureMode::BARS_16, CaptureMode::TIME_30S };
    int pillW = 72;
    int pillH = 26;
    int pillY = 72;
    int pillSpacing = 8;

    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);

    for (int i = 0; i < 4; ++i) {
        int px = 24 + i * (pillW + pillSpacing);
        bool isSelected = (m_mode == pillModes[i]);
        bool isHovered = (m_hoveredPill == i);

        GraphicsPath pPath;
        int r = 5;
        pPath.AddArc(px, pillY, r * 2, r * 2, 180, 90);
        pPath.AddArc(px + pillW - r * 2, pillY, r * 2, r * 2, 270, 90);
        pPath.AddArc(px + pillW - r * 2, pillY + pillH - r * 2, r * 2, r * 2, 0, 90);
        pPath.AddArc(px, pillY + pillH - r * 2, r * 2, r * 2, 90, 90);
        pPath.CloseFigure();

        if (isSelected) {
            SolidBrush selBrush(Color(255, 255, 92, 0));
            g.FillPath(&selBrush, &pPath);
            RectF pillRect((float)px, (float)pillY, (float)pillW, (float)pillH);
            g.DrawString(pillLabels[i], -1, &fontPill, pillRect, &sfCenter, &whiteBrush);
        } else {
            SolidBrush unselBrush(isHovered ? Color(255, 42, 45, 56) : Color(255, 30, 32, 40));
            g.FillPath(&unselBrush, &pPath);
            Pen unselPen(Color(45, 255, 255, 255), 1.0f);
            g.DrawPath(&unselPen, &pPath);

            SolidBrush textBrush(isHovered ? Color(255, 240, 240, 240) : Color(255, 160, 165, 175));
            RectF pillRect((float)px, (float)pillY, (float)pillW, (float)pillH);
            g.DrawString(pillLabels[i], -1, &fontPill, pillRect, &sfCenter, &textBrush);
        }
    }

    // 5. Main Action Button
    int btnX = 24;
    int btnY = 110;
    int btnW = width - 48;
    int btnH = 70;

    GraphicsPath btnPath;
    int radius = 10;
    btnPath.AddArc(btnX, btnY, radius * 2, radius * 2, 180, 90);
    btnPath.AddArc(btnX + btnW - radius * 2, btnY, radius * 2, radius * 2, 270, 90);
    btnPath.AddArc(btnX + btnW - radius * 2, btnY + btnH - radius * 2, radius * 2, radius * 2, 0, 90);
    btnPath.AddArc(btnX, btnY + btnH - radius * 2, radius * 2, radius * 2, 90, 90);
    btnPath.CloseFigure();

    if (m_state == PluginState::RECORDING) {
        // Red Recording State with Live Progress Bar
        SolidBrush recBgBrush(Color(255, 45, 15, 18));
        g.FillPath(&recBgBrush, &btnPath);

        // Progress Fill
        float progressClamped = std::clamp(m_progress, 0.0f, 1.0f);
        int fillWidth = static_cast<int>(btnW * progressClamped);
        if (fillWidth > 4) {
            g.SetClip(&btnPath);
            LinearGradientBrush progressBrush(Point(btnX, btnY), Point(btnX + fillWidth, btnY),
                                              Color(255, 220, 38, 38), Color(255, 245, 80, 50));
            g.FillRectangle(&progressBrush, btnX, btnY, fillWidth, btnH);
            g.ResetClip();
        }

        Pen recPen(Color(255, 239, 68, 68), 1.5f);
        g.DrawPath(&recPen, &btnPath);

        std::wstringstream titleSS;
        titleSS << L"⏺  AUFNAHME... (" << std::fixed << std::setprecision(1) << m_recordedSeconds 
                << L"s / " << std::fixed << std::setprecision(1) << m_targetSeconds << L"s)";
        RectF titleRect((float)btnX, (float)btnY + 14.0f, (float)btnW, 22.0f);
        g.DrawString(titleSS.str().c_str(), -1, &fontBtnMain, titleRect, &sfCenter, &whiteBrush);

        RectF subRect((float)btnX, (float)btnY + 38.0f, (float)btnW, 18.0f);
        g.DrawString(L"(Klicken zum vorzeitigen Senden)", -1, &fontBtnSub, subRect, &sfCenter, &whiteBrush);

    } else if (m_state == PluginState::UPLOADING) {
        // Uploading Animation State with Moving Shimmer
        SolidBrush uploadBg(Color(255, 28, 30, 42));
        g.FillPath(&uploadBg, &btnPath);

        // Moving animated shimmer bar
        g.SetClip(&btnPath);
        float shimmerOffset = std::fmod(m_animPhase * 80.0f, (float)(btnW + 120)) - 60.0f;
        int sx = btnX + static_cast<int>(shimmerOffset);
        LinearGradientBrush shimmerBrush(Point(sx, btnY), Point(sx + 100, btnY),
                                         Color(0, 255, 92, 0), Color(255, 255, 120, 30));
        g.FillRectangle(&shimmerBrush, sx, btnY, 100, btnH);
        g.ResetClip();

        Pen upPen(Color(255, 255, 92, 0), 1.5f);
        g.DrawPath(&upPen, &btnPath);

        RectF titleRect((float)btnX, (float)btnY + 14.0f, (float)btnW, 22.0f);
        g.DrawString(L"⟳  STREAMING AN SONOS...", -1, &fontBtnMain, titleRect, &sfCenter, &whiteBrush);

        RectF subRect((float)btnX, (float)btnY + 38.0f, (float)btnW, 18.0f);
        g.DrawString(L"(Sonos Play:1 startet sofort...)", -1, &fontBtnSub, subRect, &sfCenter, &whiteBrush);

    } else if (m_state == PluginState::SUCCESS) {
        // Emerald Green Success State
        LinearGradientBrush successBrush(Point(btnX, btnY), Point(btnX, btnY + btnH),
                                         Color(255, 16, 185, 129), Color(255, 5, 150, 105));
        g.FillPath(&successBrush, &btnPath);

        RectF titleRect((float)btnX, (float)btnY + 14.0f, (float)btnW, 22.0f);
        g.DrawString(L"✔  WIRD AUF SONOS ABGESPIELT!", -1, &fontBtnMain, titleRect, &sfCenter, &whiteBrush);

        RectF subRect((float)btnX, (float)btnY + 38.0f, (float)btnW, 18.0f);
        g.DrawString(L"Sonos Play:1 (Buero) • 1-Click Master Stream", -1, &fontBtnSub, subRect, &sfCenter, &whiteBrush);

    } else {
        // IDLE State: Sonos Orange Gradient
        Color btnColorTop = Color(255, 255, 92, 0);
        Color btnColorBottom = Color(255, 225, 75, 0);

        if (m_btnPressed) {
            btnColorTop = Color(255, 215, 70, 0);
            btnColorBottom = Color(255, 195, 55, 0);
        } else if (m_btnHovered) {
            btnColorTop = Color(255, 255, 115, 30);
            btnColorBottom = Color(255, 255, 92, 0);
        }

        LinearGradientBrush btnGradient(Point(btnX, btnY), Point(btnX, btnY + btnH), btnColorTop, btnColorBottom);
        g.FillPath(&btnGradient, &btnPath);

        RectF titleRect((float)btnX, (float)btnY + 14.0f, (float)btnW, 22.0f);
        g.DrawString(L"▶  STREAM TO SONOS", -1, &fontBtnMain, titleRect, &sfCenter, &whiteBrush);

        const wchar_t* subLabel = L"(8 Takte Loop-Snapshot)";
        if (m_mode == CaptureMode::BARS_4) subLabel = L"(4 Takte Loop-Snapshot)";
        else if (m_mode == CaptureMode::BARS_16) subLabel = L"(16 Takte Loop-Snapshot)";
        else if (m_mode == CaptureMode::TIME_30S) subLabel = L"(30 Sekunden Snapshot)";

        RectF subRect((float)btnX, (float)btnY + 38.0f, (float)btnW, 18.0f);
        g.DrawString(subLabel, -1, &fontBtnSub, subRect, &sfCenter, &whiteBrush);
    }

    // 6. Footer info
    g.DrawString(L"Ziel: Sonos Play:1 (Buero) • Port 5005", -1, &fontFooter, PointF(24.0f, (float)(height - 22)), &mutedBrush);

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

} // namespace playson
