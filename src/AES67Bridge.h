// AES67Bridge.h Phase 2
#pragma once

#include "JackClient.h"
#include "NetworkManager.h"
#include "RTPHandler.h"
#include "PTPSync.h"
#include "AudioConverter.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>

namespace aes67 {

class AES67Bridge : public JackClient<2, 2> {
public:
    // Constructor/destructor
    AES67Bridge();
    ~AES67Bridge();

    // From JackClient
    void process(jack_nframes_t numFrames) override;
    void setSampleRate(jack_nframes_t sr) override;

    // Network configuration methods
    bool setNetworkAddress(const std::string& address, int port);
    bool setNetworkInterface(const std::string& interface);
    
    // Operation control
    bool startNetworking();
    bool stopNetworking();
    
    // Configuration 
    bool setMode(bool transmit); // true = transmit, false = receive
    void setBitDepth(int bits);
    void setPacketTime(int microseconds);
    
    // Status reporting
    bool isNetworkActive() const;
    float getBufferLevel() const;
    int getPacketCount() const;
    int getDroppedPackets() const;
    const std::string& getMasterClock() const;
    bool isPTPSynchronized() const;

private:
    // Operational mode
    enum class Mode {
        Receive,    // Receive AES67 audio and output to JACK
        Transmit,   // Take JACK input and transmit as AES67
        Inactive    // Not sending or receiving
    };

    // Configuration
    Mode mode;
    int bitDepth;
    int packetTime;  // in microseconds
    
    // Network components
    std::unique_ptr<NetworkManager> network;
    std::unique_ptr<RTPHandler> rtp;
    std::unique_ptr<PTPSync> ptp;
    std::unique_ptr<AudioConverter> converter;
    
    // Audio processing buffer
    std::vector<float> jackBuffer;
    std::vector<uint8_t> networkBuffer;
    size_t bufferSize;
    std::mutex bufferMutex;
    
    // Network thread
    std::thread networkThread;
    std::atomic<bool> threadRunning;
    
    // Status
    std::atomic<bool> networkActive;
    std::atomic<float> bufferLevel;
    
    // Network processing
    void networkReceiveLoop();
    void networkTransmitLoop();
    
    // Buffer management
    void clearBuffers(size_t numFrames);
    void resizeBuffers(size_t numFrames);
    
    // Helper functions
    size_t calculatePacketSamples() const;
};

} // namespace aes67
