// NetworkManager.h
#pragma once

#include <string>
#include <cstdint>
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>

namespace aes67 {

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // Socket configuration
    bool initialize(const std::string& multicastAddr, uint16_t port, const std::string& interface = "");
    void shutdown();
    
    // Socket operations
    bool sendPacket(const void* data, size_t size);
    bool receivePacket(void* buffer, size_t maxSize, size_t& bytesRead);
    
    // Interface management
    bool setInterface(const std::string& interfaceName);
    std::vector<std::string> getAvailableInterfaces() const;
    
    // Status
    bool isActive() const { return active; }
    const std::string& getMulticastAddress() const { return multicastAddr; }
    uint16_t getPort() const { return port; }
    const std::string& getInterface() const { return interfaceName; }
    
private:
    // Socket descriptors
    int sendSocket;
    int recvSocket;
    
    // Configuration
    std::string multicastAddr;
    uint16_t port;
    std::string interfaceName;
    
    // Interface information
    uint32_t interfaceAddr;  // Interface IP address
    uint32_t interfaceIndex; // Interface index
    uint32_t interfaceMTU;   // Interface MTU
    
    // Status
    std::atomic<bool> active;
    
    // Helper functions
    bool joinMulticastGroup();
    bool setSocketOptions();
    bool getInterfaceInfo();
};

} // namespace aes67
