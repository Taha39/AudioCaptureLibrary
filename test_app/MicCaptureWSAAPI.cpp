
#include <iostream>

#include "GetAudio.h"
#include <cassert>
#include <chrono>


class AudioCaptureTest:public AudioCapture::callback {
public:
	AudioCaptureTest() {
		fopen_s(&ftest, "CAudio.pcm", "wb");
		assert(ftest);
	};
	void onData(const uint8_t* data, int size,
		int bits_per_sample,
		int sample_rate,
		size_t number_of_channels,
		size_t number_of_frames) override {
		std::cout << "bits per sample " << bits_per_sample << " sample rate = " << sample_rate << " channel = " << number_of_channels << '\n';
		fwrite(data, 1, size, ftest);
	}
	
	~AudioCaptureTest() {
		assert(ftest);
		fclose(ftest);
	}
public:
	FILE *ftest = nullptr;
};

int main()
{
	AudioCapture::AudioCaptureRaw audioSource{};
	AudioCaptureTest audioWriter{};
	audioSource.startCapture(&audioWriter);
	
	int close = 0;
	std::this_thread::sleep_for(std::chrono::seconds(15));
	
	audioSource.stopCapture();
	
	
	return 0;

}