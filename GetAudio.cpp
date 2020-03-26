
#include <thread>
#include "GetAudio.h"
#include <cassert>
#include <iostream>
#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { return false; }

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const IID IID_IAudioClient = __uuidof(IAudioClient);

int bits_per_sample = 0;//?
int sample_rate = 0;//?
size_t	number_of_channels = 0;//?

namespace AudioCapture {

	bool AudioCaptureRaw::setConfiguration()
	{
		IMMDevice *m_pMMDevice;

		HRESULT hr = getDefaultDevice(&m_pMMDevice); // so it can re-place our pointer...
		if (FAILED(hr))
		{
			return false;
		}

		// activate an (the default, for us, since we want loopback) IAudioClient
		hr = m_pMMDevice->Activate(__uuidof(IAudioClient),
			CLSCTX_ALL, NULL,
			(void**)&pAudioClient_);

		if (FAILED(hr)) {
			printf("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08x", hr);
			return false;
		}

		m_pMMDevice->Release();

		REFERENCE_TIME hnsDefaultDevicePeriod;
		hr = pAudioClient_->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
		if (FAILED(hr))
		{
			printf("IAudioClient::GetDevicePeriod failed: hr = 0x%08x\n", hr);
			pAudioClient_->Release();
			return false;
		}

		// get the default device format (incoming...)
		WAVEFORMATEX *pwfx; // incoming wave...
		WAVEFORMATEX* pWfxClosestMatch = NULL;
		// apparently propogated by GetMixFormat...
		hr = pAudioClient_->GetMixFormat(&pwfx); // we free pwfx
		if (FAILED(hr))
		{
			printf("IAudioClient::GetMixFormat failed: hr = 0x%08x\n", hr);
			CoTaskMemFree(pwfx);
			pAudioClient_->Release();
			return false;
		}

		hr = pAudioClient_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pwfx,
			&pWfxClosestMatch);
		if (hr == S_OK) {
			printf("\n*** Is format supported success... ****\n");
			printf("\n pwfx->nChannels [%d], pwfx->wBitsPerSample [%d]",
				pwfx->nChannels, pwfx->wBitsPerSample);
		}
		else {
			printf("\n*** Is format supported failed hr [%d] ****\n", hr);
			printf("\n pwfx->nChannels [%d], pwfx->wBitsPerSample [%d], pWfxClosestMatch->nChannels [%d], pWfxClosestMatch->wBitsPerSample [%d]",
				pwfx->nChannels, pwfx->wBitsPerSample, pWfxClosestMatch->nChannels, pWfxClosestMatch->wBitsPerSample);
		}
		// coerce int-XX wave format (like int-16 or int-32)
		// can do this in-place since we're not changing the size of the format
		// also, the engine will auto-convert from float to int for us
		PWAVEFORMATEXTENSIBLE pEx;

		switch (pwfx->wFormatTag)
		{
		case WAVE_FORMAT_IEEE_FLOAT:
			pwfx->wFormatTag = WAVE_FORMAT_PCM;
			pwfx->wBitsPerSample = pwfx->wBitsPerSample / 2;

			pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
			pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
			//        if(1)
			//        printf("come in WAVE_FORMAT_IEEE_FLOAT ");
			break;

		case WAVE_FORMAT_EXTENSIBLE: // 65534
			// naked scope for case-local variable
			pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);

			if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
			{
				pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
				// convert it to PCM, but let it keep as many bits of precision as it has initially...though it always seems to be 32
				// comment this out and set wBitsPerSample to  pwfex->wBitsPerSample = getBitsPerSample(); to get an arguably "better" quality 32 bit pcm
				// unfortunately flash media live encoder basically rejects 32 bit pcm, and it's not a huge gain sound quality-wise, so disabled for now.
				pwfx->wBitsPerSample = pwfx->wBitsPerSample / 2;
				//  pwfx->nSamplesPerSec=44100;
				pEx->Samples.wValidBitsPerSample = pwfx->wBitsPerSample;
				pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
				pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
				//   printf("come in WAVE_FORMAT_EXTENSIBLE ");
				// see also setupPwfex method
			}
			else
			{
				printf("Don't know how to coerce mix format to int-16\n");
				CoTaskMemFree(pwfx);
				pAudioClient_->Release();
				return false;
			}
			break;

		default:
			printf("Don't know how to coerce WAVEFORMATEX with wFormatTag = 0x%08x to int-16\n", pwfx->wFormatTag);
			CoTaskMemFree(pwfx);
			pAudioClient_->Release();
			return false;
		}

		// -============================ now the sniffing code initialization stuff, direct from mauritius... ===================================

			// call IAudioClient::Initialize
			// note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
			// do not work together...
			// the "data ready" event never gets set
			// so we're going to have to do this in a timer-driven loop...

		hr = pAudioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED,
			0,
			REFTIMES_PER_SEC, // buffer size a full 1.0s, seems ok VLC
			0, pwfx, 0);
		if (FAILED(hr))
		{
			printf("IAudioClient::Initialize failed: hr = 0x%08x\n", hr);
			pAudioClient_->Release();
			return false;
		}

		nBlockAlign_ = pwfx->nBlockAlign;

		pwfx->nSamplesPerSec = 88200;
		if ((pwfx->nSamplesPerSec != 44100) && (pwfx->nSamplesPerSec != 48000) && (pwfx->nSamplesPerSec != 88200) &&
			(pwfx->nSamplesPerSec != 96000) && (pwfx->nSamplesPerSec != 192000))
		{
			return false;
		}


		UINT32 bufferFrameCount;
		// Get the size of the allocated buffer.
		hr = pAudioClient_->GetBufferSize(&bufferFrameCount);
		EXIT_ON_ERROR(hr)

			// Calculate the actual duration of the allocated buffer.
			hnsActualDuration = (double)REFTIMES_PER_SEC *	bufferFrameCount / pwfx->nSamplesPerSec;
		bits_per_sample = pwfx->wBitsPerSample;
	    sample_rate = pwfx->nSamplesPerSec;//?
		number_of_channels = pwfx->nChannels;

		CoTaskMemFree(pwfx);

		// activate an IAudioCaptureClient
		hr = pAudioClient_->GetService(__uuidof(IAudioCaptureClient),
			(void**)&pAudioCaptureClient_);

		if (FAILED(hr))
		{
			printf("IAudioClient::GetService(IAudioCaptureClient) failed: hr 0x%08x\n", hr);
			pAudioClient_->Release();
			return false;
		}


		// call IAudioClient::Start
		hr = pAudioClient_->Start();
		if (FAILED(hr)) {
			printf("IAudioClient::Start failed: hr = 0x%08x\n", hr);
			pAudioCaptureClient_->Release();
			pAudioClient_->Release();
			return false;
		}
		//pAudioClient_->Release();
		return true;
	}

	HRESULT AudioCaptureRaw::getDefaultDevice(IMMDevice **ppMMDevice)
	{
		HRESULT hr = S_OK;
		IMMDeviceEnumerator *pMMDeviceEnumerator;

		hr = CoInitialize(0);

		// activate a device enumerator
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator),
			(void**)&pMMDeviceEnumerator);

		if (FAILED(hr))
		{
			printf("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08x\n", hr);
			return hr;
		}

		// get the default render endpoint
		hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, ppMMDevice);
		pMMDeviceEnumerator->Release();

		if (FAILED(hr))
		{
			printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
			return hr;
		}

		return S_OK;
	}

	AudioCaptureRaw::~AudioCaptureRaw() {
		if (continueCapture) stopCapture();
		if (pAudioCaptureClient_) pAudioCaptureClient_->Release();
	}

	void AudioCaptureRaw::stopCapture() {
		assert(continueCapture);
		continueCapture = false;
		if (t_.joinable()) t_.join();
	}

	bool AudioCaptureRaw::startCapture(callback* cb) {
		assert(cb);
		assert(continueCapture == false);
		if(onceAudioInit == false)
				onceAudioInit = setConfiguration();
		assert(onceAudioInit);
		if (onceAudioInit) {
			continueCapture = true;
			t_ = std::thread{ &AudioCaptureRaw::startThread, this, cb };
		}
			
		return onceAudioInit;
	}

	void AudioCaptureRaw::startThread(callback* cb)
	{
		while (continueCapture) {
			if (!onceAudioInit)
			{
				onceAudioInit = setConfiguration();
			}

			if (onceAudioInit)
				readPacket(rawbuffer, cb);
			else
			{
				std::cout << "\nset configuration fails\n";
			}
		}
			
	}

	void AudioCaptureRaw::readPacket(UINT8 *buf, callback* ob)
	{
		try
		{

			static int count = 0;
			UINT32 nNextPacketSize = 0;
			HRESULT hr = pAudioCaptureClient_->GetNextPacketSize(&nNextPacketSize); // get next packet, if one is ready...
			if (FAILED(hr))
			{
				pAudioCaptureClient_->Release();

				onceAudioInit = false;
				if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
					printf("\nAUDCLNT_E_DEVICE_INVALIDATED");
				else if (hr == AUDCLNT_E_SERVICE_NOT_RUNNING)
					printf("\AUDCLNT_E_SERVICE_NOT_RUNNING");
				else if (hr == E_POINTER)
					printf("\E_POINTER");
				printf("\n\n  *****GetNextPacketSize()... failed.. \n");

				return;
				//return 0;			
			}
			else if (nNextPacketSize == 0)
			{
				// Sleep for half the buffer duration.
				Sleep(hnsActualDuration / REFTIMES_PER_MILLISEC / 2);
				count++;
				if (count == 4)
				{
					count = 0;
					pAudioCaptureClient_->Release();

					onceAudioInit = false;
					return;
				}
			}
			else
				count = 0;


			// get the captured data
			BYTE *pData = NULL;
			UINT32 nNumFramesToRead;
			DWORD dwFlags;

			// I guess it gives us...as much audio as possible to read...probably
			hr = pAudioCaptureClient_->GetBuffer(&pData,
				&nNumFramesToRead,
				&dwFlags,
				NULL,
				NULL);
			// ACTUALLY GET THE BUFFER which I assume it reads in the format of the fella we passed in

			if (hr != S_OK)
			{

				if (nNumFramesToRead == 0 && hr == AUDCLNT_S_BUFFER_EMPTY)
				{
					return;
					//return 0;
				}

				pAudioCaptureClient_->Release();

				onceAudioInit = false;
				printf("\n\n  *****GetNextPacketSize()... failed.. \n");

				return;
				//return 0;
			}

			long lBytesToWrite = nNumFramesToRead * nBlockAlign_; // nBlockAlign is "audio block size" or frame size, for one audio segment...
			if (dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)	// if silent fill silent in buffer
				memset(pData, 0, lBytesToWrite);

			memcpy(buf, pData, lBytesToWrite);

			hr = pAudioCaptureClient_->ReleaseBuffer(nNumFramesToRead);
			if (FAILED(hr))
			{

				pAudioCaptureClient_->Release();

				onceAudioInit = false;
				printf("\n\n  *****ReleaseBuffer()... failed.. \n");

				return;
				//return 0;							
			}

			/*
			const uint8_t* data, int size,
			int bits_per_sample,
			int sample_rate,
			size_t number_of_channels,
			size_t number_of_frames
			*/
			//const uint8_t* data = buf;
		
			size_t number_of_frames = nNumFramesToRead;
			ob->onData(buf, lBytesToWrite, bits_per_sample, sample_rate, number_of_channels, number_of_frames);

		}
		catch (...)
		{
			std::cout << "Error bcoz of headphone plugin...";
		}

	}

}	//AudioCapture