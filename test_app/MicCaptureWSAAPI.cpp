
#include <iostream>

#include "audio_capturer.h"
#include <cassert>
#include <chrono>
#include <thread>


class AudioCaptureTest:public audio::callback {
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
		std::cout << "#";
		//std::cout << "bits per sample " << bits_per_sample << " sample rate = " << sample_rate << " channel = " << number_of_channels << '\n';
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
	auto capturer_ = audio::get_capturer();
	AudioCaptureTest audioWriter{};
	std::cout << "audio capture started\n";
	capturer_->start(&audioWriter);
	
	std::this_thread::sleep_for(std::chrono::seconds(15));

	
	
	capturer_->stop();
	std::cout << "\n stoped capture\n";
	std::cout << capturer_->logs() << '\n';

	
	return 0;

}