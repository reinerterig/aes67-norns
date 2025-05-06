// AudioConverter.cpp
#include "AudioConverter.h"
#include <cstring>
#include <iostream>
#include <cmath>
#include <random>

namespace aes67 {

AudioConverter::AudioConverter()
    : sampleRate(48000), channelCount(2), bitDepth(24),
      maxIntValue(8388607.0f), minIntValue(-8388608.0f),
      ditherScale(4.0f / 8388607.0f), bytesPerSample(3)
{
    // Initialize dither states
    ditherStates.resize(channelCount);
    for (auto& state : ditherStates) {
        state.error = 0.0f;
        state.errorPrev1 = 0.0f;
        state.errorPrev2 = 0.0f;
        state.random = 0.0f;
    }
}

AudioConverter::~AudioConverter() {
    // Nothing specific to clean up
}

void AudioConverter::initialize(uint32_t rate, uint16_t channels, uint16_t bits) {
    setSampleRate(rate);
    setChannelCount(channels);
    setBitDepth(bits);
}

void AudioConverter::setSampleRate(uint32_t rate) {
    sampleRate = rate;
}

void AudioConverter::setChannelCount(uint16_t channels) {
    channelCount = channels;
    
    // Resize dither states
    ditherStates.resize(channelCount);
    for (auto& state : ditherStates) {
        state.error = 0.0f;
        state.errorPrev1 = 0.0f;
        state.errorPrev2 = 0.0f;
        state.random = 0.0f;
    }
}

void AudioConverter::setBitDepth(uint16_t bits) {
    bitDepth = bits;
    
    // Set conversion parameters based on bit depth
    switch (bitDepth) {
        case 16:
            maxIntValue = 32767.0f;
            minIntValue = -32768.0f;
            ditherScale = 4.0f / 32767.0f;
            bytesPerSample = 2;
            break;
        case 24:
            maxIntValue = 8388607.0f;
            minIntValue = -8388608.0f;
            ditherScale = 4.0f / 8388607.0f;
            bytesPerSample = 3;
            break;
        case 32:
            maxIntValue = 2147483647.0f;
            minIntValue = -2147483648.0f;
            ditherScale = 4.0f / 2147483647.0f;
            bytesPerSample = 4;
            break;
        default:
            std::cerr << "Unsupported bit depth: " << bitDepth << ", defaulting to 24-bit" << std::endl;
            bitDepth = 24;
            maxIntValue = 8388607.0f;
            minIntValue = -8388608.0f;
            ditherScale = 4.0f / 8388607.0f;
            bytesPerSample = 3;
            break;
    }
}

void AudioConverter::floatToInt(const float* input, uint8_t* output, size_t frameCount) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    
    for (size_t frame = 0; frame < frameCount; frame++) {
        for (size_t channel = 0; channel < channelCount; channel++) {
            float sample = input[frame * channelCount + channel];
            
            // Apply dither and noise shaping
            DitherState& dither = ditherStates[channel];
            
            // Scale to integer range
            float scaled = sample * maxIntValue;
            
            // Apply noise shaping
            scaled += dither.error - dither.errorPrev1 + dither.errorPrev2;
            
            // Generate random dither
            float random = dist(gen) * ditherScale;
            
            // Add dither and bias
            float dithered = scaled + 0.5f + (random - dither.random);
            
            // Clip
            if (dithered > maxIntValue) {
                dithered = maxIntValue;
            } else if (dithered < minIntValue) {
                dithered = minIntValue;
            }
            
            // Quantize
            int32_t quantized = static_cast<int32_t>(dithered);
            
            // Update dither state
            dither.random = random;
            dither.errorPrev2 = dither.errorPrev1;
            dither.errorPrev1 = dither.error;
            dither.error = scaled - static_cast<float>(quantized);
            
            // Convert to bytes
            uint8_t* dest = output + (frame * channelCount + channel) * bytesPerSample;
            
            switch (bitDepth) {
                case 16:
                    dest[0] = static_cast<uint8_t>((quantized >> 8) & 0xFF);
                    dest[1] = static_cast<uint8_t>(quantized & 0xFF);
                    break;
                case 24:
                    dest[0] = static_cast<uint8_t>((quantized >> 16) & 0xFF);
                    dest[1] = static_cast<uint8_t>((quantized >> 8) & 0xFF);
                    dest[2] = static_cast<uint8_t>(quantized & 0xFF);
                    break;
                case 32:
                    dest[0] = static_cast<uint8_t>((quantized >> 24) & 0xFF);
                    dest[1] = static_cast<uint8_t>((quantized >> 16) & 0xFF);
                    dest[2] = static_cast<uint8_t>((quantized >> 8) & 0xFF);
                    dest[3] = static_cast<uint8_t>(quantized & 0xFF);
                    break;
            }
        }
    }
}

void AudioConverter::intToFloat(const uint8_t* input, float* output, size_t frameCount) {
    for (size_t frame = 0; frame < frameCount; frame++) {
        for (size_t channel = 0; channel < channelCount; channel++) {
            const uint8_t* src = input + (frame * channelCount + channel) * bytesPerSample;
            
            // Convert from bytes to integer
            int32_t value = 0;
            
            switch (bitDepth) {
                case 16: {
                    int16_t sample = static_cast<int16_t>((src[0] << 8) | src[1]);
                    value = sample;
                    break;
                }
                case 24: {
                    // Sign extend 24-bit to 32-bit
                    if (src[0] & 0x80) {
                        value = 0xFF000000 | (src[0] << 16) | (src[1] << 8) | src[2];
                    } else {
                        value = (src[0] << 16) | (src[1] << 8) | src[2];
                    }
                    break;
                }
                case 32:
                    value = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
                    break;
            }
            
            // Convert to float
            float sample = static_cast<float>(value) / maxIntValue;
            
            // Clip
            if (sample > 1.0f) {
                sample = 1.0f;
            } else if (sample < -1.0f) {
                sample = -1.0f;
            }
            
            output[frame * channelCount + channel] = sample;
        }
    }
}

void AudioConverter::processFloatToInt(const std::vector<float>& input, std::vector<uint8_t>& output) {
    if (input.empty()) {
        output.clear();
        return;
    }
    
    size_t frameCount = input.size() / channelCount;
    output.resize(frameCount * channelCount * bytesPerSample);
    
    floatToInt(input.data(), output.data(), frameCount);
}

void AudioConverter::processIntToFloat(const std::vector<uint8_t>& input, std::vector<float>& output) {
    if (input.empty()) {
        output.clear();
        return;
    }
    
    size_t frameCount = input.size() / (channelCount * bytesPerSample);
    output.resize(frameCount * channelCount);
    
    intToFloat(input.data(), output.data(), frameCount);
}

} // namespace aes67
