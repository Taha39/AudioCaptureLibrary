
#include <thread>
#include "GetAudio.h"
#include <cassert>
#include <iostream>
#include <sstream>

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { return false; }

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const IID IID_IAudioClient = __uuidof(IAudioClient);

static std::ostringstream loger_;

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
			loger_ << "IMMDevice::Activate(IAudioClient) failed: hr = " << hr << std::endl;

			return false;
		}

		m_pMMDevice->Release();

		REFERENCE_TIME hnsDefaultDevicePeriod;
		hr = pAudioClient_->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
		if (FAILED(hr))
		{
			loger_ << "IAudioClient::GetDevicePeriod failed: hr = " << hr << std::endl;
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
			loger_ << "IAudioClient::GetMixFormat failed: hr = " << hr << std::endl;
			CoTaskMemFree(pwfx);
			pAudioClient_->Release();
			return false;
		}

		hr = pAudioClient_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pwfx,
			&pWfxClosestMatch);
		if (hr == S_OK) {
			loger_ << "\n*** Is format supported success... ****\n" << std::endl;
			loger_ << "Channels: " << pwfx->nChannels << ", BitsPerSample: " << pwfx->wBitsPerSample
				<< ", SamplesPerSeconds: " << pwfx->nSamplesPerSec << std::endl;
		}
		else {
			loger_ << "\n*** Is format supported failed hr [%d] ****\n" << std::endl;
			loger_  << "Channels: " << pwfx->nChannels << ", BitsPerSample: " << pwfx->wBitsPerSample
				<< ", SamplesPerSeconds: " << pwfx->nSamplesPerSec << std::endl;
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
				pEx->Samples.wValidBitsPerSample = pwfx->wBitsPerSample;
				pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
				pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
				// see also setupPwfex method
			}
			else
			{
				loger_ << "Don't know how to coerce mix format to int-16" << std::endl;
				CoTaskMemFree(pwfx);
				pAudioClient_->Release();
				return false;
			}
			break;

		default:
			loger_ << "Don't know how to coerce WAVEFORMATEX" << std::endl;
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
			loger_ << "IAudioClient::Initialize failed: hr = " << hr << std::endl;
			pAudioClient_->Release();
			return false;
		}

		nBlockAlign_ = pwfx->nBlockAlign;


		UINT32 bufferFrameCount;
		// Get the size of the allocated buffer.
		hr = pAudioClient_->GetBufferSize(&bufferFrameCount);
		EXIT_ON_ERROR(hr)

			// Calculate the actual duration of the allocated buffer.
		hnsActualDuration = (double)REFTIMES_PER_SEC *	bufferFrameCount / pwfx->nSamplesPerSec;
		bitsPerSample = pwfx->wBitsPerSample;
	    sampleRate = pwfx->nSamplesPerSec;//?
		numberOfChannels = pwfx->nChannels;

		CoTaskMemFree(pwfx);

		// activate an IAudioCaptureClient
		hr = pAudioClient_->GetService(__uuidof(IAudioCaptureClient),
			(void**)&pAudioCaptureClient_);

		if (FAILED(hr))
		{
			loger_ << "IAudioClient::GetService(IAudioCaptureClient) failed: hr = " << hr << std::endl;
			pAudioClient_->Release();
			return false;
		}


		// call IAudioClient::Start
		hr = pAudioClient_->Start();
		if (FAILED(hr)) {
			loger_ << "IAudioClient::Start failed: hr = " << hr << std::endl;
			pAudioCaptureClient_->Release();
			pAudioClient_->Release();
			return false;
		}
		return true;
	}

	//todo: no need to return HRESULT, return true/false
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
			loger_ << "CoCreateInstance(IMMDeviceEnumerator) failed: hr =" << hr << std::endl;
			return hr;
		}

		// get the default render endpoint
		hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, ppMMDevice);
		pMMDeviceEnumerator->Release();

		if (FAILED(hr))
		{
			loger_ << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = " << hr << std::endl;
			return hr;
		}

		return S_OK;
	}

	AudioCaptureRaw::~AudioCaptureRaw() {
		if (continueCapture) stop();
		if (pAudioCaptureClient_) pAudioCaptureClient_->Release();
	}

	void AudioCaptureRaw::stop() {
		assert(continueCapture);
		continueCapture = false;
		if (t_.joinable()) t_.join();
	}

	bool AudioCaptureRaw::start(callback* cb) {
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

	std::string AudioCaptureRaw::logs() {
		const auto log = loger_.str();
		/*loger_.clear();
		assert(log != loger_.str());*/
		return log;
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
				loger_ << "\nset configuration fails\n";
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
					loger_ << "\nAUDCLNT_E_DEVICE_INVALIDATED" << std::endl;
				else if (hr == AUDCLNT_E_SERVICE_NOT_RUNNING)
					loger_ << "\nAUDCLNT_E_SERVICE_NOT_RUNNING" << std::endl;
				else if (hr == E_POINTER)
					loger_ << "\nE_POINTER" << std::endl;
				loger_ << "\n\n  *****GetNextPacketSize()... failed.. \n" << std::endl;

				return;
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
				loger_ << "\n\n  *****GetNextPacketSize()... failed.. " << std::endl;

				return;
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
				loger_ << "\n\n  *****ReleaseBuffer()... failed.. " << std::endl;

				return;
			}

			ob->onData(buf, lBytesToWrite, bitsPerSample, sampleRate, numberOfChannels, nNumFramesToRead);

		}
		catch (...)
		{
			loger_ << "Error bcoz of headphone plugin..." << std::endl;
		}

	}

}	//AudioCapture