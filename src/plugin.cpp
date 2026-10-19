#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <clap/clap.h>
#include <clap/ext/gui.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/params.h>

#include <vector>
#include <atomic>
#include <memory>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "wav_encoder.h"
#include "http_client.h"
#include "ui_win32.h"

namespace playson {

static const char* s_plugin_features[] = {
    CLAP_PLUGIN_FEATURE_UTILITY,
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    nullptr
};

static const clap_plugin_descriptor_t s_descriptor = {
    CLAP_VERSION_INIT,
    "com.maxdedecke.playson",
    "Playson - Sonos Master Bridge",
    "Max Dedecke",
    "https://github.com/MaxDedecke/Playson",
    "",
    "",
    "1.0.0",
    "Instant 1-Click Loop Bounce from Master Bus to Sonos Speakers",
    s_plugin_features
};

class PlaysonPlugin {
public:
    PlaysonPlugin(const clap_host_t* host) : m_host(host) {
        m_bufferL.reserve(48000 * 60); // pre-allocate up to 60 seconds of stereo
        m_bufferR.reserve(48000 * 60);
    }

    ~PlaysonPlugin() {
        if (m_gui) {
            m_gui->destroy();
        }
    }

    bool init() {
        return true;
    }

    bool activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) {
        m_sampleRate = static_cast<uint32_t>(sampleRate);
        return true;
    }

    void deactivate() {
    }

    bool startProcessing() {
        return true;
    }

    void stopProcessing() {
    }

    void reset() {
        m_bufferL.clear();
        m_bufferR.clear();
        m_recording = false;
        m_state = PluginState::IDLE;
    }

    clap_process_status process(const clap_process_t* process) {
        const uint32_t numFrames = process->frames_count;
        if (numFrames == 0) return CLAP_PROCESS_CONTINUE;

        if (process->audio_inputs_count == 0 || !process->audio_inputs ||
            process->audio_outputs_count == 0 || !process->audio_outputs) {
            return CLAP_PROCESS_CONTINUE;
        }

        const float* inL = process->audio_inputs[0].data32[0];
        const float* inR = (process->audio_inputs[0].channel_count > 1) 
                           ? process->audio_inputs[0].data32[1] 
                           : inL;

        float* outL = process->audio_outputs[0].data32[0];
        float* outR = (process->audio_outputs[0].channel_count > 1) 
                          ? process->audio_outputs[0].data32[1] 
                          : outL;

        // 1. Transparent Passthrough (0 latency on master)
        if (outL != inL) {
            std::memcpy(outL, inL, numFrames * sizeof(float));
        }
        if (outR != inR) {
            std::memcpy(outR, inR, numFrames * sizeof(float));
        }

        // 2. Transport tempo inspection
        if (process->transport && (process->transport->flags & CLAP_TRANSPORT_HAS_TEMPO)) {
            m_tempo = process->transport->tempo;
        }

        // 3. Audio Recording
        if (m_recording) {
            for (uint32_t i = 0; i < numFrames; ++i) {
                m_bufferL.push_back(inL[i]);
                m_bufferR.push_back(inR[i]);
            }

            float recordedSec = static_cast<float>(m_bufferL.size()) / static_cast<float>(m_sampleRate);
            float progress = (m_targetSeconds > 0.0f) ? (recordedSec / m_targetSeconds) : 0.0f;

            if (m_gui) {
                m_gui->setProgress(progress, recordedSec, m_targetSeconds);
            }

            if (recordedSec >= m_targetSeconds) {
                finishAndUpload();
            }
        }

        return CLAP_PROCESS_CONTINUE;
    }

    void triggerBounce() {
        if (m_state == PluginState::RECORDING) {
            finishAndUpload();
            return;
        }

        // Calculate target duration
        double tempo = (m_tempo > 20.0) ? m_tempo : 120.0;
        double secPerBeat = 60.0 / tempo;

        if (m_captureMode == CaptureMode::BARS_4) {
            m_targetSeconds = static_cast<float>(16.0 * secPerBeat);
        } else if (m_captureMode == CaptureMode::BARS_8) {
            m_targetSeconds = static_cast<float>(32.0 * secPerBeat);
        } else if (m_captureMode == CaptureMode::BARS_16) {
            m_targetSeconds = static_cast<float>(64.0 * secPerBeat);
        } else if (m_captureMode == CaptureMode::TIME_30S) {
            m_targetSeconds = 30.0f;
        }

        m_bufferL.clear();
        m_bufferR.clear();
        m_recording = true;
        m_state = PluginState::RECORDING;

        if (m_gui) {
            m_gui->setStatus(PluginState::RECORDING, "Aufnahme laeuft...");
            m_gui->setProgress(0.0f, 0.0f, m_targetSeconds);
        }
    }

    void finishAndUpload() {
        m_recording = false;
        m_state = PluginState::UPLOADING;
        if (m_gui) {
            m_gui->setStatus(PluginState::UPLOADING, "Sende Snapshot an Sonos Play:1...");
        }

        if (m_bufferL.empty()) {
            m_state = PluginState::IDLE;
            if (m_gui) {
                m_gui->setStatus(PluginState::IDLE, "Kein Audio aufgenommen");
            }
            return;
        }

        std::vector<float> copyL = m_bufferL;
        std::vector<float> copyR = m_bufferR;
        uint32_t sRate = m_sampleRate;

        m_bufferL.clear();
        m_bufferR.clear();

        std::vector<uint8_t> wavData = WavEncoder::encode16BitStereo(copyL, copyR, sRate);

        HttpClient::sendWavAsync(wavData, "FL_Studio_Snapshot.wav", m_connectorPort, [this](bool success, std::string msg) {
            if (success) {
                m_state = PluginState::SUCCESS;
                if (m_gui) {
                    m_gui->setStatus(PluginState::SUCCESS, "Wird jetzt auf Sonos Lautsprecher abgespielt!");
                }
            } else {
                m_state = PluginState::IDLE;
                if (m_gui) {
                    m_gui->setStatus(PluginState::IDLE, "Fehler: " + msg);
                }
            }
        });
    }

    bool guiCreate(const char* api, bool isFloating) {
        if (std::strcmp(api, CLAP_WINDOW_API_WIN32) != 0) return false;
        m_gui = std::make_unique<Win32Gui>();
        m_gui->setMode(m_captureMode);
        m_gui->onTriggerBounce = [this]() {
            this->triggerBounce();
        };
        m_gui->onModeChanged = [this](CaptureMode mode) {
            this->m_captureMode = mode;
        };
        return true;
    }

    void guiDestroy() {
        if (m_gui) {
            m_gui->destroy();
            m_gui.reset();
        }
    }

    bool guiSetParent(const clap_window_t* window) {
        if (!window || !m_gui) return false;
        return m_gui->create((HWND)window->win32, 360, 240);
    }

    bool guiSetVisible(bool visible) {
        if (m_gui) {
            m_gui->setVisible(visible);
            return true;
        }
        return false;
    }

    bool guiGetSize(uint32_t* width, uint32_t* height) {
        *width = 360;
        *height = 240;
        return true;
    }

    const clap_plugin_t* getClapPlugin() { return &m_plugin; }

    static const clap_plugin_t* createPlugin(const clap_host_t* host);

private:
    const clap_host_t* m_host = nullptr;
    clap_plugin_t m_plugin = { 0 };

    uint32_t m_sampleRate = 44100;
    double m_tempo = 120.0;
    uint16_t m_connectorPort = 5005;

    std::atomic<PluginState> m_state{ PluginState::IDLE };
    std::atomic<bool> m_recording{ false };
    CaptureMode m_captureMode{ CaptureMode::BARS_8 };
    float m_targetSeconds = 16.0f;

    std::vector<float> m_bufferL;
    std::vector<float> m_bufferR;

    std::unique_ptr<Win32Gui> m_gui;
};

// -------------------------------------------------------------
// CLAP C Interface Glue Code
// -------------------------------------------------------------

static bool CLAP_ABI clap_init_cb(const struct clap_plugin* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->init();
}

static void CLAP_ABI clap_destroy_cb(const struct clap_plugin* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    delete self;
}

static bool CLAP_ABI clap_activate_cb(const struct clap_plugin* plugin, double sample_rate, uint32_t min_frames, uint32_t max_frames) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->activate(sample_rate, min_frames, max_frames);
}

static void CLAP_ABI clap_deactivate_cb(const struct clap_plugin* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    self->deactivate();
}

static bool CLAP_ABI clap_start_processing_cb(const struct clap_plugin* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->startProcessing();
}

static void CLAP_ABI clap_stop_processing_cb(const struct clap_plugin* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    self->stopProcessing();
}

static void CLAP_ABI clap_reset_cb(const struct clap_plugin* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    self->reset();
}

static clap_process_status CLAP_ABI clap_process_cb(const struct clap_plugin* plugin, const clap_process_t* process) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->process(process);
}

// Audio Ports Extension
static uint32_t CLAP_ABI clap_audio_ports_count_cb(const clap_plugin_t* plugin, bool is_input) {
    return 1;
}

static bool CLAP_ABI clap_audio_ports_get_cb(const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_audio_port_info_t* info) {
    if (index > 0) return false;
    info->id = 0;
    std::strncpy(info->name, is_input ? "Master In" : "Master Out", sizeof(info->name));
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = 0;
    return true;
}

static const clap_plugin_audio_ports_t s_audio_ports = {
    clap_audio_ports_count_cb,
    clap_audio_ports_get_cb
};

// GUI Extension
static bool CLAP_ABI clap_gui_is_api_supported_cb(const clap_plugin_t* plugin, const char* api, bool is_floating) {
    return std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
}

static bool CLAP_ABI clap_gui_get_preferred_api_cb(const clap_plugin_t* plugin, const char** api, bool* is_floating) {
    *api = CLAP_WINDOW_API_WIN32;
    *is_floating = false;
    return true;
}

static bool CLAP_ABI clap_gui_create_cb(const clap_plugin_t* plugin, const char* api, bool is_floating) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->guiCreate(api, is_floating);
}

static void CLAP_ABI clap_gui_destroy_cb(const clap_plugin_t* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    self->guiDestroy();
}

static bool CLAP_ABI clap_gui_set_scale_cb(const clap_plugin_t* plugin, double scale) {
    return false;
}

static bool CLAP_ABI clap_gui_get_size_cb(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->guiGetSize(width, height);
}

static bool CLAP_ABI clap_gui_can_resize_cb(const clap_plugin_t* plugin) {
    return false;
}

static bool CLAP_ABI clap_gui_get_resize_hints_cb(const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints) {
    return false;
}

static bool CLAP_ABI clap_gui_adjust_size_cb(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
    return true;
}

static bool CLAP_ABI clap_gui_set_size_cb(const clap_plugin_t* plugin, uint32_t width, uint32_t height) {
    return false;
}

static bool CLAP_ABI clap_gui_set_parent_cb(const clap_plugin_t* plugin, const clap_window_t* window) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->guiSetParent(window);
}

static bool CLAP_ABI clap_gui_set_transient_cb(const clap_plugin_t* plugin, const clap_window_t* window) {
    return false;
}

static void CLAP_ABI clap_gui_suggest_title_cb(const clap_plugin_t* plugin, const char* title) {
}

static bool CLAP_ABI clap_gui_show_cb(const clap_plugin_t* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->guiSetVisible(true);
}

static bool CLAP_ABI clap_gui_hide_cb(const clap_plugin_t* plugin) {
    auto* self = (PlaysonPlugin*)plugin->plugin_data;
    return self->guiSetVisible(false);
}

static const clap_plugin_gui_t s_plugin_gui = {
    clap_gui_is_api_supported_cb,
    clap_gui_get_preferred_api_cb,
    clap_gui_create_cb,
    clap_gui_destroy_cb,
    clap_gui_set_scale_cb,
    clap_gui_get_size_cb,
    clap_gui_can_resize_cb,
    clap_gui_get_resize_hints_cb,
    clap_gui_adjust_size_cb,
    clap_gui_set_size_cb,
    clap_gui_set_parent_cb,
    clap_gui_set_transient_cb,
    clap_gui_suggest_title_cb,
    clap_gui_show_cb,
    clap_gui_hide_cb
};

// Extension query
static const void* CLAP_ABI clap_get_extension_cb(const struct clap_plugin* plugin, const char* id) {
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &s_audio_ports;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &s_plugin_gui;
    return nullptr;
}

static void CLAP_ABI clap_on_main_thread_cb(const struct clap_plugin* plugin) {
}

const clap_plugin_t* PlaysonPlugin::createPlugin(const clap_host_t* host) {
    auto* instance = new PlaysonPlugin(host);
    instance->m_plugin.desc = &s_descriptor;
    instance->m_plugin.plugin_data = instance;
    instance->m_plugin.init = clap_init_cb;
    instance->m_plugin.destroy = clap_destroy_cb;
    instance->m_plugin.activate = clap_activate_cb;
    instance->m_plugin.deactivate = clap_deactivate_cb;
    instance->m_plugin.start_processing = clap_start_processing_cb;
    instance->m_plugin.stop_processing = clap_stop_processing_cb;
    instance->m_plugin.reset = clap_reset_cb;
    instance->m_plugin.process = clap_process_cb;
    instance->m_plugin.get_extension = clap_get_extension_cb;
    instance->m_plugin.on_main_thread = clap_on_main_thread_cb;
    return &instance->m_plugin;
}

// -------------------------------------------------------------
// Factory & Entry Point
// -------------------------------------------------------------

static uint32_t CLAP_ABI clap_factory_get_plugin_count(const struct clap_plugin_factory* factory) {
    return 1;
}

static const clap_plugin_descriptor_t* CLAP_ABI clap_factory_get_plugin_descriptor(const struct clap_plugin_factory* factory, uint32_t index) {
    if (index == 0) return &s_descriptor;
    return nullptr;
}

static const clap_plugin_t* CLAP_ABI clap_factory_create_plugin(const struct clap_plugin_factory* factory, const clap_host_t* host, const char* plugin_id) {
    if (std::strcmp(plugin_id, s_descriptor.id) == 0) {
        return PlaysonPlugin::createPlugin(host);
    }
    return nullptr;
}

static const clap_plugin_factory_t s_factory = {
    clap_factory_get_plugin_count,
    clap_factory_get_plugin_descriptor,
    clap_factory_create_plugin
};

static bool CLAP_ABI clap_entry_init(const char* plugin_path) {
    return true;
}

static void CLAP_ABI clap_entry_deinit(void) {
}

static const void* CLAP_ABI clap_entry_get_factory(const char* factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &s_factory;
    }
    return nullptr;
}

} // namespace playson

extern "C" {
CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    playson::clap_entry_init,
    playson::clap_entry_deinit,
    playson::clap_entry_get_factory
};
}
