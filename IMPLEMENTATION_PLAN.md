# AES67 Bridge for Monome Norns: Implementation Plan and Next Steps

## What We've Accomplished

1. **Created a Full-Duplex AES67 Bridge**
   - Adapted the MAI (Mark's AES67 Implementation) for norns
   - Implemented simultaneous send/receive functionality
   - Set up proper JACK audio routing to norns crone inputs/outputs
   - Built a user-friendly wrapper script with configuration options

2. **Published to GitHub**
   - Repository available at: https://github.com/reinerterig/aes67-norns
   - Complete with documentation, scripts, and source code

## Testing Plan with Dante on macOS

### Step 1: Set Up Norns
1. SSH into your norns: `ssh we@norns.local`
2. Install dependencies:
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential libsamplerate0-dev libjack-jackd2-dev git
   ```
3. Clone the repository:
   ```bash
   cd ~
   git clone https://github.com/reinerterig/aes67-norns.git
   cd aes67-norns
   ```
4. Build the implementation:
   ```bash
   cd AES67-JACK
   make
   cd ..
   ```
5. Connect your norns via Ethernet cable to the same network as your Mac

### Step 2: Set Up Dante Virtual Soundcard on macOS
1. Install Dante Virtual Soundcard from Audinate
2. Open Dante Virtual Soundcard preferences
3. Set the following options:
   - Audio Interface: AES67 mode
   - Sample Rate: 48kHz (must match norns settings)
   - Channel Count: At least 2x2
   - Network Interface: Select the interface connected to your norns
4. Apply settings and start Dante Virtual Soundcard

### Step 3: Install Dante Controller
1. Download and install Dante Controller from Audinate
2. Launch Dante Controller to manage device routing

### Step 4: Start AES67 Bridge on norns
1. On your norns (via SSH), run:
   ```bash
   cd ~/aes67-norns
   ./norns_aes67_duplex.sh
   ```
2. Note the output showing the multicast addresses being used:
   - Receive address (default: 239.69.83.133)
   - Send address (default: 239.69.83.134)

### Step 5: Connect the Devices
1. In Dante Controller on macOS:
   - Look for devices named "aes67_rx" and "aes67_tx" (or similar)
   - If they don't appear, click "Device Info" and verify AES67 mode is enabled
   - If they still don't appear, you may need to manually add them as AES67 devices
   
2. To manually add AES67 devices in Dante Controller:
   - Go to "Device" menu → "Add AES67 Device"
   - Enter the multicast addresses used in Step 4
   - Name them "norns_rx" and "norns_tx" for clarity

3. Create routing in Dante Controller:
   - Connect "norns_tx" outputs to Dante Virtual Soundcard inputs
   - Connect Dante Virtual Soundcard outputs to "norns_rx" inputs

### Step 6: Test the Connection
1. On your Mac:
   - Open your DAW or audio application
   - Select Dante Virtual Soundcard as your audio interface
   - Play audio to test transmission to norns

2. On your norns:
   - Run a patch that outputs audio
   - Monitor on your Mac to verify the signal is being received

## Troubleshooting Tips

- **Network Issues**: 
  - Ensure both devices are on the same subnet
  - Check that multicast traffic is allowed on your network
  - Try using a direct Ethernet connection

- **Discovery Problems**:
  - Verify PTP timing settings in Dante Controller
  - Try manually adding devices as described above
  - Check firewall settings on macOS

- **Audio Glitches**:
  - Increase packet time with `--ptime 2000` for better stability
  - Try different buffer sizes in your audio application
  - Ensure you're using a wired connection

## Next Steps for Future Sessions

1. **Fine-tune Configuration**:
   - Optimize latency settings for your specific setup
   - Adjust sample rates if needed
   - Set up automatic startup via systemd

2. **Integration Enhancements**:
   - Create a norns script to control AES67 settings
   - Add visual monitoring of connection status

3. **Advanced Features**:
   - Multiple channel support beyond stereo
   - Integration with other AES67 hardware
   - Custom discovery and routing UI

## Important: GitHub Repository Management

### For Future Agents Working With This Repository

If you need to make changes to the GitHub repository, NEVER make assumptions about authentication details. Always follow these exact steps:

1. **Ask for explicit permission** before pushing any changes to GitHub
   - "Would you like me to push these changes to GitHub?"

2. **Request specific authentication information** from the user:
   - "To push to GitHub, I'll need your GitHub username and a Personal Access Token. Please provide this information explicitly."
   - "Would you like me to use your existing GitHub configuration or set up a new one?"

3. **Verify repository details**:
   - "Should I push to the existing 'aes67-norns' repository or a different one?"
   - "Which branch should I push to?"

4. **NEVER** assume GitHub credentials or commit the user's authentication tokens to files in the repository

### Required Information for GitHub Operations

Before performing any GitHub operations, collect the following information explicitly from the user:
1. GitHub username: `reinerterig`
2. Repository name: `aes67-norns` (or as specified by user)
3. Branch to use: `main` (or as specified by user)
4. A newly created GitHub Personal Access Token (these expire and should never be reused from documentation)

Remember that Git configurations containing authentication tokens should be treated as sensitive and should never be committed to the repository.

When you're ready to continue, simply refer back to this implementation plan, clone the repository from GitHub, and follow the testing steps outlined above.