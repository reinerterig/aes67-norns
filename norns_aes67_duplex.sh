#!/bin/bash
# norns_aes67_duplex.sh
# Full-duplex AES67 bridge for monome norns using two instances of MAI

# Determine script location
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
MAI_PATH="$SCRIPT_DIR/AES67-JACK/mai"

# Check if MAI executable exists
if [ ! -f "$MAI_PATH" ]; then
    echo "Error: MAI executable not found at $MAI_PATH"
    echo "Make sure you've compiled the AES67-JACK implementation with 'make'"
    exit 1
fi

# Default parameters
RECV_ADDRESS="239.69.83.133"   # Default receive multicast address
SEND_ADDRESS="239.69.83.134"   # Default send multicast address (different to avoid collision)
PORT=5004                      # Default AES67 port
INTERFACE=""                   # Network interface (auto-detect)
SAMPLE_RATE=48000              # Most common AES67 sample rate
BIT_DEPTH=24                   # 24-bit audio
CHANNELS=2                     # Stereo
PACKET_TIME=1000               # 1ms packet time (common setting)
RECV_CLIENT="aes67_rx"         # JACK client name for receiver
SEND_CLIENT="aes67_tx"         # JACK client name for sender

# Custom port connection parameters
AUTO_CONNECT=true              # Whether to auto-connect to norns
NORNS_IN_PORTS="crone:input_1,crone:input_2"
NORNS_OUT_PORTS="crone:output_1,crone:output_2"

# Process command line options
while [[ $# -gt 0 ]]; do
    case $1 in
        --recv-address)
            RECV_ADDRESS="$2"
            shift 2
            ;;
        --send-address)
            SEND_ADDRESS="$2"
            shift 2
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -i|--interface)
            INTERFACE="$2"
            shift 2
            ;;
        -r|--rate)
            SAMPLE_RATE="$2"
            shift 2
            ;;
        -b|--bits)
            BIT_DEPTH="$2"
            shift 2
            ;;
        -c|--channels)
            CHANNELS="$2"
            shift 2
            ;;
        -t|--ptime)
            PACKET_TIME="$2"
            shift 2
            ;;
        --norns-in)
            NORNS_IN_PORTS="$2"
            shift 2
            ;;
        --norns-out)
            NORNS_OUT_PORTS="$2"
            shift 2
            ;;
        --no-auto-connect)
            AUTO_CONNECT=false
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo
            echo "This script runs two instances of MAI to create a full-duplex AES67 bridge"
            echo "for monome norns, functioning as a complete audio interface with simultaneous"
            echo "send and receive capabilities."
            echo
            echo "Options:"
            echo "  --recv-address ADDR    Set receive multicast address [default: 239.69.83.133]"
            echo "  --send-address ADDR    Set send multicast address [default: 239.69.83.134]"
            echo "  -p, --port PORT        Set port number [default: 5004]"
            echo "  -i, --interface IFACE  Set network interface [default: auto]"
            echo "  -r, --rate RATE        Set sample rate (44100, 48000, 96000) [default: 48000]"
            echo "  -b, --bits BITS        Set bit depth (16, 24, 32) [default: 24]"
            echo "  -c, --channels CHANS   Set channel count (1-8) [default: 2]"
            echo "  -t, --ptime TIME       Set packet time in microseconds [default: 1000]"
            echo "  --norns-in PORTS       Norns input ports [default: crone:input_1,crone:input_2]"
            echo "  --norns-out PORTS      Norns output ports [default: crone:output_1,crone:output_2]"
            echo "  --no-auto-connect      Don't auto-connect to norns ports (manual connection)"
            echo "  -h, --help             Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Function to start an MAI instance
start_mai() {
    local mode="$1"
    local client="$2"
    local address="$3"
    
    # Build the command
    local cmd="$MAI_PATH"
    cmd+=" --mode $mode"
    cmd+=" --address $address:$PORT"
    cmd+=" --client $client"
    cmd+=" --rate $SAMPLE_RATE"
    cmd+=" --bits $BIT_DEPTH"
    cmd+=" --channels $CHANNELS"
    cmd+=" --ptime $PACKET_TIME"
    
    # Add interface if specified
    if [[ -n "$INTERFACE" ]]; then
        cmd+=" --interface $INTERFACE"
    fi
    
    # Add verbose output
    cmd+=" --verbose"
    
    echo "Starting AES67 $mode bridge with:"
    echo "$cmd"
    echo
    
    # Start in background
    $cmd &
    
    # Return the PID
    echo $!
}

# Start both instances
echo "Starting full-duplex AES67 bridge..."
RECV_PID=$(start_mai "recv" "$RECV_CLIENT" "$RECV_ADDRESS")
SEND_PID=$(start_mai "send" "$SEND_CLIENT" "$SEND_ADDRESS")

echo "Receiver PID: $RECV_PID"
echo "Sender PID: $SEND_PID"

# Wait a moment for the JACK clients to be created
sleep 2

# Auto-connect if enabled
if $AUTO_CONNECT; then
    echo "Auto-connecting ports..."
    
    # Parse the norns input ports
    IFS=',' read -ra IN_PORTS <<< "$NORNS_IN_PORTS"
    # Parse the norns output ports
    IFS=',' read -ra OUT_PORTS <<< "$NORNS_OUT_PORTS"
    
    # Connect receiver outputs to norns inputs
    for i in "${!IN_PORTS[@]}"; do
        if [ $i -lt $CHANNELS ]; then
            port_num=$((i+1))
            echo "Connecting ${RECV_CLIENT}:output_${port_num} to ${IN_PORTS[$i]}"
            jack_connect "${RECV_CLIENT}:output_${port_num}" "${IN_PORTS[$i]}" 2>/dev/null || echo "Warning: Connection failed"
        fi
    done
    
    # Connect norns outputs to sender inputs
    for i in "${!OUT_PORTS[@]}"; do
        if [ $i -lt $CHANNELS ]; then
            port_num=$((i+1))
            echo "Connecting ${OUT_PORTS[$i]} to ${SEND_CLIENT}:input_${port_num}"
            jack_connect "${OUT_PORTS[$i]}" "${SEND_CLIENT}:input_${port_num}" 2>/dev/null || echo "Warning: Connection failed"
        fi
    done
    
    echo "Port connections complete."
    
    # Display all connections
    echo -e "\nCurrent JACK connections:"
    jack_lsp -c
fi

echo -e "\nAES67 bridge is running. Press Ctrl+C to stop."

# Function to clean up on exit
cleanup() {
    echo -e "\nStopping AES67 bridge..."
    kill $RECV_PID $SEND_PID 2>/dev/null
    wait $RECV_PID $SEND_PID 2>/dev/null
    echo "Exited."
    exit 0
}

# Register the cleanup function on script exit
trap cleanup SIGINT SIGTERM

# Wait for both processes
wait $RECV_PID $SEND_PID