#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace playson {

class WavEncoder {
public:
    static std::vector<uint8_t> encode16BitStereo(const std::vector<float>& leftChannel,
                                                   const std::vector<float>& rightChannel,
                                                   uint32_t sampleRate) {
        size_t numSamples = std::min(leftChannel.size(), rightChannel.size());
        uint32_t numChannels = 2;
        uint16_t bitsPerSample = 16;
        uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        uint16_t blockAlign = numChannels * (bitsPerSample / 8);
        uint32_t dataSize = static_cast<uint32_t>(numSamples * blockAlign);
        uint32_t totalFileSize = 36 + dataSize;

        std::vector<uint8_t> wav;
        wav.reserve(44 + dataSize);

        auto write32 = [&wav](uint32_t value) {
            wav.push_back(static_cast<uint8_t>(value & 0xFF));
            wav.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            wav.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            wav.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        };

        auto write16 = [&wav](uint16_t value) {
            wav.push_back(static_cast<uint8_t>(value & 0xFF));
            wav.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        };

        auto writeChars = [&wav](const char* chars, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                wav.push_back(static_cast<uint8_t>(chars[i]));
            }
        };

        // 1. RIFF Header
        writeChars("RIFF", 4);
        write32(totalFileSize);
        writeChars("WAVE", 4);

        // 2. fmt Subchunk
        writeChars("fmt ", 4);
        write32(16);             // Subchunk1Size (16 for PCM)
        write16(1);              // AudioFormat (1 = PCM)
        write16(numChannels);    // NumChannels
        write32(sampleRate);     // SampleRate
        write32(byteRate);       // ByteRate
        write16(blockAlign);     // BlockAlign
        write16(bitsPerSample);  // BitsPerSample

        // 3. data Subchunk
        writeChars("data", 4);
        write32(dataSize);

        // Samples: Interleave L and R with 16-bit clamp
        for (size_t i = 0; i < numSamples; ++i) {
            float l = std::clamp(leftChannel[i], -1.0f, 1.0f);
            float r = std::clamp(rightChannel[i], -1.0f, 1.0f);

            int16_t sampleL = static_cast<int16_t>(l * 32767.0f);
            int16_t sampleR = static_cast<int16_t>(r * 32767.0f);

            write16(static_cast<uint16_t>(sampleL));
            write16(static_cast<uint16_t>(sampleR));
        }

        return wav;
    }
};

} // namespace playson
