#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <atomic>

namespace playson {

enum class PluginState {
    IDLE,
    WAITING_FOR_LOOP_START,
    RECORDING,
    UPLOADING,
    SUCCESS
};

enum class CaptureMode {
    LOOP_RANGE,
    BARS_4,
    BARS_8,
    BARS_16,
    MANUAL
};

class Win32Gui {
public:
    Win32Gui();
    ~Win32Gui();

    bool create(HWND parent, uint32_t width, uint32_t height);
    void destroy();
    void setVisible(bool visible);

    void setStatus(PluginState state, const std::string& message);
    void setRecordedSeconds(float seconds);

    // Callbacks
    std::function<void()> onTriggerBounce;
    std::function<void(CaptureMode)> onModeChanged;

    HWND getHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void paint(HDC hdc);

    HWND m_parent = NULL;
    HWND m_hwnd = NULL;
    uint32_t m_width = 340;
    uint32_t m_height = 240;

    PluginState m_state = PluginState::IDLE;
    CaptureMode m_mode = CaptureMode::LOOP_RANGE;
    std::string m_statusMessage = "Bereit zum Abspielen auf Sonos";
    float m_recordedSeconds = 0.0f;
    bool m_btnHovered = false;
    bool m_btnPressed = false;
};

} // namespace playson
