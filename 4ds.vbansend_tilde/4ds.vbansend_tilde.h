#pragma once

#include "c74_min.h"
#include "vban.h"
#ifdef __APPLE__
	#include <arpa/inet.h>
	#include <sys/socket.h>
#elif defined(_WIN32)
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

#define DESIRED_ADDRESS "127.0.0.1"
#define DESIRED_PORT 13251
#define IS_LOGGING 0

using namespace c74::min;


class VbanSender : public object<VbanSender>, public vector_operator<>
{
public:
	~VbanSender();
	VbanSender(const atoms &args = {});
	void operator()(audio_bundle input, audio_bundle output);

	MIN_DESCRIPTION { "Send audio signal over vban" };
	MIN_TAGS { "VBAN,Network,UDP" };
	MIN_AUTHOR { "4DSound" };

	outlet<> output { this, "(signal) Output Pass thru", "signal" };

	message<> active { this, "active", "Start or Stop the sender",
		MIN_FUNCTION{
			if (args[0] == 1) {
				startSocket();
			}
			else if (args[0] == 0) {
				stopSocket();
			}
			return {};
		}
	};

	message<> port { this, "port", "Set the port number",
		MIN_FUNCTION{
			cout << "SETTING PORT: " << args[0] << endl;
			mPort = args[0];
			return {};
		}
	};

	message<> host { this, "host", "Set the IP address",
		MIN_FUNCTION{
			cout << "SETTING HOST: " << args[0] << endl;
			mIP = args[0];
			return {};
		}
	};

	message<> chan { this, "chan", "Set the number of channels",
		MIN_FUNCTION{
			cout << "SETTING CHANNELS: " << args[0] << endl;
			mChannelCount = args[0];
			return {};
		}
	};

	message<> stream { this, "stream", "Set the stream name",
		MIN_FUNCTION{
			cout << "SETTING STREAM NAME: "<<args[0] <<endl;
			mStream = args[0];
			return {};
		}

	};

	// post to max window == but only when the class is loaded the first time
	message<> maxclass_setup{this, "maxclass_setup",
		MIN_FUNCTION{
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
	int  startSocket();
	int  stopSocket();
	void setupDSP();
    void sendPacket(long framecount);

private:
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
	std::vector<std::unique_ptr<inlet<>>> mInlets;
};

MIN_EXTERNAL(VbanSender);


