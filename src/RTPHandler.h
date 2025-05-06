// RTPHandler.h
#pragma once

#include <cstdint>
#include <vector>
#include <atomic>
#include <mutex>
#include <array>
#include <memory>

namespace aes67 {

class RTPHandler {
public:
    RTPHandler();
    ~RTPHandler();
    
    struct AudioData {
        std::vector<float> samples;  // Interleaved audio samples
        uint32_t channelCount;       // Number of channels
        uint32_t sampleRate;         // Sample rate
        uint32_t frameCount;         // Number of frames
    };
    
    // Configuration
    void initialize(uint32_t sampleRate, uint16_t channels, uint16_t payloadType = 96);
    void setSampleRate(uint32_t rate);
    void setChannelCount(uint16_t channels);
    void setPayloadType(uint16_t type);
    
    // Packet operations
    bool createPacket(const AudioData& audio, std::vector<uint8_t>& packet);
    bool parsePacket(const uint8_t* data, size_t size, AudioData& audio);
    
    // Buffer management for handling packet reordering and jitter
    void addPacketToBuffer(const uint8_t* data, size_t size);
    bool getNextAudioFrame(AudioData& audio);
    
    // Status
    uint32_t getPacketCount() const { return packetCount; }
    uint32_t getDroppedPackets() const { return droppedPackets; }
    uint32_t getOutOfOrderPackets() const { return outOfOrderPackets; }
    
private:
    // RTP session data
    uint32_t ssrc;           // Synchronization source identifier
    uint16_t sequenceNumber; // Packet sequence number
    uint32_t timestamp;      // RTP timestamp
    
    // Configuration
    uint32_t sampleRate;
    uint16_t channelCount;
    uint16_t payloadType;
    
    // Packet buffer for reordering and jitter management
    static constexpr size_t MAX_BUFFER_PACKETS = 32;
    struct PacketEntry {
        std::vector<uint8_t> data;
        uint16_t sequenceNumber;
        bool valid;
    };
    std::array<PacketEntry, MAX_BUFFER_PACKETS> packetBuffer;
    
    // Expected next sequence number
    uint16_t expectedSequence;
    
    // Statistics
    std::atomic<uint32_t> packetCount;
    std::atomic<uint32_t> droppedPackets;
    std::atomic<uint32_t> outOfOrderPackets;
    
    // Synchronization
    std::mutex bufferMutex;
    
    // Helper functions
    uint16_t getBufferIndex(uint16_t sequence) const;
    void processBuffer();
};

} // namespace aes67
