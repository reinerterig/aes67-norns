// PTPSync.cpp
#include "PTPSync.h"
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>

namespace aes67 {

// PTP packet structure
struct PTPHeader {
    uint8_t  messageType;  // Message type and transport specific
    uint8_t  versionPTP;   // Version PTP
    uint16_t messageLength;// Message length
    uint8_t  domainNumber; // Domain number
    uint8_t  reserved1;    // Reserved
    uint16_t flags;        // Flags
    int64_t  correction;   // Correction
    uint32_t reserved2;    // Reserved
    uint8_t  sourcePortId[10]; // Source port identity
    uint16_t sequenceId;   // Sequence ID
    uint8_t  control;      // Control
    int8_t   logMessageInt;// Log message interval
};

// PTP timestamp structure
struct PTPTimestamp {
    uint8_t seconds[6];    // 48-bit seconds
    uint32_t nanoseconds;  // 32-bit nanoseconds
};

PTPSync::PTPSync()
    : eventSocket(-1), generalSocket(-1), requestSocket(-1), 
      sampleRate(48000), active(false), synchronized(false),
      clockOffset(0), masterTimestamp(0), localTimestamp(0),
      syncSequence(0), delaySequence(0)
{
}

PTPSync::~PTPSync() {
    shutdown();
}

bool PTPSync::initialize(const std::string& addr) {
    multicastAddr = addr;
    
    // Create sockets
    eventSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (eventSocket < 0) {
        std::cerr << "Failed to create event socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    generalSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (generalSocket < 0) {
        std::cerr << "Failed to create general socket: " << strerror(errno) << std::endl;
        close(eventSocket);
        eventSocket = -1;
        return false;
    }
    
    requestSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (requestSocket < 0) {
        std::cerr << "Failed to create request socket: " << strerror(errno) << std::endl;
        close(eventSocket);
        close(generalSocket);
        eventSocket = -1;
        generalSocket = -1;
        return false;
    }
    
    // Set socket options
    int optval = 1;
    if (setsockopt(eventSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ||
        setsockopt(generalSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    // Join multicast groups
    struct sockaddr_in addr_in;
    struct ip_mreq mreq;
    
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = INADDR_ANY;
    
    // Set up event socket (port 319)
    addr_in.sin_port = htons(319);
    if (bind(eventSocket, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0) {
        std::cerr << "Failed to bind event socket: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(multicastAddr.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(eventSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join event multicast group: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    // Set up general socket (port 320)
    addr_in.sin_port = htons(320);
    if (bind(generalSocket, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0) {
        std::cerr << "Failed to bind general socket: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    if (setsockopt(generalSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join general multicast group: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    // Set up request socket (for sending delay requests)
    addr_in.sin_port = htons(319);
    addr_in.sin_addr.s_addr = inet_addr(multicastAddr.c_str());
    if (connect(requestSocket, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0) {
        std::cerr << "Failed to connect request socket: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    // Start worker threads
    active = true;
    eventThread = std::thread(&PTPSync::eventThreadFunc, this);
    generalThread = std::thread(&PTPSync::generalThreadFunc, this);
    
    std::cout << "PTP Synchronization initialized with multicast address " << multicastAddr << std::endl;
    return true;
}

void PTPSync::shutdown() {
    active = false;
    
    // Join threads if they're running
    if (eventThread.joinable()) {
        eventThread.join();
    }
    
    if (generalThread.joinable()) {
        generalThread.join();
    }
    
    // Close sockets
    if (eventSocket >= 0) {
        close(eventSocket);
        eventSocket = -1;
    }
    
    if (generalSocket >= 0) {
        close(generalSocket);
        generalSocket = -1;
    }
    
    if (requestSocket >= 0) {
        close(requestSocket);
        requestSocket = -1;
    }
    
    synchronized = false;
}

void PTPSync::setSampleRate(uint32_t rate) {
    sampleRate = rate;
}

int64_t PTPSync::getClockOffset() const {
    return clockOffset.load();
}

uint64_t PTPSync::getCurrentTimestamp() const {
    return localTimestamp.load() - clockOffset.load();
}

void PTPSync::eventThreadFunc() {
    uint8_t buffer[1500];
    PTPHeader* header = reinterpret_cast<PTPHeader*>(buffer);
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    
    while (active) {
        // Receive a PTP event message
        ssize_t len = recvfrom(eventSocket, buffer, sizeof(buffer), 0, 
                             (struct sockaddr*)&src_addr, &src_addr_len);
        
        if (len <= 0) {
            if (active) {
                std::cerr << "Error receiving PTP event message: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        if (len < sizeof(PTPHeader)) {
            continue;  // Packet too small
        }
        
        // Validate PTP version (2) and domain (0)
        if ((header->versionPTP & 0x0F) != 2 || header->domainNumber != 0) {
            continue;
        }
        
        // Get the message type
        uint8_t messageType = header->messageType & 0x0F;
        
        // Handle SYNC message (type 0)
        if (messageType == 0) {
            // Extract master clock ID
            char clockId[32];
            snprintf(clockId, sizeof(clockId), "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X",
                    header->sourcePortId[0], header->sourcePortId[1],
                    header->sourcePortId[2], header->sourcePortId[3],
                    header->sourcePortId[4], header->sourcePortId[5],
                    header->sourcePortId[6], header->sourcePortId[7]);
            
            // Check if this is a new master clock
            if (masterClockId != clockId) {
                masterClockId = clockId;
                std::cout << "New PTP master clock detected: " << masterClockId << std::endl;
                synchronized = false;  // Reset synchronization with new master
            }
            
            // Check if this is a two-step clock
            bool twoStep = (ntohs(header->flags) & 0x0200) != 0;
            
            // Record the sequence ID for two-step clocks
            if (twoStep) {
                syncSequence = ntohs(header->sequenceId);
                // Timestamp will be in the follow-up message
            } else {
                // Single-step clock, timestamp is in this message
                if (len >= sizeof(PTPHeader) + sizeof(PTPTimestamp)) {
                    // Extract timestamp
                    PTPTimestamp* ts = reinterpret_cast<PTPTimestamp*>(buffer + sizeof(PTPHeader));
                    masterTimestamp = ptpToSamples(reinterpret_cast<uint8_t*>(ts));
                    
                    // Record our local time
                    localTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count() * sampleRate / 1000000;
                    
                    // Send delay request periodically
                    sendDelayRequest();
                    
                    synchronized = true;
                }
            }
        }
    }
}

void PTPSync::generalThreadFunc() {
    uint8_t buffer[1500];
    PTPHeader* header = reinterpret_cast<PTPHeader*>(buffer);
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    
    // Variables for delay request-response
    uint64_t t1 = 0;  // Master sync timestamp
    uint64_t t2 = 0;  // Local sync receive time
    uint64_t t3 = 0;  // Local delay request send time
    uint64_t t4 = 0;  // Master delay response timestamp
    
    while (active) {
        // Receive a PTP general message
        ssize_t len = recvfrom(generalSocket, buffer, sizeof(buffer), 0, 
                             (struct sockaddr*)&src_addr, &src_addr_len);
        
        if (len <= 0) {
            if (active) {
                std::cerr << "Error receiving PTP general message: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        if (len < sizeof(PTPHeader)) {
            continue;  // Packet too small
        }
        
        // Validate PTP version (2) and domain (0)
        if ((header->versionPTP & 0x0F) != 2 || header->domainNumber != 0) {
            continue;
        }
        
        // Get the message type
        uint8_t messageType = header->messageType & 0x0F;
        
        // Handle FOLLOW_UP message (type 8) - second phase of two-step clock sync
        if (messageType == 8) {
            // Check if this is the follow-up for our recorded sync message
            if (ntohs(header->sequenceId) == syncSequence) {
                if (len >= sizeof(PTPHeader) + sizeof(PTPTimestamp)) {
                    // Extract timestamp
                    PTPTimestamp* ts = reinterpret_cast<PTPTimestamp*>(buffer + sizeof(PTPHeader));
                    t1 = ptpToSamples(reinterpret_cast<uint8_t*>(ts));
                    masterTimestamp = t1;
                    
                    // Get our local time
                    t2 = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count() * sampleRate / 1000000;
                    localTimestamp = t2;
                    
                    // Send delay request
                    sendDelayRequest();
                    
                    synchronized = true;
                }
            }
        }
        // Handle DELAY_RESP message (type 9)
        else if (messageType == 9) {
            // Check if this is the response to our delay request
            if (ntohs(header->sequenceId) == delaySequence) {
                if (len >= sizeof(PTPHeader) + sizeof(PTPTimestamp)) {
                    // Extract timestamp
                    PTPTimestamp* ts = reinterpret_cast<PTPTimestamp*>(buffer + sizeof(PTPHeader));
                    t4 = ptpToSamples(reinterpret_cast<uint8_t*>(ts));
                    
                    // Calculate clock offset: ((t2 - t1) + (t4 - t3)) / 2
                    int64_t offset = ((static_cast<int64_t>(t2) - static_cast<int64_t>(t1)) + 
                                      (static_cast<int64_t>(t4) - static_cast<int64_t>(t3))) / 2;
                    
                    // Update our clock offset
                    clockOffset = offset;
                    
                    std::cout << "PTP clock offset: " << offset << " samples" << std::endl;
                }
            }
        }
    }
}

void PTPSync::sendDelayRequest() {
    // Only send delay requests if we're synchronized
    if (!synchronized) {
        return;
    }
    
    // Create a delay request packet
    uint8_t buffer[sizeof(PTPHeader) + sizeof(PTPTimestamp)];
    PTPHeader* header = reinterpret_cast<PTPHeader*>(buffer);
    
    // Fill in the header
    memset(header, 0, sizeof(PTPHeader));
    header->messageType = 1;  // Delay Request
    header->versionPTP = 2;   // PTP Version 2
    header->messageLength = htons(sizeof(PTPHeader) + sizeof(PTPTimestamp));
    header->sequenceId = htons(++delaySequence);
    
    // Send the packet
    if (send(requestSocket, buffer, sizeof(PTPHeader) + sizeof(PTPTimestamp), 0) <= 0) {
        std::cerr << "Failed to send delay request: " << strerror(errno) << std::endl;
        return;
    }
    
    // Record the send time
    uint64_t t3 = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() * sampleRate / 1000000;
    
    // Store it for later calculation
    this->t3 = t3;
}

uint64_t PTPSync::ptpToSamples(const uint8_t* timestamp) const {
    // Extract 48-bit seconds field
    uint64_t seconds = 0;
    for (int i = 0; i < 6; i++) {
        seconds = (seconds << 8) | timestamp[i];
    }
    
    // Extract 32-bit nanoseconds field
    uint32_t nanoseconds = 0;
    for (int i = 0; i < 4; i++) {
        nanoseconds = (nanoseconds << 8) | timestamp[6 + i];
    }
    
    // Convert to samples
    uint64_t samples = seconds * sampleRate;
    samples += (static_cast<uint64_t>(nanoseconds) * sampleRate) / 1000000000;
    
    return samples;
}

} // namespace aes67
