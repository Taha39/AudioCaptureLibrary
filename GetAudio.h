
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <thread>

namespace AudioCapture {
	class callback {
	public:
		virtual void onData(const uint8_t* data, int size,
			int bits_per_sample,
			int sample_rate,
			size_t number_of_channels,
			size_t number_of_frames) = 0;
		virtual ~callback() {};
	};


	class AudioCaptureRaw
	{
	public:
		//AudioCaptureRaw() = default;
		
		~AudioCaptureRaw();
		
		void stopCapture();
		bool startCapture(callback* cb);
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