#pragma once

#include "c74_min.h"

#include <vban/vban.h>
#include <vban/vbanstreamencoder.h>

#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <asio/io_service.hpp>
#include <asio/system_error.hpp>

#define VERSION "0.06"

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
			if (args[0] == 1)
				mEncoder.setActive(true);
			else if (args[0] == 0)
				mEncoder.setActive(false);
			return {};
		}
	};

	message<> port { this, "port", "Set the port number",
		MIN_FUNCTION{
			std::lock_guard<std::mutex> lock(mSocketSettingsMutex);
			mPort = args[0];
			mSocketSettingsDirty.set();
			cout << "Setting port: " << args[0] << endl;
			return {};
		}
	};

	message<> host { this, "host", "Set the IP address",
		MIN_FUNCTION{
			std::lock_guard<std::mutex> lock(mSocketSettingsMutex);
			mIP = args[0];
			mSocketSettingsDirty.set();
			cout << "Setting host: " << args[0] << endl;
			return {};
		}
	};

	message<> chan { this, "channels", "Set the number of channels",
		MIN_FUNCTION{
			int channelCount = args[0];
			if (channelCount > VBAN_CHANNELS_MAX_NB)
			{
				cerr << "Channel count " << channelCount << " not allowed, clamping to maximum." << endl;
				channelCount = VBAN_CHANNELS_MAX_NB;
			}
			cout << "Setting number of channels: " << channelCount << endl;
			mEncoder.setChannelCount(channelCount);
			return {};
		}
	};

	message<> stream { this, "stream", "Set the stream name",
		MIN_FUNCTION{
			cout << "Setting stream name: "<<args[0] <<endl;
			mEncoder.setStreamName(args[0]);
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

public:
	void sendPacket(const std::vector<char>& data);

private:
	void startSocket();
	void stopSocket();
	void setupDSP();

private:
	std::vector<std::unique_ptr<inlet<>>> mInlets;
	vban::VBANStreamEncoder<VbanSender> mEncoder;

	// Socket settings
	symbol mIP = "127.0.0.1";
	int mPort = 13251;
	std::mutex mSocketSettingsMutex;
	vban::DirtyFlag mSocketSettingsDirty;

	// ASIO Socket
	asio::io_context 			mIOContext;
	asio::ip::udp::endpoint 	mRemoteEndpoint;
	asio::ip::udp::socket       mSocket{ mIOContext };
};


