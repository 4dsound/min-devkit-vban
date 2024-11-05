#include "4ds.vbansend_tilde.h"


VbanSender::VbanSender(const atoms& args) {
	if (mChannelCount > 254) {
		cerr << "Channel count " << mChannelCount << " not allowed, clamping to 254" << endl;
		mChannelCount = 254;
	}

	for (auto i = 0; i < 8; i++) {
		auto an_inlet = std::make_unique<inlet<>>(this, "(signal) Input signal " + std::to_string(i + 1));
		mInlets.push_back(std::move(an_inlet));
	}

	mChannelSize = VBAN_SAMPLES_MAX_NB * mChannelCount;

	while (mChannelSize * mChannelCount > VBAN_DATA_MAX_SIZE) {
		// Calculate the new size in integer form to avoid precision issues
		int newSize = static_cast<int>(VBAN_DATA_MAX_SIZE / mChannelCount);
		newSize     = newSize & ~1;    // Ensure newSize is even

		// Assign the newSize back to mChannelSize as a double
		mChannelSize = static_cast<double>(newSize);
	}

	mDataBufferSize = mChannelSize * mChannelCount;
	mVbanBuffer.resize(VBAN_HEADER_SIZE + mDataBufferSize);

	// mVbanBuffer = (char *)malloc(VBAN_HEADER_SIZE + mDataBufferSize);
	mPacketHeader   = (VBanHeader*)malloc(sizeof(VBanHeader));
	mServerAddr     = (sockaddr_in*)malloc(sizeof(sockaddr_in));
	mPacketWritePos = VBAN_HEADER_SIZE;
}


VbanSender::~VbanSender() {
	stopSocket();
	close(mSockfd);
	// free(mVbanBuffer);
	free(mPacketHeader);
	delete mServerAddr;
}


int VbanSender::startSocket() {
	mSockfd                      = socket(AF_INET, SOCK_DGRAM, 0);
	mServerAddr->sin_family      = AF_INET;
	mServerAddr->sin_port        = htons(mPort);
	mServerAddr->sin_addr.s_addr = inet_addr(mIP);
	setsockopt(mSockfd, SOL_SOCKET, SO_BROADCAST, (char*)&mServerAddr, sizeof(mServerAddr));
	if (inet_pton(AF_INET, mIP, &mServerAddr->sin_addr) <= 0) {
		cout << "Invalid address" << endl;
		close(mSockfd);
	}
	for (int i = 0; i < VBAN_SR_MAXNUMBER; i++) {
		if (samplerate() == VBanSRList[i]) {
			mSampleRateFormat = i;
			mSampleRateFixed  = true;
			cout << "SAMPLE RATE: " << mSampleRateFormat << endl;
			break;
		}
	}

	cout << "STARTING STEAM: " << mStream << " TO IP: " << mIP << " PORT: " << ntohs(mServerAddr->sin_port) << endl;

	mIsRecieving = true;
	return mIsRecieving;
}


int VbanSender::stopSocket() {
	close(mSockfd);
	mIsRecieving = false;
	cout << "CLOSING SOCKET" << endl;
	return mIsRecieving;
}


void VbanSender::setupDSP() {
	mFrameCounter = 0;
	if (!mSampleRateFixed) {
		for (int i = 0; i < VBAN_SR_MAXNUMBER; i++) {
			if (samplerate() == VBanSRList[i]) {
				mSampleRateFormat = i;
				mSampleRateFixed  = true;
				cout << "SAMPLE RATE: " << mSampleRateFormat << endl;
			}
		}
	}
	mConnected += 1;
	mPacketWritePos = VBAN_HEADER_SIZE;
}


void VbanSender::sendPacket(long framecount) {
	if (mSockfd < 0) {
		cout << "Error creating socket" << endl;
	}

	mPacketHeader->vban       = *(int32_t*)("VBAN");
	mPacketHeader->format_nbc = mChannelCount - 1;
	mPacketHeader->format_SR  = mSampleRateFormat;
	mPacketHeader->format_bit = VBAN_BITFMT_16_INT;
	strncpy(mPacketHeader->streamname, mStream, VBAN_STREAM_NAME_SIZE - 1);
	mPacketHeader->nuFrame    = mFrameCounter;
	mPacketHeader->format_nbs = (mChannelSize / 2) - 1;
	mVbanBuffer.insert(mVbanBuffer.begin(), (char*)mPacketHeader, (char*)mPacketHeader + sizeof(VBanHeader));
	// memcpy(mVbanBuffer, mPacketHeader, sizeof(VBanHeader));
	assert(mPacketWritePos <= VBAN_DATA_MAX_SIZE);
	// Send the message according to the number of frames written
	// wait for destination socket to be ready
	// if(mDestAddr->)
	ssize_t sent_len = sendto(mSockfd, mVbanBuffer.data(), mPacketWritePos, 0, (struct sockaddr*)mServerAddr, sizeof(sockaddr));
	if (sent_len < 0) {
		cout << "Error sending message" << endl;
		cout << "Error: " << strerror(errno) << endl;
	}

	mPacketWritePos = VBAN_HEADER_SIZE;
	mVbanBuffer.clear();
	mFrameCounter += 1;
}


void VbanSender::operator()(audio_bundle input, audio_bundle output) {
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
}
