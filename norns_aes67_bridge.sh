#!/bin/bash
# norns_aes67_bridge.sh
# AES67 bridge for monome norns using MAI (Mark's AES67 Implementation)

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
MODE="recv"               # recv or send
MULTICAST="239.69.83.133" # Default AES67 multicast address
PORT=5004                 # Default AES67 port
INTERFACE=""              # Network interface (auto-detect)
SAMPLE_RATE=48000         # Most common AES67 sample rate
BIT_DEPTH=24              # 24-bit audio
CHANNELS=2                # Stereo
PACKET_TIME=1000          # 1ms packet time (common setting)
JACK_CLIENT="aes67_norns" # JACK client name

# Process command line options
while [[ $# -gt 0 ]]; do
    case $1 in
        -m|--mode)
            MODE="$2"
            shift 2
            ;;
        -a|--address)
            MULTICAST="$2"
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
        -h|--help)
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "  -m, --mode MODE       Set mode (recv or send) [default: recv]"
            echo "  -a, --address ADDR    Set multicast address [default: 239.69.83.133]"
            echo "  -p, --port PORT       Set port number [default: 5004]"
            echo "  -i, --interface IFACE Set network interface [default: auto]"
            echo "  -r, --rate RATE       Set sample rate (44100, 48000, 96000) [default: 48000]"
            echo "  -b, --bits BITS       Set bit depth (16, 24, 32) [default: 24]"
            echo "  -c, --channels CHANS  Set channel count (1-8) [default: 2]"
            echo "  -t, --ptime TIME      Set packet time in microseconds [default: 1000]"
            echo "  -h, --help            Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Validate mode
if [[ "$MODE" != "recv" && "$MODE" != "send" ]]; then
    echo "Error: Mode must be 'recv' or 'send'"
    exit 1
fi

# Prepare port connections for monome norns
# The naming format may need adjustment based on your norns JACK port names
if [[ "$MODE" == "recv" ]]; then
    # When receiving AES67, we connect mai outputs to crone inputs
    PORTS="output_1:crone_in_1,output_2:crone_in_2"
else
    # When sending AES67, we connect mai inputs to crone outputs
    PORTS="input_1:crone_out_1,input_2:crone_out_2"
fi

# Build the command
CMD="$MAI_PATH"
CMD+=" --mode $MODE"
CMD+=" --address $MULTICAST:$PORT"
CMD+=" --client $JACK_CLIENT"
CMD+=" --ports $PORTS"
CMD+=" --rate $SAMPLE_RATE"
CMD+=" --bits $BIT_DEPTH"
CMD+=" --channels $CHANNELS"
CMD+=" --ptime $PACKET_TIME"

# Add interface if specified
if [[ -n "$INTERFACE" ]]; then
    CMD+=" --interface $INTERFACE"
fi

# Add verbose output
CMD+=" --verbose"

# Display the command
echo "Starting AES67 bridge with:"
echo "$CMD"
echo

# Execute the command
exec $CMD