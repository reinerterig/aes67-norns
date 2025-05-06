# AES67 Bridge for Monome Norns

This project adds AES67 audio-over-IP functionality to the monome norns sound computer, allowing it to send and receive audio over standard Ethernet networks using the professional AES67 standard.

## What is AES67?

AES67 is a professional audio-over-IP interoperability standard that allows high-performance audio transmission over networks with:
- Low latency (typically 1ms)
- High quality audio (up to 24-bit, 96kHz)
- PTP clock synchronization
- Standard network hardware (no special equipment needed)
- Compatibility with many professional audio devices

## Overview

This implementation allows your norns to:
1. Receive AES67 streams and route them to norns inputs
2. Send norns outputs as AES67 streams
3. Function as a complete network audio interface

The implementation is based on MAI (Mark's AES67 Implementation) with custom scripts to integrate with norns' JACK audio system.

## Installation on Norns

### Prerequisites

First, ensure your norns is connected to the network via Ethernet. While AES67 can work over WiFi, a wired connection is strongly recommended for reliable low-latency performance.

Connect to your norns via SSH:

```bash
ssh we@norns.local
```

### Installation Steps

1. Install required dependencies:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libsamplerate0-dev libjack-jackd2-dev git
```

2. Clone this repository:

```bash
cd ~
git clone https://github.com/yourusername/norns-aes67.git
cd norns-aes67
```

3. Build the AES67-JACK implementation:

```bash
cd AES67-JACK
make
cd ..
```

4. Make the scripts executable:

```bash
chmod +x norns_aes67_duplex.sh
chmod +x norns_aes67_bridge.sh
```

5. Test the installation:

```bash
./norns_aes67_duplex.sh
```

You should see output indicating the AES67 bridge has started.

### Running at Startup (Optional)

To have the AES67 bridge start automatically when norns boots:

1. Create a systemd service file:

```bash
sudo nano /etc/systemd/system/aes67bridge.service
```

2. Add the following contents (adjust paths if necessary):

```
[Unit]
Description=AES67 Bridge for Norns
After=network.target sound.target

[Service]
Type=simple
User=we
WorkingDirectory=/home/we/norns-aes67
ExecStart=/home/we/norns-aes67/norns_aes67_duplex.sh
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

3. Enable and start the service:

```bash
sudo systemctl enable aes67bridge.service
sudo systemctl start aes67bridge.service
```

4. Check its status:

```bash
sudo systemctl status aes67bridge.service
```

## Connecting from macOS

### Method 1: Using Dante Virtual Soundcard (Recommended)

For the simplest integration with macOS, Audinate's Dante Virtual Soundcard (DVS) provides excellent compatibility with AES67:

1. Purchase and install [Dante Virtual Soundcard](https://www.audinate.com/products/software/dante-virtual-soundcard) from Audinate
2. In DVS settings, enable AES67 mode
3. Configure DVS to use the same sample rate as your norns (48kHz by default)
4. Start DVS
5. The norns AES67 streams should be visible in Dante Controller
6. Map the streams to Dante Virtual Soundcard channels
7. In your macOS audio applications, select Dante Virtual Soundcard as your audio interface

### Method 2: Using Ravenna/AES67 Virtual Sound Card

1. Download and install [Merging RAVENNA/AES67 Virtual Sound Card](https://www.merging.com/products/interfaces/ravenna-aes67-vsc) 
2. Configure it to use the same network interface as your norns connection
3. Enable AES67 mode in the settings
4. Set the sample rate to match norns (48kHz by default)
5. The norns AES67 streams should be automatically discovered
6. Select the Ravenna/AES67 Virtual Sound Card as the audio device in your macOS applications

### Method 3: Using Hardware AES67 Adapters

For professional setups, you can use hardware AES67 adapters:

1. Connect a hardware AES67 adapter to your Mac (via USB or other interface)
   - Options include [Ferrofish Pulse 16 DX](https://www.ferrofish.com/pulse-16-dx/), [Focusrite RedNet](https://pro.focusrite.com/), or [DirectOut EXBOX.RAV](https://www.directout.eu/en/products/exbox-rav/)
2. Configure the adapter for AES67 mode
3. Use the manufacturer's control software to discover norns AES67 streams
4. Map the streams to the adapter's input/output channels

## Advanced Configuration

### Customizing Network Settings

By default, the AES67 bridge uses multicast addresses 239.69.83.133 (receive) and 239.69.83.134 (send). You can customize these:

```bash
./norns_aes67_duplex.sh --recv-address 239.69.83.140 --send-address 239.69.83.141
```

### Specifying Network Interface

If your norns has multiple network interfaces, specify which one to use:

```bash
./norns_aes67_duplex.sh --interface eth0
```

### Custom Audio Quality Settings

You can adjust audio quality settings:

```bash
./norns_aes67_duplex.sh --rate 96000 --bits 24 --ptime 125
```

Lower `ptime` values (packet time in microseconds) reduce latency but require more network performance.

### Custom Port Names

If your norns uses different JACK port names:

```bash
./norns_aes67_duplex.sh --norns-in "system:playback_1,system:playback_2" --norns-out "system:capture_1,system:capture_2"
```

## Troubleshooting

### Checking AES67 Stream Status

To check if your AES67 streams are active:

```bash
jack_lsp -c
```

This will show all JACK ports and their connections.

### Network Issues

If you're having trouble discovering AES67 streams:

1. Ensure multicast traffic is allowed on your network
2. Check that PTP (Precision Time Protocol) traffic isn't being blocked
3. Try a direct Ethernet connection between norns and computer
4. Make sure both devices are on the same subnet

### Audio Dropouts or Glitches

If you experience audio dropouts:

1. Try increasing packet time (`--ptime 1000` or higher)
2. Ensure you're using a wired Ethernet connection
3. Try a different network interface if available
4. Check for network congestion with other traffic

## Technical Details

### Port Connections

The full-duplex bridge creates two separate JACK clients:

1. `aes67_rx` - Receives AES67 audio and routes it to norns
2. `aes67_tx` - Takes norns audio and sends it as AES67

The connections are:

```
[AES67 Network RX] <--> [aes67_rx] ---> [norns inputs]
                        [aes67_tx] <--- [norns outputs]
```

### AES67 Discovery

The bridge implementation includes SAP (Session Announcement Protocol) for service discovery. Compatible AES67 devices and software should automatically discover the norns AES67 streams.

### PTP Synchronization

The bridge automatically handles PTP clock synchronization, keeping audio in sync with other AES67 devices on the network.

## Credits

- Original MAI implementation by [Mark Hills](https://github.com/marcan/)
- AES67 Bridge wrapper for monome norns by [Your Name]