// PTPSync.h
#pragma once

#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>

namespace aes67 {

class PTPSync {
public:
    PTPSync();
    ~PTPSync();
    
    // Configuration and control
    bool initialize(const std::string& multicastAddr = "224.0.1.129");
    void shutdown();
    void setSampleRate(uint32_t rate);
    
    // Clock operations
    int64_t getClockOffset() const;
    uint64_t getCurrentTimestamp() const;
    
    // Status
    bool isActive() const { return active; }
    bool isSynchronized() const { return synchronized; }
    const std::string& getMasterClockId() const { return masterClockId; }
    
private:
    // Socket descriptors
    int eventSocket;  // For PTP event messages (port 319)
    int generalSocket; // For PTP general messages (port 320)
    int requestSocket; // For sending delay requests
    
    // Configuration
    std::string multicastAddr;
    uint32_t sampleRate;
    
    // Synchronization state
    std::atomic<bool> active;
    std::atomic<bool> synchronized;
    std::string masterClockId;
    
    // PTP timestamps
    std::atomic<int64_t> clockOffset;
    std::atomic<uint64_t> masterTimestamp;
    std::atomic<uint64_t> localTimestamp;
    
    // Sequence counters
    uint16_t syncSequence;
    uint16_t delaySequence;
    
    // Worker threads
    std::thread eventThread;
    std::thread generalThread;
    
    // Thread functions
    void eventThreadFunc();
    void generalThreadFunc();
    void sendDelayRequest();
    
    // Timestamp conversion
    uint64_t ptpToSamples(const uint8_t* timestamp) const;
};

} // namespace aes67
