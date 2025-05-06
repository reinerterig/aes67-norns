// AES67Bridge.cpp Phase 2
#include "AES67Bridge.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <chrono>

namespace aes67 {

AES67Bridge::AES67Bridge() 
    : JackClient<2, 2>("aes67_bridge"), 
      mode(Mode::Inactive),
      bitDepth(24),
      packetTime(1000), // 1ms default
      bufferSize(0),
      threadRunning(false),
      networkActive(false),
      bufferLevel(0.0f)
{
    // Create network components
    network = std::make_unique<NetworkManager>();
    rtp = std::make_unique<RTPHandler>();
    ptp = std::make_unique<PTPSync>();
    converter = std::make_unique<AudioConverter>();
    
    std::cout << "AES67Bridge created" << std::endl;
}

AES67Bridge::~AES67Bridge() {
    // Stop networking
    stopNetworking();
    
    std::cout << "AES67Bridge destroyed" << std::endl;
}

void AES67Bridge::process(jack_nframes_t numFrames) {
    // In receive mode, read from network buffer and output to JACK
    if (mode == Mode::Receive && networkActive) {
        // Copy audio from network buffer to JACK output
        std::lock_guard<std::mutex> lock(bufferMutex);
        
        // Clear output if we don't have enough samples
        if (jackBuffer.size() < numFrames * 2) {
            clearBuffers(numFrames);
            return;
        }
        
        // Copy samples to output
        for (unsigned int i = 0; i < numFrames; i++) {
            sink[0][0][i] = jackBuffer[i * 2];
            sink[0][1][i] = jackBuffer[i * 2 + 1];
        }
        
        // Remove used samples from buffer
        jackBuffer.erase(jackBuffer.begin(), jackBuffer.begin() + numFrames * 2);
        
        // Update buffer level
        bufferLevel = static_cast<float>(jackBuffer.size() / 2) / static_cast<float>(bufferSize);
    }
    // In transmit mode, read from JACK input and send to network
    else if (mode == Mode::Transmit && networkActive) {
        // Copy audio from JACK input to network buffer
        std::lock_guard<std::mutex> lock(bufferMutex);
        
        // Append input samples to buffer
        for (unsigned int i = 0; i < numFrames; i++) {
            jackBuffer.push_back(source[0][0][i]);
            jackBuffer.push_back(source[0][1][i]);
        }
        
        // Update buffer level
        bufferLevel = static_cast<float>(jackBuffer.size() / 2) / static_cast<float>(bufferSize);
        
        // If in simple pass-through mode, also copy to output
        for (unsigned int i = 0; i < numFrames; i++) {
            sink[0][0][i] = source[0][0][i];
            sink[0][1][i] = source[0][1][i];
        }
    }
    // In inactive mode or if networking is not active, just pass through
    else {
        for (unsigned int i = 0; i < numFrames; i++) {
            sink[0][0][i] = source[0][0][i];
            sink[0][1][i] = source[0][1][i];
        }
    }
}

void AES67Bridge::setSampleRate(jack_nframes_t sr) {
    sampleRate = sr;
    
    // Update network components
    rtp->setSampleRate(sr);
    ptp->setSampleRate(sr);
    converter->setSampleRate(sr);
    
    // Recalculate buffer size
    resizeBuffers(calculatePacketSamples() * 20); // Buffer 20 packets
    
    std::cout << "Sample rate set to " << sr << " Hz" << std::endl;
}

bool AES67Bridge::setNetworkAddress(const std::string& address, int port) {
    if (networkActive) {
        std::cerr << "Cannot change network address while networking is active" << std::endl;
        return false;
    }
    
    return true;
}

bool AES67Bridge::setNetworkInterface(const std::string& interface) {
    if (networkActive) {
        std::cerr << "Cannot change network interface while networking is active" << std::endl;
        return false;
    }
    
    return network->setInterface(interface);
}

bool AES67Bridge::startNetworking() {
    if (networkActive) {
        std::cerr << "Networking is already active" << std::endl;
        return false;
    }
    
    if (mode == Mode::Inactive) {
        std::cerr << "Cannot start networking in inactive mode" << std::endl;
        return false;
    }
    
    // Initialize network components
    // TODO: Get these from configuration
    std::string multicastAddr = "239.69.83.133"; // Example AES67 multicast address
    int port = 5004; // Default AES67 port
    
    // Initialize PTP synchronization
    if (!ptp->initialize()) {
        std::cerr << "Failed to initialize PTP synchronization" << std::endl;
        return false;
    }
    
    // Initialize network
    if (!network->initialize(multicastAddr, port)) {
        std::cerr << "Failed to initialize network" << std::endl;
        ptp->shutdown();
        return false;
    }
    
    // Initialize RTP handler
    rtp->initialize(sampleRate, 2, 96); // L24 format, stereo
    
    // Initialize audio converter
    converter->initialize(sampleRate, 2, bitDepth);
    
    // Start network thread
    threadRunning = true;
    
    if (mode == Mode::Receive) {
        networkThread = std::thread(&AES67Bridge::networkReceiveLoop, this);
    } else {
        networkThread = std::thread(&AES67Bridge::networkTransmitLoop, this);
    }
    
    networkActive = true;
    std::cout << "AES67 networking started in " 
              << (mode == Mode::Receive ? "receive" : "transmit") 
              << " mode" << std::endl;
    
    return true;
}

bool AES67Bridge::stopNetworking() {
    if (!networkActive) {
        return true; // Already stopped
    }
    
    // Stop network thread
    threadRunning = false;
    
    if (networkThread.joinable()) {
        networkThread.join();
    }
    
    // Shutdown network components
    network->shutdown();
    ptp->shutdown();
    
    // Clear buffers
    std::lock_guard<std::mutex> lock(bufferMutex);
    jackBuffer.clear();
    networkBuffer.clear();
    
    networkActive = false;
    std::cout << "AES67 networking stopped" << std::endl;
    
    return true;
}

bool AES67Bridge::setMode(bool transmit) {
    if (networkActive) {
        std::cerr << "Cannot change mode while networking is active" << std::endl;
        return false;
    }
    
    mode = transmit ? Mode::Transmit : Mode::Receive;
    std::cout << "Mode set to " << (transmit ? "transmit" : "receive") << std::endl;
    
    return true;
}

void AES67Bridge::setBitDepth(int bits) {
    if (networkActive) {
        std::cerr << "Cannot change bit depth while networking is active" << std::endl;
        return;
    }
    
    if (bits != 16 && bits != 24 && bits != 32) {
        std::cerr << "Invalid bit depth: " << bits << ", must be 16, 24, or 32" << std::endl;
        return;
    }
    
    bitDepth = bits;
    converter->setBitDepth(bits);
    
    std::cout << "Bit depth set to " << bits << std::endl;
}

void AES67Bridge::setPacketTime(int microseconds) {
    if (networkActive) {
        std::cerr << "Cannot change packet time while networking is active" << std::endl;
        return;
    }
    
    // Check for valid AES67 packet times
    if (microseconds != 125 && microseconds != 250 && 
        microseconds != 333 && microseconds != 1000 && 
        microseconds != 4000) {
        std::cerr << "Invalid packet time: " << microseconds 
                  << "us, must be 125, 250, 333, 1000, or 4000" << std::endl;
        return;
    }
    
    packetTime = microseconds;
    
    // Recalculate buffer size
    resizeBuffers(calculatePacketSamples() * 20); // Buffer 20 packets
    
    std::cout << "Packet time set to " << microseconds << "us" << std::endl;
}

bool AES67Bridge::isNetworkActive() const {
    return networkActive;
}

float AES67Bridge::getBufferLevel() const {
    return bufferLevel;
}

int AES67Bridge::getPacketCount() const {
    return rtp->getPacketCount();
}

int AES67Bridge::getDroppedPackets() const {
    return rtp->getDroppedPackets();
}

const std::string& AES67Bridge::getMasterClock() const {
    return ptp->getMasterClockId();
}

bool AES67Bridge::isPTPSynchronized() const {
    return ptp->isSynchronized();
}

void AES67Bridge::networkReceiveLoop() {
    std::vector<uint8_t> packetBuffer(2048); // Buffer for incoming packets
    std::vector<float> audioBuffer;
    size_t bytesReceived;
    
    while (threadRunning) {
        // Receive a packet
        if (network->receivePacket(packetBuffer.data(), packetBuffer.size(), bytesReceived)) {
            // Process the packet
            RTPHandler::AudioData audio;
            if (rtp->parsePacket(packetBuffer.data(), bytesReceived, audio)) {
                // Convert audio from network format to float
                audioBuffer.resize(audio.frameCount * audio.channelCount);
                converter->intToFloat(
                    packetBuffer.data() + sizeof(RTPHandler), // Skip RTP header
                    audioBuffer.data(),
                    audio.frameCount
                );
                
                // Add to playback buffer
                std::lock_guard<std::mutex> lock(bufferMutex);
                jackBuffer.insert(jackBuffer.end(), audioBuffer.begin(), audioBuffer.end());
                
                // Trim buffer if it's getting too large
                if (jackBuffer.size() > bufferSize * 2) {
                    jackBuffer.resize(bufferSize * 2);
                }
            }
        }
        
        // Sleep a bit to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void AES67Bridge::networkTransmitLoop() {
    size_t packetSamples = calculatePacketSamples();
    std::vector<float> audioBuffer;
    std::vector<uint8_t> packetBuffer;
    RTPHandler::AudioData audio;
    
    while (threadRunning) {
        // Check if we have enough samples to send a packet
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            if (jackBuffer.size() >= packetSamples * 2) {
                // Copy samples to a temporary buffer
                audioBuffer.assign(jackBuffer.begin(), jackBuffer.begin() + packetSamples * 2);
                
                // Remove used samples from buffer
                jackBuffer.erase(jackBuffer.begin(), jackBuffer.begin() + packetSamples * 2);
            } else {
                // Not enough samples yet
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }
        
        // Set up audio data
        audio.samples = audioBuffer;
        audio.channelCount = 2;
        audio.sampleRate = sampleRate;
        audio.frameCount = packetSamples;
        
        // Convert audio from float to network format
        converter->processFloatToInt(audioBuffer, networkBuffer);
        
        // Create an RTP packet
        if (rtp->createPacket(audio, packetBuffer)) {
            // Send the packet
            network->sendPacket(packetBuffer.data(), packetBuffer.size());
        }
        
        // Sleep until it's time to send the next packet
        std::this_thread::sleep_for(std::chrono::microseconds(packetTime));
    }
}

void AES67Bridge::clearBuffers(size_t numFrames) {
    // Clear output buffers
    for (int ch = 0; ch < 2; ++ch) {
        memset(sink[0][ch], 0, numFrames * sizeof(float));
    }
}

void AES67Bridge::resizeBuffers(size_t numFrames) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    bufferSize = numFrames;
    jackBuffer.reserve(numFrames * 2 * 2); // Reserve space for stereo frames with some headroom
    networkBuffer.reserve(numFrames * 2 * (bitDepth / 8) + 100); // Reserve space for packet headers
    
    std::cout << "Buffer size set to " << numFrames << " frames (" 
              << (numFrames * 1000 / sampleRate) << "ms)" << std::endl;
}

size_t AES67Bridge::calculatePacketSamples() const {
    // Calculate samples per packet based on packet time and sample rate
    return (packetTime * sampleRate) / 1000000;
}

} // namespace aes67
