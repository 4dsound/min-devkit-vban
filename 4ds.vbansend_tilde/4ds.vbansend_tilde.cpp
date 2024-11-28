#include "4ds.vbansend_tilde.h"

#include <asio/ip/udp.hpp>
#include <asio/ip/address.hpp>


VbanSender::VbanSender(const atoms &args) : mEncoder(*this)
{
	// Create inlets
	for (auto i = 0; i < 8; i++)
	{
		auto an_inlet = std::make_unique<inlet<>>(this, "(signal) Input signal " + std::to_string(i + 1));
		mInlets.push_back(std::move(an_inlet));
	}

	// Open the socket at default host and port
	startSocket();
}


VbanSender::~VbanSender()
{
	stopSocket();
}


void VbanSender::startSocket()
{
	std::lock_guard<std::mutex> lock(mSocketSettingsMutex);

	// Try open socket
	asio::error_code asio_error_code;
	mSocket.open(asio::ip::udp::v4(), asio_error_code);
	if (asio_error_code)
	{
		cout << asio_error_code.message() << endl;
		return;
	}

	// Disable broadcast
	mSocket.set_option(asio::socket_base::broadcast(false), asio_error_code);
	if (asio_error_code)
	{
		cout << asio_error_code.message() << endl;
		return;
	}

	// resolve ip address from endpoint
	asio::ip::tcp::resolver resolver(mIOContext);
	asio::ip::tcp::resolver::query query(mIP, "80");
	asio::ip::tcp::resolver::iterator iter = resolver.resolve(query, asio_error_code);
	asio::ip::tcp::endpoint endpoint = iter->endpoint();
	auto address = asio::ip::address::from_string(endpoint.address().to_string(), asio_error_code);
	if (asio_error_code)
	{
		cout << asio_error_code.message() << endl;
		return;
	}

	mRemoteEndpoint = asio::ip::udp::endpoint(address, mPort);

	// Log
	cout << "Starting socket: IP: " << mIP << " port: " << mPort << endl;
}


void VbanSender::stopSocket()
{
	std::lock_guard<std::mutex> lock(mSocketSettingsMutex);
	asio::error_code asio_error_code;
	mSocket.close();
	if (asio_error_code)
	{
		cout << asio_error_code.message() << endl;
		return;
	}
	cout << "Stopping socket" << endl;
}


void VbanSender::setupDSP()
{
	// Determine samplerate
	int sampleRateFormat = -1;
	for (int i = 0; i < VBAN_SR_MAXNUMBER; i++)
		if (samplerate() == VBanSRList[i])
			sampleRateFormat = i;
	if (sampleRateFormat == -1)
	{
		cout << "Invalid samplerate" << endl;
		return;
	}
	cout << "Setting samplerate: " << samplerate() << endl;
	mEncoder.setSampleRateFormat(sampleRateFormat);
}


void VbanSender::sendPacket(const std::vector<char>& data)
{
	// Send the message according to the number of frames written
	// wait for destination socket to be ready
	asio::error_code asio_error_code;
	mSocket.send_to(asio::buffer(&data.data()[0], data.size()), mRemoteEndpoint, 0, asio_error_code);
	if (asio_error_code)
	{
		cout << "Error sending message" << endl;
		cout << "Error: " << asio_error_code.message() << endl;
	}
}


void VbanSender::operator()(audio_bundle input, audio_bundle output)
{
	// Update the socket settings
	if (mSocketSettingsDirty.check())
	{
		stopSocket();
		startSocket();
	}

	mEncoder.process(input.samples(), input.channel_count(), input.frame_count());
}

MIN_EXTERNAL(VbanSender);
