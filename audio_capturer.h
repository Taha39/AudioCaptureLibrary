#ifndef __AUDIO_CAPTURER_INTERFACE__
#define __AUDIO_CAPTURER_INTERFACE__
#include <string>
#include "../common/common.h"

namespace audio {

	class callback {
	public:
		virtual void onData(const uint8_t* data, int size,
			int bits_per_sample,
			int sample_rate,
			size_t number_of_channels,
			size_t number_of_frames) = 0;
		virtual ~callback() {};
	};

	class capturer {
	public:
		virtual ~capturer() {}
		virtual void stop() = 0;
		virtual bool start(callback* cb, grt::device_info device) = 0;
		virtual std::string logs() = 0;
	};

	std::unique_ptr<capturer> get_capturer();

	grt::mic_list get_mic_list();
	

}//namespace grt

#endif//__AUDIO_CAPTURER_INTERFACE__