# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This project integrates AES67 audio-over-IP compatibility with the monome norns sound computer. It uses Mark's AES67 Implementation (MAI) to bridge between JACK audio and AES67 network audio. The implementation connects the norns crone audio inputs/outputs to AES67 streams.

The project provides two modes:
1. Full-duplex mode with simultaneous send/receive capabilities (using dual MAI instances)
2. Single-direction mode for legacy support

## Build Commands

### Building the MAI Implementation

```bash
cd AES67-JACK
make
```

### Running the Full-Duplex Bridge (Recommended)

```bash
./norns_aes67_duplex.sh [options]
```

### Running in Single Direction Mode

```bash
./norns_aes67_bridge.sh [options]
```

## Architecture

The system consists of three main components:

1. **MAI (Mark's AES67 Implementation)**: A C implementation that handles:
   - JACK audio connections
   - RTP packet processing
   - PTP clock synchronization
   - SAP announcements (for AES67 discovery)

2. **norns_aes67_duplex.sh**: A shell script that:
   - Launches two MAI instances (one for receiving, one for sending)
   - Manages JACK connections between MAI and norns
   - Provides a full audio interface experience

3. **norns_aes67_bridge.sh**: A legacy script for single-direction operation

The duplex system architecture:
```
[AES67 Network RX] → [MAI recv instance] → [norns crone inputs]
[norns crone outputs] → [MAI send instance] → [AES67 Network TX]
```

## Important Files

- `/AES67-JACK/`: Contains the MAI implementation
- `/norns_aes67_duplex.sh`: Main script for full-duplex operation
- `/norns_aes67_bridge.sh`: Legacy script for single-direction operation
- `/README.md`: Usage documentation

## Port Connection Logic

The port connection logic in `norns_aes67_duplex.sh` works as follows:

1. Two MAI instances are launched with different JACK client names
2. After a brief delay to ensure clients are registered
3. The script uses `jack_connect` to establish connections:
   - Connect receiver outputs to norns inputs
   - Connect norns outputs to sender inputs

Default port format is `crone:input_X` and `crone:output_X` but this can be customized via parameters.

## Notes for Development

- When making changes, focus on the shell script wrappers rather than modifying the MAI core
- The default port naming assumes standard norns JACK port names
- Each MAI instance needs its own multicast address to avoid collisions
- PTP synchronization is handled automatically by MAI
- Different client names must be used for the two MAI instances to avoid conflicts