// JackClient.h - JACK Audio client interface template
#pragma once

#include <array>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cstring>  // Add this for string functions

#include <jack/jack.h>

namespace aes67 {

template<int NumIns, int NumOuts>
class JackClient {
private:
    static_assert(NumIns % 2 == 0, "non-even input count");
    static_assert(NumOuts % 2 == 0, "non-even output count");

    typedef const jack_default_audio_sample_t* Source[2];
    typedef jack_default_audio_sample_t* Sink[2];

    std::array<jack_port_t*, NumIns> inPort;
    std::array<jack_port_t*, NumOuts> outPort;
    const char *name;

protected:
    jack_client_t *client{};
    std::array<Source, NumIns/2> source;
    std::array<Sink, NumOuts/2> sink;
    float sampleRate;

private:
    // Set up pointers for the current buffer
    void preProcess(jack_nframes_t numFrames) {
        int j=0;
        for(int i=0; i<NumIns/2; ++i) {
            source[i][0] = static_cast<const float*>(jack_port_get_buffer(inPort[j++], numFrames));
            source[i][1] = static_cast<const float*>(jack_port_get_buffer(inPort[j++], numFrames));
        }
        j = 0;
        for(int i=0; i<NumOuts/2; ++i) {
            sink[i][0] = static_cast<float*>(jack_port_get_buffer(outPort[j++], numFrames));
            sink[i][1] = static_cast<float*>(jack_port_get_buffer(outPort[j++], numFrames));
        }
    }
    
    // Process using our source and sink pointers.
    // Subclasses must implement this!
    virtual void process(jack_nframes_t numFrames) = 0;

    virtual void setSampleRate(jack_nframes_t sr) = 0;

private:
    // Static handlers for JACK API
    static int callback(jack_nframes_t numFrames, void *data) {
        auto *self = (JackClient*)(data);
        self->preProcess(numFrames);
        self->process(numFrames);
        return 0;
    }
    
    // Static handler for shutdown from JACK
    static void jack_shutdown(void* data) {
        (void)data;
        std::cerr << "JACK server shutdown detected. Exiting." << std::endl;
        exit(1);
    }

public:
    virtual void setup() {
        using std::cerr;
        using std::cout;
        using std::endl;
        
        jack_status_t status;
        client = jack_client_open(name, JackNullOption, &status, nullptr);
        
        if(client == nullptr) {
            std::cerr << "jack_client_open() failed; status = " << status << endl;
            if (status & JackServerFailed) {
                cerr << "unable to connect to JACK server" << endl;
            }
            throw std::runtime_error("Failed to connect to JACK server");
        }
        
        if (status & JackServerStarted) {
            cout << "JACK server started" << endl;
        }
        if (status & JackNameNotUnique) {
            name = jack_get_client_name(client);
            cout << "unique name `" << name << "' assigned" << endl;
        }

        jack_set_process_callback(client, JackClient::callback, this);
        jack_on_shutdown(client, jack_shutdown, this);

        sampleRate = jack_get_sample_rate(client);
        std::cout << "JACK sample rate: " << sampleRate << " Hz" << std::endl;
        this->setSampleRate(sampleRate);

        for(int i=0; i<NumIns; ++i) {
            std::ostringstream os;
            os << "input_" << (i+1);
            // Create a copy of the string instead of using strdup
            std::string portNameStr = os.str();
            const char* portName = portNameStr.c_str();
            inPort[i] = jack_port_register(client, portName,
                                          JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
            if(inPort[i] == nullptr) {
                throw std::runtime_error("failed to register input port");
            }
        }

        for(int i=0; i<NumOuts; ++i) {
            std::ostringstream os;
            os << "output_" << (i+1);
            // Create a copy of the string instead of using strdup
            std::string portNameStr = os.str();
            const char* portName = portNameStr.c_str();
            outPort[i] = jack_port_register(client, portName,
                                          JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            if(outPort[i] == nullptr) {
                throw std::runtime_error("failed to register output port");
            }
        }
    }

    void cleanup() {
        jack_client_close(client);
    }

    void start() {
        if (jack_activate(client)) {
            throw std::runtime_error("client failed to activate");
        }
        std::cout << "JACK client activated" << std::endl;
    }

    void stop() {
        jack_deactivate(client);
    }

    void connectAdcPorts() {
        const char **ports = jack_get_ports(client, nullptr, nullptr,
                                          JackPortIsPhysical|JackPortIsOutput);

        if (ports == nullptr) {
            throw std::runtime_error("no physical capture ports found");
        }

        for(int i=0; i<NumIns; ++i) {
            if(i > 1) { break; }
            if (jack_connect(client, ports[i], jack_port_name(inPort[i]))) {
                std::cerr << "failed to connect input port " << i << std::endl;
                throw std::runtime_error("connectAdcPorts() failed");
            }
        }
        free(ports);
    }

    void connectDacPorts() {
        const char **ports = jack_get_ports(client, nullptr, nullptr,
                                          JackPortIsPhysical|JackPortIsInput);

        if (ports == nullptr) {
            throw std::runtime_error("no physical playback ports found");
        }

        for(int i=0; i<NumOuts; ++i) {
            if(i > 1) { break; }
            if (jack_connect(client, jack_port_name(outPort[i]), ports[i])) {
                std::cerr << "failed to connect output port " << i << std::endl;
                throw std::runtime_error("failed to connect output port");
            }
        }
        free(ports);
    }

    const char* getInputPortName(int idx) {
        return jack_port_name(inPort[idx]);
    }

    const char* getOutputPortName(int idx) {
        return jack_port_name(outPort[idx]);
    }

    int getNumSinks() { return NumIns/2; }
    int getNumSources() { return NumOuts/2; }

    // Connect ports by name
    bool connectPorts(const char* source, const char* destination) {
        if (jack_connect(client, source, destination)) {
            std::cerr << "Failed to connect " << source << " to " << destination << std::endl;
            return false;
        }
        return true;
    }

    explicit JackClient(const char* n) : name(n), client(nullptr) {
        std::cout << "Constructed JackClient: " << name << std::endl;
    }
    
    virtual ~JackClient() = default;
};

} // namespace aes67
