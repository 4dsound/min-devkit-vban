#pragma once

#include "c74_min.h"
#include "vban.h"

//#include <asio/ts/buffer.hpp>
//#include <asio/ts/internet.hpp>
//#include <asio/io_service.hpp>
//#include <asio/system_error.hpp>

#ifdef __APPLE__
	#include <arpa/inet.h>
	#include <sys/socket.h>
#elif defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

#define DESIRED_ADDRESS "127.0.0.1"
#define DESIRED_PORT 13251
#define VERSION "0.05"
#define IS_LOGGING 0

using namespace c74::min;


class VbanSender : public object<VbanSender>, public vector_operator<>
{
public:
	~VbanSender();
	VbanSender(const atoms &args = {});

	/**
	 * Processed the audio input to audio output
	 * @param input Multichannel input buffer
	 * @param output Multichannel output buffer
	 */
	void operator()(audio_bundle input, audio_bundle output);

	MIN_DESCRIPTION { "Send audio stream over vban." };
	MIN_TAGS { "VBAN, Network, UDP" };
	MIN_AUTHOR { "4DSound" };

	outlet<> output { this, "(signal) Output Pass thru", "signal" };

	message<> active { this, "active", "Start or stop the sender",
		MIN_FUNCTION{
			if (args[0] == 1) {
				start();
			}
			else if (args[0] == 0) {
				stop();
			}
			return {};
		}
	};

	message<> port { this, "port", "Set the port number",
		MIN_FUNCTION{
			cout << "Setting port: " << args[0] << endl;
			mPort = args[0];
			setupDSP();
			return {};
		}
	};

	message<> host { this, "host", "Set the IP address",
		MIN_FUNCTION{
			cout << "Setting host: " << args[0] << endl;
			mIP = args[0];
			setupDSP();
			return {};
		}
	};

	message<> chan { this, "channels", "Set the number of channels",
		MIN_FUNCTION{
			cout << "Setting number of channels: " << args[0] << endl;
			mChannelCount = args[0];
			setupDSP();
			return {};
		}
	};

	message<> stream { this, "stream", "Set the stream name",
		MIN_FUNCTION{
			cout << "Setting stream name: "<<args[0] <<endl;
			mStreamName = args[0];
			setupDSP();
			return {};
		}

	};

	// Post to max window, but only when the class is loaded the first time
	message<> maxclass_setup{this, "maxclass_setup",
		MIN_FUNCTION{
			cout << "4ds.vbansend~ " << VERSION << endl;
			return {};
		}
	};

	message<> dspsetup {this, "dspsetup",
		MIN_FUNCTION{
			setupDSP();
			return {};
		}
	};

private:
	void start();
	void stop();
	void setupDSP();
    void sendPacket();

private:
	number mSampleRateFormat = 0; // Index to VBanSRList, sample rates supported by VBAN
	vector<char> mVbanBuffer = { }; // Data containing the full VBAN packet including the header
	int mChannelCount = 2; // Number of channels of audio being sent
	int mPacketChannelSize = 0; // Size in bytes of one channel of audio in the VBAN packet

	int mPacketWritePos = VBAN_HEADER_SIZE; // Write position in the mVbanBuffer of incoming audio data.
	int mPacketCounter = 0; // Number of packets sent
	int mSockfd = -1;
	int mAudioBufferSize = 0;
	int mPacketSize = 0;
	struct sockaddr_in mServerAddr;
	VBanHeader *mPacketHeader = nullptr;
	bool mIsRunning = false;
	symbol mIP = DESIRED_ADDRESS;
	int mPort = DESIRED_PORT;
	symbol mStreamName = "vbandemo0";
	std::vector<std::unique_ptr<inlet<>>> mInlets;

	// ASIO
//	asio::io_context 			mIOContext;
//	asio::ip::udp::endpoint 	mRemoteEndpoint;
//	asio::ip::udp::socket       mSocket{ mIOContext };
};


