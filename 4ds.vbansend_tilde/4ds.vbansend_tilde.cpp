#include "4ds.vbansend_tilde.h"


VbanSender::VbanSender(const atoms &args) : mEncoder(*this)
{
	// Create inlets
	for (auto i = 0; i < 8; i++)
	{
		auto an_inlet = std::make_unique<inlet<>>(this, "(signal) Input signal " + std::to_string(i + 1));
		mInlets.push_back(std::move(an_inlet));
	}
}


VbanSender::~VbanSender()
{
	stop();
}


void VbanSender::start()
{
	// Open the socket
	mSockfd = socket(AF_INET, SOCK_DGRAM, 0);
	mServerAddr.sin_family = AF_INET;
	mServerAddr.sin_port = htons(mPort);
	mServerAddr.sin_addr.s_addr = inet_addr(mIP);
	setsockopt(mSockfd, SOL_SOCKET, SO_BROADCAST, (char *) &mServerAddr, sizeof(mServerAddr));
	if (inet_pton(AF_INET, mIP, &mServerAddr.sin_addr) <= 0)
	{
		cout << "Invalid address" << endl;
		close(mSockfd);
		return;
	}

	// Determine samplerate
	int sampleRateFormat = -1;
	for (int i = 0; i < VBAN_SR_MAXNUMBER; i++)
		if (samplerate() == VBanSRList[i])
			sampleRateFormat = i;
	if (sampleRateFormat == -1)
	{
		cout << "Invalid samplerate" << endl;
		close(mSockfd);
		return;
	}
	mEncoder.setSampleRateFormat(sampleRateFormat);

	// Log
	if (!mEncoder.isRunning())
	{
		cout << "Starting stream: " << mEncoder.getStreamName() << " to IP: " << mIP << " port: " << ntohs(mServerAddr.sin_port) << endl;
		mEncoder.start();
	}
}


void VbanSender::stop()
{
	if (mEncoder.isRunning())
	{
		cout << "Stopping socket" << endl;
		close(mSockfd);
		mEncoder.stop();
	}
}


void VbanSender::setupDSP()
{
	if (mEncoder.isRunning())
	{
		stop();
		start();
	}
}


void VbanSender::sendPacket(char* data, int size)
{
	// Send the message according to the number of frames written
	// wait for destination socket to be ready
	ssize_t sent_len = sendto(mSockfd, data, size, 0, (struct sockaddr *)&mServerAddr, sizeof(sockaddr));
	if (sent_len < 0)
	{
		cout << "Error sending message" << endl;
		cout << "Error: " << strerror(errno) << endl;
	}
}


void VbanSender::operator()(audio_bundle input, audio_bundle output)
{
	mEncoder.process(input.samples(), input.channel_count(), input.frame_count());
}

MIN_EXTERNAL(VbanSender);
