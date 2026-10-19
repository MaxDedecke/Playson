#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _HAS_STD_BYTE
#define _HAS_STD_BYTE 0
#endif

#include <windows.h>
#include <objidl.h>
#include <string>
#include <functional>
#include <atomic>

namespace playson {

enum class PluginState {
    IDLE,
    RECORDING,
    UPLOADING,
    SUCCESS
};

enum class CaptureMode {
    BARS_4,
    BARS_8,
    BARS_16,
    TIME_30S
};

class Win32Gui {
public:
    Win32Gui();
    ~Win32Gui();

    bool create(HWND parent, uint32_t width, uint32_t height);
    void destroy();
    void setVisible(bool visible);

    void setStatus(PluginState state, const std::string& message);
    void setProgress(float progress, float currentSec, float targetSec);
    void setMode(CaptureMode mode) { m_mode = mode; }
    CaptureMode getMode() const { return m_mode; }

    // Callbacks
    std::function<void()> onTriggerBounce;
    std::function<void(CaptureMode)> onModeChanged;

    HWND getHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void paint(HDC hdc);

    HWND m_parent = NULL;
    HWND m_hwnd = NULL;
    uint32_t m_width = 360;
    uint32_t m_height = 250;

    PluginState m_state = PluginState::IDLE;
    CaptureMode m_mode = CaptureMode::BARS_8;
    std::string m_statusMessage = "Bereit zum Abspielen auf Sonos";

    float m_progress = 0.0f;
    float m_recordedSeconds = 0.0f;
    float m_targetSeconds = 16.0f;
    float m_animPhase = 0.0f;
    uint32_t m_successCountdown = 0;

    bool m_btnHovered = false;
    bool m_btnPressed = false;
    int m_hoveredPill = -1; // 0=4bars, 1=8bars, 2=16bars, 3=30s
};

} // namespace playson
