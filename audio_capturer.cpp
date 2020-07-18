#include "audio_capturer.h"
#include "GetAudio.h"

namespace audio{
	std::unique_ptr<capturer> get_capturer() {
		return std::make_unique< AudioCapture::AudioCaptureRaw>();
	}

	grt::mic_list get_mic_list() {
		return AudioCapture::get_mic_list();
	}
}//namespace audio