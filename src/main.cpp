// main.cpp Phase 2
#include "AES67Bridge.h"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <getopt.h>

// Global bridge instance for signal handling
aes67::AES67Bridge* bridge = nullptr;

// Signal handler
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    
    if (bridge) {
        std::cout << "Stopping AES67 bridge...\n";
        bridge->stopNetworking();
        bridge->stop();
        bridge->cleanup();
    }
    
    exit(signum);
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -m, --mode <mode>          Set mode (transmit or receive)\n"
              << "  -a, --address <address>    Set multicast address\n"
              << "  -p, --port <port>          Set port number\n"
              << "  -i, --interface <name>     Set network interface\n"
              << "  -b, --bit-depth <bits>     Set bit depth (16, 24, or 32)\n"
              << "  -t, --packet-time <us>     Set packet time in microseconds\n"
              << "                             (125, 250, 333, 1000, or 4000)\n"
              << "  -s, --start                Start networking after initialization\n"
              << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "AES67 Bridge for Monome Norns - Phase 2\n";
    std::cout << "======================================\n";
    
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Default values
    bool transmitMode = false;
    std::string address = "239.69.83.133";
    int port = 5004;
    std::string interface = "";
    int bitDepth = 24;
    int packetTime = 1000;
    bool startNetworking = false;
    
    // Parse command line options
    static struct option long_options[] = {
        {"help",        no_argument,       0, 'h'},
        {"mode",        required_argument, 0, 'm'},
        {"address",     required_argument, 0, 'a'},
        {"port",        required_argument, 0, 'p'},
        {"interface",   required_argument, 0, 'i'},
        {"bit-depth",   required_argument, 0, 'b'},
        {"packet-time", required_argument, 0, 't'},
        {"start",       no_argument,       0, 's'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "hm:a:p:i:b:t:s", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 'm':
                if (std::string(optarg) == "transmit") {
                    transmitMode = true;
                } else if (std::string(optarg) == "receive") {
                    transmitMode = false;
                } else {
                    std::cerr << "Invalid mode: " << optarg << ". Must be 'transmit' or 'receive'.\n";
                    return 1;
                }
                break;
            case 'a':
                address = optarg;
                break;
            case 'p':
                port = std::stoi(optarg);
                break;
            case 'i':
                interface = optarg;
                break;
            case 'b':
                bitDepth = std::stoi(optarg);
                if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) {
                    std::cerr << "Invalid bit depth: " << bitDepth << ". Must be 16, 24, or 32.\n";
                    return 1;
                }
                break;
            case 't':
                packetTime = std::stoi(optarg);
                if (packetTime != 125 && packetTime != 250 && 
                    packetTime != 333 && packetTime != 1000 && 
                    packetTime != 4000) {
                    std::cerr << "Invalid packet time: " << packetTime 
                              << ". Must be 125, 250, 333, 1000, or 4000.\n";
                    return 1;
                }
                break;
            case 's':
                startNetworking = true;
                break;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    try {
        // Create and configure the bridge
        bridge = new aes67::AES67Bridge();
        
        // Set up JACK client
        bridge->setup();
        
        // Configure the bridge
        bridge->setMode(transmitMode);
        bridge->setBitDepth(bitDepth);
        bridge->setPacketTime(packetTime);
        
        if (!interface.empty()) {
            bridge->setNetworkInterface(interface);
        }
        
        bridge->setNetworkAddress(address, port);
        
        // Start the JACK client
        bridge->start();
        
        // Connect to system ports by default (can be customized later)
        try {
            bridge->connectAdcPorts();
            bridge->connectDacPorts();
            std::cout << "Connected to system audio ports\n";
        } catch (const std::exception& e) {
            std::cerr << "Warning: " << e.what() << std::endl;
            std::cerr << "Port connections must be made manually\n";
        }
        
        // Start networking if requested
        if (startNetworking) {
            if (!bridge->startNetworking()) {
                std::cerr << "Failed to start networking\n";
            } else {
                std::cout << "Networking started in " 
                          << (transmitMode ? "transmit" : "receive") 
                          << " mode\n";
            }
        } else {
            std::cout << "Networking not started. Use --start or call startNetworking() to begin.\n";
        }
        
        // Main loop - just keep running and handle JACK callbacks
        std::cout << "AES67 Bridge is running. Press Ctrl+C to exit.\n";
        
        // Stay alive until interrupted
        while (true) {
            sleep(1);
            
            // Print some status information periodically
            if (bridge->isNetworkActive()) {
                std::cout << "Buffer level: " << (bridge->getBufferLevel() * 100) << "%, "
                          << "Packets: " << bridge->getPacketCount() << ", "
                          << "Dropped: " << bridge->getDroppedPackets() 
                          << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
