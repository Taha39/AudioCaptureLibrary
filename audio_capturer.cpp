#include "audio_capturer.h"
#include "GetAudio.h"

namespace audio{
	std::unique_ptr<capturer> get_capturer() {
		return std::make_unique< AudioCapture::AudioCaptureRaw>();
	}
}//namespace audio