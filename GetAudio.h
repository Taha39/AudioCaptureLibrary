#ifndef _RAW_AUDIO_CAPTURERE_H__
#define _RAW_AUDIO_CAPTURERE_H__
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <thread>
#include "audio_capturer.h"

namespace AudioCapture {

	using callback = audio::callback;
	class AudioCaptureRaw : public audio::capturer
	{
	public:
		~AudioCaptureRaw() override;
		void stop() override;
		bool start(callback* cb) override;
		std::string logs() override;

	private:
		std::thread t_;
		IAudioClient *pAudioClient_{ nullptr };
		IAudioCaptureClient *pAudioCaptureClient_{ nullptr };

		UINT32 nBlockAlign_{ 0 };
		UINT8 rawbuffer[4000];
		bool onceAudioInit{ false };
		bool continueCapture{ false };
		REFERENCE_TIME hnsActualDuration{ 0 };

		int bitsPerSample{ 0 };
		int sampleRate{ 0 };
		size_t numberOfChannels{ 0 };

	private:
		bool setConfiguration();
		HRESULT getDefaultDevice(IMMDevice **ppMMDevice);
		void readPacket(UINT8 *buf, callback* ob);
		void startThread(callback* ob);
	};
}	//AudioCapture

#endif//_RAW_AUDIO_CAPTURERE_H__