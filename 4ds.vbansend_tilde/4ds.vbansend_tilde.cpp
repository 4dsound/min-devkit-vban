#include "4ds.vbansend_tilde.h"


VbanSender::VbanSender(const atoms &args)
{
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
	if (mIsRunning)
		return;

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

	// buffer size for each channel
	mPacketChannelSize = VBAN_SAMPLES_MAX_NB * 2;

	// if total buffersize exceeds max data size, resize packet channel size to fit max data size
	if (mPacketChannelSize * mChannelCount > VBAN_DATA_MAX_SIZE)
		mPacketChannelSize = (VBAN_DATA_MAX_SIZE / (mChannelCount * 2)) * 2;

	// compute the buffer size of all channels together
	mAudioBufferSize = mPacketChannelSize * mChannelCount;

	// set packet size
	mPacketSize = mAudioBufferSize + VBAN_HEADER_SIZE;

	// resize the packet data to have the correct size
	mVbanBuffer.resize(mPacketSize);

	// Reset packet counter and buffer write position
	mPacketCounter = 0;
	mPacketWritePos = VBAN_HEADER_SIZE;

	// Determine samplerate
	for (int i = 0; i < VBAN_SR_MAXNUMBER; i++)
		if (samplerate() == VBanSRList[i])
			mSampleRateFormat = i;

	// initialize VBAN header
	mPacketHeader = (struct VBanHeader*)(&mVbanBuffer[0]);
	mPacketHeader->vban       = *(int32_t*)("VBAN");
	mPacketHeader->format_nbc = mChannelCount - 1;
	mPacketHeader->format_SR  = mSampleRateFormat;
	mPacketHeader->format_bit = VBAN_BITFMT_16_INT;
	strncpy(mPacketHeader->streamname, mStreamName.c_str(), VBAN_STREAM_NAME_SIZE - 1);
	mPacketHeader->nuFrame    = mPacketCounter;
	mPacketHeader->format_nbs = (mPacketChannelSize / 2) - 1;

	if (mChannelCount > 254)
	{
		cerr << "Channel count " << mChannelCount << " not allowed, clamping to 254" << endl;
		mChannelCount = 254;
	}

	// Log
	cout << "Sample rate: " << VBanSRList[int(mSampleRateFormat)] << endl;
	cout << "Starting stream: " << mStreamName << " to IP: " << mIP << " port: " << ntohs(mServerAddr.sin_port) << endl;

	mIsRunning = true;
}


void VbanSender::stop()
{
	if (mIsRunning)
	{
		close(mSockfd);
		mIsRunning = false;
		cout << "Stopping socket" << endl;
	}
}


void VbanSender::setupDSP()
{
	if (mIsRunning)
	{
		stop();
		start();
	}
}


void VbanSender::sendPacket()
{
	mPacketHeader->nuFrame = mPacketCounter;
	assert(mPacketWritePos <= VBAN_DATA_MAX_SIZE);
	// Send the message according to the number of frames written
	// wait for destination socket to be ready
	ssize_t sent_len = sendto(mSockfd, mVbanBuffer.data(), mPacketWritePos, 0, (struct sockaddr *)&mServerAddr, sizeof(sockaddr));
	if (sent_len < 0)
	{
		cout << "Error sending message" << endl;
		cout << "Error: " << strerror(errno) << endl;
	}

	mPacketWritePos = VBAN_HEADER_SIZE;
	mPacketCounter++;
}


void VbanSender::operator()(audio_bundle input, audio_bundle output)
{
	if (!mIsRunning)
		return;

	int packet_size = mAudioBufferSize + VBAN_HEADER_SIZE;
	for (auto i = 0; i < input.frame_count(); ++i)
	{
		for (auto channel = 0; channel < mChannelCount; ++channel)
		{
			float sample = input.samples(channel)[i];
			short value = static_cast<short>(sample * 32768.0f);

			// convert short to two bytes
			char byte_1 = value;
			char byte_2 = value >> 8;
			mVbanBuffer[mPacketWritePos] = byte_1;
			mVbanBuffer[mPacketWritePos + 1] = byte_2;

			mPacketWritePos += 2;
		}
	}
	if (mPacketWritePos >= packet_size)
		sendPacket();
}

MIN_EXTERNAL(VbanSender);
