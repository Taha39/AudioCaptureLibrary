
#include <iostream>
#include <mmdeviceapi.h>
#include <Audioclient.h>

namespace AudioCapture {
	class callback {
	public:
		virtual void capturedData(const uint8_t* data, int size) = 0;
		virtual ~callback() {};
	};


	class AudioCaptureRaw
	{
	public:
		AudioCaptureRaw() = default;
		bool startThread(callback* ob);
		void InitExit();
		void stopThread(bool value);
	private:

		IAudioClient *pAudioClient_{ nullptr };
		IAudioCaptureClient *pAudioCaptureClient_{ nullptr };

		UINT32 nBlockAlign_{ 0 };
		UINT8 rawbuffer[4000];
		bool onceAudioInit{ false };
		bool continueCapture{ false };
		REFERENCE_TIME hnsActualDuration{ 0 };


	private:
		bool setConfiguration();
		HRESULT getDefaultDevice(IMMDevice **ppMMDevice);
		void readPacket(UINT8 *buf, callback* ob);
	};
}	//AudioCapture