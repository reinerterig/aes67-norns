// NetworkManager.cpp
#include "NetworkManager.h"

#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <iostream>

namespace aes67 {

NetworkManager::NetworkManager() 
    : sendSocket(-1), recvSocket(-1), port(0), 
      interfaceAddr(0), interfaceIndex(0), interfaceMTU(1500),
      active(false)
{
}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize(const std::string& addr, uint16_t port, const std::string& interface) {
    // Store configuration
    this->multicastAddr = addr;
    this->port = port;
    
    // Set the interface if provided
    if (!interface.empty() && !setInterface(interface)) {
        return false;
    }
    
    // Create sockets
    sendSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSocket < 0) {
        std::cerr << "Failed to create send socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    recvSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (recvSocket < 0) {
        std::cerr << "Failed to create receive socket: " << strerror(errno) << std::endl;
        close(sendSocket);
        sendSocket = -1;
        return false;
    }
    
    // Configure sockets
    if (!setSocketOptions()) {
        shutdown();
        return false;
    }
    
    // Join multicast group for receiving
    if (!joinMulticastGroup()) {
        shutdown();
        return false;
    }
    
    // Bind receive socket to port
    struct sockaddr_in addr_in;
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    addr_in.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(recvSocket, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0) {
        std::cerr << "Failed to bind receive socket: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    // Connect send socket to destination
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    if (inet_aton(multicastAddr.c_str(), &addr_in.sin_addr) == 0) {
        std::cerr << "Invalid multicast address: " << multicastAddr << std::endl;
        shutdown();
        return false;
    }
    
    if (connect(sendSocket, (struct sockaddr*)&addr_in, sizeof(addr_in)) < 0) {
        std::cerr << "Failed to connect send socket: " << strerror(errno) << std::endl;
        shutdown();
        return false;
    }
    
    active = true;
    return true;
}

void NetworkManager::shutdown() {
    active = false;
    
    if (sendSocket >= 0) {
        close(sendSocket);
        sendSocket = -1;
    }
    
    if (recvSocket >= 0) {
        close(recvSocket);
        recvSocket = -1;
    }
}

bool NetworkManager::sendPacket(const void* data, size_t size) {
    if (!active || sendSocket < 0) {
        return false;
    }
    
    ssize_t result = send(sendSocket, data, size, 0);
    if (result < 0) {
        std::cerr << "Failed to send packet: " << strerror(errno) << std::endl;
        return false;
    }
    
    return static_cast<size_t>(result) == size;
}

bool NetworkManager::receivePacket(void* buffer, size_t maxSize, size_t& bytesRead) {
    if (!active || recvSocket < 0) {
        return false;
    }
    
    ssize_t result = recv(recvSocket, buffer, maxSize, 0);
    if (result < 0) {
        std::cerr << "Failed to receive packet: " << strerror(errno) << std::endl;
        return false;
    }
    
    bytesRead = static_cast<size_t>(result);
    return true;
}

bool NetworkManager::setInterface(const std::string& ifName) {
    interfaceName = ifName;
    return getInterfaceInfo();
}

std::vector<std::string> NetworkManager::getAvailableInterfaces() const {
    std::vector<std::string> interfaces;
    struct ifaddrs* ifap = nullptr;
    
    if (getifaddrs(&ifap) == 0) {
        for (struct ifaddrs* ifa = ifap; ifa; ifa = ifa->ifa_next) {
            // Only include interfaces with IPv4 addresses
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                // Skip loopback interfaces
                if (!(ifa->ifa_flags & IFF_LOOPBACK)) {
                    interfaces.push_back(ifa->ifa_name);
                }
            }
        }
        freeifaddrs(ifap);
    }
    
    return interfaces;
}

bool NetworkManager::joinMulticastGroup() {
    struct ip_mreqn mreq;
    memset(&mreq, 0, sizeof(mreq));
    
    // Set the multicast group address
    if (inet_aton(multicastAddr.c_str(), &mreq.imr_multiaddr) == 0) {
        std::cerr << "Invalid multicast address: " << multicastAddr << std::endl;
        return false;
    }
    
    // Set the interface
    mreq.imr_address.s_addr = interfaceAddr;
    mreq.imr_ifindex = interfaceIndex;
    
    // Join the multicast group
    if (setsockopt(recvSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

bool NetworkManager::setSocketOptions() {
    // Allow reuse of address/port
    int optval = 1;
    if (setsockopt(recvSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set multicast TTL
    optval = 32;
    if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_TTL, &optval, sizeof(optval)) < 0) {
        std::cerr << "Failed to set multicast TTL: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set outgoing interface
    if (interfaceAddr != 0) {
        struct in_addr addr;
        addr.s_addr = interfaceAddr;
        if (setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to set multicast interface: " << strerror(errno) << std::endl;
            return false;
        }
    }
    
    // Set QoS (DSCP class AF41 - audio)
    optval = 0x88;  // IPTOS_PREC_INTERNETCONTROL | IPTOS_RELIABILITY
    if (setsockopt(sendSocket, IPPROTO_IP, IP_TOS, &optval, sizeof(optval)) < 0) {
        std::cerr << "Failed to set IP_TOS: " << strerror(errno) << std::endl;
        // Non-critical, continue anyway
    }
    
    return true;
}

bool NetworkManager::getInterfaceInfo() {
    if (interfaceName.empty()) {
        // Default to any available interface
        interfaceAddr = INADDR_ANY;
        interfaceIndex = 0;
        return true;
    }
    
    // Get interface information
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ - 1);
    
    int tempSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (tempSocket < 0) {
        std::cerr << "Failed to create temporary socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Get interface index
    if (ioctl(tempSocket, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "Failed to get interface index: " << strerror(errno) << std::endl;
        close(tempSocket);
        return false;
    }
    interfaceIndex = ifr.ifr_ifindex;
    
    // Get interface address
    if (ioctl(tempSocket, SIOCGIFADDR, &ifr) < 0) {
        std::cerr << "Failed to get interface address: " << strerror(errno) << std::endl;
        close(tempSocket);
        return false;
    }
    interfaceAddr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
    
    // Get interface MTU
    if (ioctl(tempSocket, SIOCGIFMTU, &ifr) < 0) {
        std::cerr << "Failed to get interface MTU: " << strerror(errno) << std::endl;
        // Non-critical, use default
        interfaceMTU = 1500;
    } else {
        interfaceMTU = ifr.ifr_mtu;
    }
    
    close(tempSocket);
    return true;
}

} // namespace aes67
