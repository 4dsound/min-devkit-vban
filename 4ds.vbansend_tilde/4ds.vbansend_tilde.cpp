#include "4ds.vbansend_tilde.h"


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

	// Open the socket
	mSocketDescriptor = socket(AF_INET, SOCK_DGRAM, 0);
	mServerAddress.sin_family = AF_INET;
	mServerAddress.sin_port = htons(mPort);
	mServerAddress.sin_addr.s_addr = inet_addr(mIP);
	setsockopt(mSocketDescriptor, SOL_SOCKET, SO_BROADCAST, (char *) &mServerAddress, sizeof(mServerAddress));
	if (inet_pton(AF_INET, mIP, &mServerAddress.sin_addr) <= 0)
	{
		cout << "Invalid address" << endl;
		close(mSocketDescriptor);
		return;
	}

	// Log
	cout << "Starting socket: IP: " << mIP << " port: " << ntohs(mServerAddress.sin_port) << endl;
}


void VbanSender::stopSocket()
{
	std::lock_guard<std::mutex> lock(mSocketSettingsMutex);
	close(mSocketDescriptor);
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
	ssize_t sent_len = sendto(mSocketDescriptor, data.data(), data.size(), 0, (struct sockaddr *)&mServerAddress, sizeof(sockaddr));
	if (sent_len < 0)
	{
		cout << "Error sending message" << endl;
		cout << "Error: " << strerror(errno) << endl;
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
