// AudioConverter.h
#pragma once

#include <cstdint>
#include <vector>

namespace aes67 {

class AudioConverter {
public:
    AudioConverter();
    ~AudioConverter();
    
    // Configuration
    void initialize(uint32_t sampleRate, uint16_t channels, uint16_t bitDepth = 24);
    void setSampleRate(uint32_t rate);
    void setChannelCount(uint16_t channels);
    void setBitDepth(uint16_t bits);
    
    // Conversion functions
    // Convert from float to integer (JACK to AES67)
    void floatToInt(const float* input, uint8_t* output, size_t frameCount);
    
    // Convert from integer to float (AES67 to JACK)
    void intToFloat(const uint8_t* input, float* output, size_t frameCount);
    
    // Batch processing
    void processFloatToInt(const std::vector<float>& input, std::vector<uint8_t>& output);
    void processIntToFloat(const std::vector<uint8_t>& input, std::vector<float>& output);
    
private:
    // Configuration
    uint32_t sampleRate;
    uint16_t channelCount;
    uint16_t bitDepth;
    
    // Conversion parameters
    float maxIntValue;    // Maximum integer sample value
    float minIntValue;    // Minimum integer sample value
    float ditherScale;    // Dither scale factor
    size_t bytesPerSample; // Bytes per sample
    
    // Dither state
    struct DitherState {
        float error;      // Current error
        float errorPrev1; // Previous error
        float errorPrev2; // Error from 2 samples ago
        float random;     // Last random value
    };
    std::vector<DitherState> ditherStates;
    
    // Helper functions
    float convertIntToFloat(const uint8_t* input);
    void convertFloatToInt(float sample, uint8_t* output, DitherState& dither);
};

} // namespace aes67
