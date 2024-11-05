/// @file		
///	@ingroup 	4ds
///	@copyright	Copyright 2024 4DSound. All rights
///reserved.

#include "c74_min.h"
#include "vban.h"


#ifdef __APPLE__
#include <arpa/inet.h>
#include <sys/socket.h>
#elif defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

using namespace c74::min;

#define DESIRED_ADDRESS "127.0.0.1"
#define DESIRED_PORT 13251
#define IS_LOGGING 0

class vban_recvr : public object<vban_recvr>, public vector_operator<> {
    number mSampleRateFormat = 0;

    char *mVbanBuffer = nullptr;
    number mChannelCount = 2;
    number mChannelSize = 512;

    int mPacketWritePos = VBAN_HEADER_SIZE;
    number mFrameCounter = 0;
    bool mSampleRateFixed = false;
    int mConnected = 0;
    int mSockfd = -1;
    number mDataBufferSize = 0;
    struct sockaddr_in *mServerAddr = nullptr;
    VBanHeader *mPacketHeader = nullptr;
    bool mIsRecieving = false;
    symbol mIP = "";
    int mPort = DESIRED_PORT;
    symbol mStream = "";
    number mBundlesSent = 0;
    std::vector<uint8_t> mRecvBuffer;
    buffer_reference mRecvBufferReference {this};

    static double** convertToDoublePointer(const std::vector<std::vector<float>>& vec) {
        // Number of rows
        size_t numRows = vec.size();

        // Allocate memory for the array of double pointers
        auto** arr = new double*[numRows];

        for (size_t i = 0; i < numRows; ++i) {
            // Number of columns for each row
            size_t numCols = vec[i].size();

            // Allocate memory for each row's data
            arr[i] = new double[numCols];

            // Copy elements from std::vector<float> to double array
            for (size_t j = 0; j < numCols; ++j) {
                arr[i][j] = static_cast<double>(vec[i][j]);  // Cast float to double
            }
        }

        return arr;
    }

public:
    std::vector<std::unique_ptr<outlet<> > > m_outlets;

    MIN_DESCRIPTION{"recvive signal over vban"};
    MIN_TAGS{"VBAN,Network,UDP"};
    MIN_AUTHOR{"4D Sound"};

    inlet<> inlet{this, "(signal) inlet Pass thru", "signal"};


    message<> active{
        this, "active", "Start or Stop the sender",
        MIN_FUNCTION {
            if (mIsRecieving) {
                stopSock();
                mIsRecieving = false;
            } else {
                startSock();
                mIsRecieving = true;
            }
            return {};
        }
    };

    message<> port{
        this, "port", "Set the port number",
        MIN_FUNCTION {
            cout << "SETTING PORT: " << args[0] << endl;
            mPort = args[0];
            return {};
        }
    };

    message<> host{
        this, "host", "Set the IP address",
        MIN_FUNCTION {
            cout << "SETTING HOST: " << args[0] << endl;
            mIP = args[0];
            return {};
        }
    };

    message<> chan{
        this, "chan", "Set the number of channels",
        MIN_FUNCTION {
            cout << "SETTING CHANNELS: " << args[0] << endl;
            mChannelCount = args[0];
            return {};
        }
    };

    message<> stream{
        this, "stream", "Set the stream name",
        MIN_FUNCTION {
            cout << "SETTING STREAM NAME: " << args[0] << endl;
            mStream = args[0];
            return {};
        }

    };


    int startSock() {
        mSockfd = socket(AF_INET, SOCK_DGRAM, 0);
        int recv_buf_size = 30 * VBAN_DATA_MAX_SIZE;

        if (mSockfd < 0) {
            setsockopt(mSockfd,SOL_SOCKET,SO_RCVBUF, &recv_buf_size, sizeof(int));
        }
        mServerAddr = new struct sockaddr_in;
        mServerAddr->sin_family = AF_INET;
        mServerAddr->sin_addr.s_addr = INADDR_ANY;
        mServerAddr->sin_port = htons(mPort);
        if (bind(mSockfd, reinterpret_cast<struct sockaddr *>(mServerAddr), sizeof(struct sockaddr_in)) < 0) {
            cout << "bind failed" << endl;
            close(mSockfd);
        }

        mRecvBuffer.resize(recv_buf_size);

        for (int i = 0; i < VBAN_SR_MAXNUMBER; i++) {
            if (samplerate() == VBanSRList[i]) {
                mSampleRateFormat = i;
                mSampleRateFixed = true;
                cout << "SAMPLE RATE: " << mSampleRateFormat << endl;
                break;
            }
        }

        cout << "Listening for VBAN streams from : " << mPort << endl;
        mIsRecieving = true;
        return mIsRecieving;
    }

    int stopSock() {
        close(mSockfd);
        mIsRecieving = false;
        cout << "CLOSING SOCKET" << endl;
        return mIsRecieving;
    }

    // post to max window == but only when the class is loaded the first time
    message<> maxclass_setup{
        this, "maxclass_setup",
        MIN_FUNCTION {
            return {};
        }
    };

    message<> dspsetup{
        this, "dspsetup",
        MIN_FUNCTION {
            mFrameCounter = 0;
            if (!mSampleRateFixed) {
                for (int i = 0; i < VBAN_SR_MAXNUMBER; i++) {
                    if (samplerate() == VBanSRList[i]) {
                        mSampleRateFormat = i;
                        mSampleRateFixed = true;
                        cout << "SAMPLE RATE: " << mSampleRateFormat << endl;
                    }
                }
            }

            mConnected += 1;
            mPacketWritePos = VBAN_HEADER_SIZE;
            cout<< " CONNECTED SIGNALS : " << mConnected << endl;

            return {};
        }
    };

    ~vban_recvr() {
        stopSock();
        close(mSockfd);
        free(mVbanBuffer);
        free(mPacketHeader);
        delete mServerAddr;
    }

    vban_recvr(const atoms &args = {}) {
        if (mChannelCount > 254) {
            cerr << "Channel count " << mChannelCount << " not allowed, clamping to 254"
                    << endl;
            mChannelCount = 254;
        }

        for (auto i = 0; i < 8; i++) {
            auto an_outlet = std::make_unique<outlet<> >(this, "(signal) Output Signal " +
                                                              std::to_string(i + 1),"signal");
            m_outlets.push_back(std::move(an_outlet));
        }

        mChannelSize = VBAN_SAMPLES_MAX_NB * mChannelCount;
    }


    // log the number of packets sent to number of frames sent
    void logPacketCount() {
        FILE *logFile = fopen("/Users/abalaji/log.csv", "a+");
        if (logFile == NULL)
            cout << "Error opening log file" << endl;
        else {
            time_t now = time(0);
            char *dt = ctime(&now);
            fprintf(logFile, "%f,%f,%s\n", mFrameCounter, mBundlesSent, dt);
            fclose(logFile);
        }
    }

    std::vector<std::vector<float>> parsePacket(std::vector<uint8_t> *buffer, int len) {
        if (checkPacket(buffer, VBAN_HEADER_SIZE)) {
            cout << "Packet is valid" << endl;
            auto *const hdr = (struct VBanHeader *) (buffer->data());
            int const sample_rate_format = hdr->format_SR & VBAN_SR_MASK;
            if (int sample_rate = 0; getSampleRateFromVBANSampleRateFormat(sample_rate, sample_rate_format)) {
                const symbol stream_name = hdr->streamname;
                if (stream_name == mStream) {
                    int const nb_samples = hdr->format_nbs + 1;
                    int const nb_channels = hdr->format_nbc + 1;
                    size_t sample_size = VBanBitResolutionSize[static_cast<const VBanBitResolution>(
                        hdr->format_bit & VBAN_BIT_RESOLUTION_MASK)];
                    size_t payload_size = nb_samples * nb_channels * sample_size;

                    std::vector<std::vector<float> > buffers;
                    int float_buffer_size = (payload_size / sample_size) / nb_channels;
                    for (int i = 0; i < nb_channels; ++i) {
                        std::vector<float> new_buffer;
                        new_buffer.resize(float_buffer_size);
                        buffers.emplace_back(new_buffer);
                    }
                    for (size_t i = 0; i < float_buffer_size; ++i) {
                        for (int c = 0; c < nb_channels; ++c) {
                            size_t pos = (i * nb_channels * 2) + (c * 2) + VBAN_HEADER_SIZE;
                            char byte_1 = buffer->data()[pos];
                            char byte_2 = buffer->data()[pos + 1];
                            short original_value = ((static_cast<short>(byte_2)) << 8) | (0x00ff & byte_1);

                            buffers[c][i] = ((float) original_value) / (float) 32768;
                        }
                    }
                    return buffers;
                }
            }
        } else {
            cout << "Packet is invalid" << endl;
        }
        return {};

    }

    bool getSampleRateFromVBANSampleRateFormat(int &sampleRate, uint srFormat) {
        if (srFormat >= 0 && srFormat < VBAN_SR_MAXNUMBER) {
            sampleRate = VBanSRList[srFormat];
            return true;
        }

        cout << "Could not find samplerate for VBAN sample rate " << srFormat << endl;
        return false;
    }

    bool checkPacket(std::vector<uint8_t> *hdrBuffer, const int len) {
        struct VBanHeader const *const hdr = (struct VBanHeader *) (hdrBuffer->data());
        enum VBanProtocol protocol = static_cast<VBanProtocol>(VBAN_PROTOCOL_UNDEFINED_4);
        enum VBanCodec codec = static_cast<VBanCodec>(VBAN_BIT_RESOLUTION_MAX);
        if (hdrBuffer == 0) {
            cout << "Error: No header buffer" << endl;
            return false;
        }
        if (len > VBAN_HEADER_SIZE) {
            cout << "Error: Packet size too large" << endl;
            return false;
        }
        if (hdr->vban != *(int32_t *) ("VBAN")) {
            cout << "Error: Recieved header does not have the magic number" << endl;
            return false;
        }
        if (hdr->format_bit != VBAN_BITFMT_16_INT) {
            cout << "Format bit is not 16 bit , only 16bit PCM supported at this time." << endl;
        }
        if (hdr->format_nbc + 1 < 1) {
            cout << "Error: Channel count is less than 1" << endl;
            return false;
        }
        protocol = static_cast<VBanProtocol>(hdr->format_SR & VBAN_PROTOCOL_MASK);
        codec = static_cast<VBanCodec>(hdr->format_bit & VBAN_CODEC_MASK);

        if (protocol != VBAN_PROTOCOL_AUDIO) {
            switch (protocol) {
                case VBAN_PROTOCOL_SERIAL:
                case VBAN_PROTOCOL_TXT:
                case VBAN_PROTOCOL_UNDEFINED_1:
                case VBAN_PROTOCOL_UNDEFINED_2:
                case VBAN_PROTOCOL_UNDEFINED_3:
                case VBAN_PROTOCOL_UNDEFINED_4:
                    cout << "Protocol not supported yet" << endl;
                default:
                    cout << "Packet with unknown protocol" << endl;
            }

            return false;
        } else {
            if (codec != VBAN_CODEC_PCM)
                return false;

            if (!checkPcmPacket(hdrBuffer, len))
                return false;
        }
        return true;
    }

    bool checkPcmPacket(void *buffer, int size) {
        // the packet is already a valid vban packet and buffer already checked before
        struct VBanHeader const *const hdr = (struct VBanHeader *) (buffer);
        enum VBanBitResolution const bit_resolution = static_cast<const VBanBitResolution>(
            hdr->format_bit & VBAN_BIT_RESOLUTION_MASK);
        int const sample_rate_format = hdr->format_SR & VBAN_SR_MASK;

        if (bit_resolution >= VBAN_BIT_RESOLUTION_MAX) {
            cout << "Invalid bit resolution" << endl;
            return false;
        }

        if (sample_rate_format >= VBAN_SR_MAXNUMBER) {
            cout << "Invalid sample rate" << endl;
            return false;
        }

        return true;
    }

    void recvPacket(audio_bundle output) {
        auto l = recv(mSockfd,mRecvBuffer.data(),VBAN_DATA_MAX_SIZE,0);
        if(l>0) {
            // get num of channels
            // validate sample rate
            // create buffers (or resize to num of channels)
            auto recv_buffer =  parsePacket(&mRecvBuffer,VBAN_DATA_MAX_SIZE);
            auto temp_buffer = convertToDoublePointer(recv_buffer);
             
            cout<<"recived outlets : "<<output.channel_count()<<" frame count:  "<<output.frame_count()<<endl;

        }
    }

    void operator()(audio_bundle input, audio_bundle output) {
        if (!mIsRecieving) {
            return;
        }
        if (!mSockfd) {
            return;
        }
        // read from socket
        recvPacket(output);

    }
};

MIN_EXTERNAL(vban_recvr);
