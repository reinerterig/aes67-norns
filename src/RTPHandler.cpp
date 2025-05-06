// RTPHandler.cpp
#include "RTPHandler.h"
#include <cstring>
#include <iostream>
#include <random>
#include <algorithm>
#include <arpa/inet.h>  // For htonl, htons

namespace aes67 {

// RTP header structure
struct RTPHeader {
    uint8_t  vpxcc;      // Version(2), P(1), X(1), CC(4)
    uint8_t  mpt;        // Marker(1), PT(7)
    uint16_t seq;        // Packet Sequence Number
    uint32_t timestamp;  // Timestamp
    uint32_t ssrc;       // Synchronization source identifier
};

RTPHandler::RTPHandler()
    : ssrc(0), sequenceNumber(0), timestamp(0), 
      sampleRate(48000), channelCount(2), payloadType(96),
      expectedSequence(0), packetCount(0), droppedPackets(0), outOfOrderPackets(0)
{
    // Initialize random SSRC and sequence number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> ssrcDist(1, UINT32_MAX);
    std::uniform_int_distribution<uint16_t> seqDist(0, UINT16_MAX);
    
    ssrc = ssrcDist(gen);
    sequenceNumber = seqDist(gen);
    expectedSequence = sequenceNumber;
    
    // Initialize packet buffer
    for (auto& entry : packetBuffer) {
        entry.valid = false;
    }
}

RTPHandler::~RTPHandler() {
    // Nothing specific to clean up
}

void RTPHandler::initialize(uint32_t rate, uint16_t channels, uint16_t type) {
    setSampleRate(rate);
    setChannelCount(channels);
    setPayloadType(type);
}

void RTPHandler::setSampleRate(uint32_t rate) {
    sampleRate = rate;
}

void RTPHandler::setChannelCount(uint16_t channels) {
    channelCount = channels;
}

void RTPHandler::setPayloadType(uint16_t type) {
    payloadType = type;
}

bool RTPHandler::createPacket(const AudioData& audio, std::vector<uint8_t>& packet) {
    if (audio.samples.empty() || audio.channelCount == 0 || audio.frameCount == 0) {
        return false;
    }
    
    // Calculate the packet size
    size_t payloadSize = audio.frameCount * audio.channelCount * sizeof(int32_t);
    size_t packetSize = sizeof(RTPHeader) + payloadSize;
    
    // Resize the packet buffer
    packet.resize(packetSize);
    
    // Set up the RTP header
    RTPHeader* header = reinterpret_cast<RTPHeader*>(packet.data());
    header->vpxcc = 0x80;  // Version 2, no padding, no extension, 0 CSRCs
    header->mpt = static_cast<uint8_t>(payloadType & 0x7F);  // No marker, payload type
    header->seq = htons(sequenceNumber++);
    header->timestamp = htonl(timestamp);
    header->ssrc = htonl(ssrc);
    
    // Update timestamp for next packet (assumes 48kHz sample rate by default)
    timestamp += audio.frameCount;
    
    // Copy audio samples to the payload
    // Note: This is a simplified implementation that assumes the audio data
    // is already in the correct format. A real implementation would need to
    // convert from float to the appropriate format (e.g., L24).
    uint8_t* payload = packet.data() + sizeof(RTPHeader);
    memcpy(payload, audio.samples.data(), payloadSize);
    
    // Update statistics
    packetCount++;
    
    return true;
}

bool RTPHandler::parsePacket(const uint8_t* data, size_t size, AudioData& audio) {
    if (size < sizeof(RTPHeader)) {
        return false;
    }
    
    // Parse the RTP header
    const RTPHeader* header = reinterpret_cast<const RTPHeader*>(data);
    
    // Check version (must be 2)
    if ((header->vpxcc & 0xC0) != 0x80) {
        return false;
    }
    
    // Extract sequence number and timestamp
    uint16_t seq = ntohs(header->seq);
    uint32_t ts = ntohl(header->timestamp);
    
    // Calculate payload size and extract payload
    size_t payloadSize = size - sizeof(RTPHeader);
    const uint8_t* payload = data + sizeof(RTPHeader);
    
    // Update our expected sequence number
    if (seq != expectedSequence) {
        outOfOrderPackets++;
        // In a real implementation, we would handle out-of-order packets more carefully
    }
    expectedSequence = seq + 1;
    
    // Calculate frames based on payload size and channel count
    // This assumes L24 (24-bit) format by default
    size_t bytesPerSample = 3;  // 24-bit = 3 bytes
    size_t frameCount = payloadSize / (channelCount * bytesPerSample);
    
    // Prepare the audio data structure
    audio.channelCount = channelCount;
    audio.sampleRate = sampleRate;
    audio.frameCount = frameCount;
    audio.samples.resize(frameCount * channelCount);
    
    // Copy and convert payload to audio samples
    // In a real implementation, this would convert from the network format (e.g., L24)
    // to float samples for JACK. This is a simplified placeholder.
    // Actual conversion should be done by AudioConverter class.
    for (size_t i = 0; i < frameCount * channelCount; i++) {
        // This is where we would convert from int24 to float
        // placeholder - in reality, AudioConverter would do this
        audio.samples[i] = 0.0f;
    }
    
    // Update statistics
    packetCount++;
    
    return true;
}

void RTPHandler::addPacketToBuffer(const uint8_t* data, size_t size) {
    if (size < sizeof(RTPHeader)) {
        return;
    }
    
    // Parse the RTP header
    const RTPHeader* header = reinterpret_cast<const RTPHeader*>(data);
    uint16_t seq = ntohs(header->seq);
    
    // Calculate sequence difference
    int16_t seqDiff = seq - expectedSequence;
    
    // If packet is too old, drop it
    if (seqDiff < -MAX_BUFFER_PACKETS/2) {
        droppedPackets++;
        return;
    }
    
    // If packet is too far in the future, we might have missed many packets
    if (seqDiff > MAX_BUFFER_PACKETS/2) {
        // Reset our expected sequence
        expectedSequence = seq;
        droppedPackets++;
        // Clear the buffer in this case
        for (auto& entry : packetBuffer) {
            entry.valid = false;
        }
    }
    
    // Get buffer index for this sequence number
    uint16_t idx = getBufferIndex(seq);
    
    // Lock the buffer
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    // Store packet in buffer
    packetBuffer[idx].data.resize(size);
    memcpy(packetBuffer[idx].data.data(), data, size);
    packetBuffer[idx].sequenceNumber = seq;
    packetBuffer[idx].valid = true;
    
    // If this is an out of order packet
    if (seq != expectedSequence && seqDiff > 0) {
        outOfOrderPackets++;
    }
    
    // Process the buffer to handle any contiguous packets
    processBuffer();
}

bool RTPHandler::getNextAudioFrame(AudioData& audio) {
    // This would normally retrieve the next frame from the processed buffer
    // In a real implementation, we would maintain a queue of processed frames
    // For simplicity, we're just doing a placeholder here
    return false;
}

uint16_t RTPHandler::getBufferIndex(uint16_t sequence) const {
    return sequence % MAX_BUFFER_PACKETS;
}

void RTPHandler::processBuffer() {
    // This method would process the buffer to handle any contiguous packets
    // It would extract audio data from packets in sequence and make it available
    // for getNextAudioFrame()
    
    // Check if we have the next expected packet
    uint16_t idx = getBufferIndex(expectedSequence);
    
    while (packetBuffer[idx].valid && packetBuffer[idx].sequenceNumber == expectedSequence) {
        // Process this packet
        // In a real implementation, we would extract the audio data and
        // add it to our processed frame queue
        
        // Mark as processed
        packetBuffer[idx].valid = false;
        
        // Move to next packet
        expectedSequence++;
        idx = getBufferIndex(expectedSequence);
    }
    
    // If we don't have the next expected packet but we've waited long enough,
    // we might need to skip ahead to avoid excessive delay
    // This would be implemented in a real system
}

} // namespace aes67
