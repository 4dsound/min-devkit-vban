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

class vban_sender : public object<vban_sender>, public vector_operator<> {

	number mSampleRateFormat = 0;

	vector <char> mVbanBuffer = {};
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
	symbol mIP = DESIRED_ADDRESS;
	int mPort = DESIRED_PORT;
	symbol mStream = "vbandemo0";
	number mBundlesSent = 0;
	std::vector<std::unique_ptr<inlet<>>> m_inlets;
	public:
		MIN_DESCRIPTION{"send signal over vban"};
		MIN_TAGS{"VBAN,Network,UDP"};
		MIN_AUTHOR{"4D Sound"};

		outlet<> output{this, "(signal) Output Pass thru", "signal"};

		message<> active {this, "active", "Start or Stop the sender",
				MIN_FUNCTION{
					if(args[0] == 1) {
						startSock();
						mIsRecieving = true;
					}
					else if(args[0]==0) {
						mIsRecieving= false;
						stopSock();
					}
				return {};
			}
		};

		message<> port{this, "port", "Set the port number",
			MIN_FUNCTION{
				cout << "SETTING PORT: " << args[0] << endl;
				mPort = args[0];
				return {};
			}
		};

		message<> host{this, "host", "Set the IP address",
			MIN_FUNCTION{
				cout << "SETTING HOST: " << args[0] << endl;
				mIP = args[0];
				return {};
			}
		};

		message<> chan{this, "chan", "Set the number of channels",
			MIN_FUNCTION{
				cout << "SETTING CHANNELS: " << args[0] << endl;
				mChannelCount = args[0];
				return {};
			}
		};

		message<> stream{this,"stream","Set the stream name",
			MIN_FUNCTION{
				cout << "SETTING STREAM NAME: "<<args[0] <<endl;
				mStream = args[0];
				return {};
			}
			
		};



		int startSock() {
			mSockfd = socket(AF_INET, SOCK_DGRAM, 0);
			mServerAddr->sin_family = AF_INET;
			mServerAddr->sin_port = htons(mPort);
			mServerAddr->sin_addr.s_addr = inet_addr(mIP);
			setsockopt(mSockfd, SOL_SOCKET, SO_BROADCAST, (char *)&mServerAddr, sizeof(mServerAddr));
			if (inet_pton(AF_INET, mIP, &mServerAddr->sin_addr) <= 0) {
				cout << "Invalid address" << endl;
				close(mSockfd);
			}
			for (int i = 0; i < VBAN_SR_MAXNUMBER; i++) {
				if (samplerate() == VBanSRList[i]) {
				mSampleRateFormat = i;
				mSampleRateFixed = true;
				cout << "SAMPLE RATE: " << mSampleRateFormat << endl;
					break;
				}
			}

			cout << "STARTING STEAM: " << mStream << " TO IP: " << mIP
					<< " PORT: " << ntohs(mServerAddr->sin_port) << endl;

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
		message<> maxclass_setup{this, "maxclass_setup", 
			MIN_FUNCTION{
				return {};
			}
		};

		message<> dspsetup{this, "dspsetup", 
			MIN_FUNCTION{
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

				return {};
			}
	};

	~vban_sender() {
		stopSock();
		close(mSockfd);
		// free(mVbanBuffer);
		free(mPacketHeader);
		delete mServerAddr;
	}

	vban_sender(const atoms &args = {}) {


        if (mChannelCount > 254) {
            cerr << "Channel count " << mChannelCount << " not allowed, clamping to 254"
                 << endl;
            mChannelCount = 254;
        }

        for (auto i = 0; i < 8; i++) {
			auto an_inlet = std::make_unique<inlet<>>(this, "(signal) Input signal " +
																std::to_string(i+1));
			m_inlets.push_back(std::move(an_inlet));
		}

		mChannelSize = VBAN_SAMPLES_MAX_NB * mChannelCount;

		while (mChannelSize * mChannelCount > VBAN_DATA_MAX_SIZE) {
			// Calculate the new size in integer form to avoid precision issues
			int newSize = static_cast<int>(VBAN_DATA_MAX_SIZE / mChannelCount);
			newSize = newSize & ~1; // Ensure newSize is even

			// Assign the newSize back to mChannelSize as a double
			mChannelSize = static_cast<double>(newSize);
		}

		mDataBufferSize = mChannelSize * mChannelCount;
		mVbanBuffer.resize(VBAN_HEADER_SIZE +  mDataBufferSize);

		// mVbanBuffer = (char *)malloc(VBAN_HEADER_SIZE + mDataBufferSize);
		mPacketHeader = (VBanHeader *)malloc(sizeof(VBanHeader));
		mServerAddr = (sockaddr_in *)malloc(sizeof(sockaddr_in));
		mPacketWritePos = VBAN_HEADER_SIZE;
	}

    void sendPacket(long framecount) {
        if (mSockfd < 0) {
			cout << "Error creating socket" << endl;
		}

		mPacketHeader->vban = *(int32_t *)("VBAN");
		mPacketHeader->format_nbc = mChannelCount - 1;
		mPacketHeader->format_SR = mSampleRateFormat;
		mPacketHeader->format_bit = VBAN_BITFMT_16_INT;
		strncpy(mPacketHeader->streamname, mStream, VBAN_STREAM_NAME_SIZE - 1);
		mPacketHeader->nuFrame = mFrameCounter;
		mPacketHeader->format_nbs = (mChannelSize / 2) - 1;
		mVbanBuffer.insert(mVbanBuffer.begin(), (char *)mPacketHeader, (char *)mPacketHeader + sizeof(VBanHeader));
		// memcpy(mVbanBuffer, mPacketHeader, sizeof(VBanHeader));
		assert(mPacketWritePos <= VBAN_DATA_MAX_SIZE);
		// Send the message according to the number of frames written
		// wait for destination socket to be ready
		// if(mDestAddr->)
		ssize_t sent_len = sendto(mSockfd, mVbanBuffer.data(), mPacketWritePos, 0,
									(struct sockaddr *)mServerAddr, sizeof(sockaddr));
		if (sent_len < 0) {
			cout << "Error sending message" << endl;
			cout << "Error: " << strerror(errno) << endl;
}

		mPacketWritePos = VBAN_HEADER_SIZE;
		mVbanBuffer.clear();
		mFrameCounter += 1;
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

	void operator()(audio_bundle input, audio_bundle output) {
		if (!mIsRecieving) {
			return;
		}

		number packet_size = mDataBufferSize + VBAN_HEADER_SIZE;

		long previous_frame = 0;

		for (auto i = 0; i < input.frame_count(); ++i) {
			for (auto channel = 0; channel < mChannelCount; ++channel) {

			float sample = input.samples(channel)[i];

			short value = static_cast<short>(sample * 32768.0f);

			// convert short to two bytes
			char byte_1 = value;
			char byte_2 = value >> 8;
			mVbanBuffer.push_back(byte_1);
			mVbanBuffer.push_back(byte_2);

			mPacketWritePos += 2;
			}
		}
		if (mPacketWritePos >= packet_size) {
			sendPacket(input.frame_count());
		}
		mBundlesSent += 1;
		if(IS_LOGGING)
			logPacketCount();
	}
};

MIN_EXTERNAL(vban_sender);
