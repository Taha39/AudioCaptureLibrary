
#include <thread>
#include "GetAudio.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <Functiondiscoverykeys_devpkey.h>


#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const IID IID_IAudioClient = __uuidof(IAudioClient);

static std::ostringstream loger_;

struct release_helper {
	IUnknown* resource = nullptr;
	release_helper(IUnknown* ptr) :resource{ ptr } {}
	~release_helper() { if (resource)resource->Release(); }
};

struct Unintializer {
	~Unintializer() {
		CoUninitialize();
	}
};

namespace detail {
	grt::mic_list get_mic_list() {
		IMMDeviceEnumerator *pMMDeviceEnumerator = nullptr;;

		HRESULT hr = CoInitialize(0);
		/*if (FAILED(hr)) {
			loger_ << "failed in cointilze mic list \n";
			assert(false);
			return grt::mic_list{};
		}
		
		Unintializer unintializer_helper{};*/
		// activate a device enumerator
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator),
			(void**)&pMMDeviceEnumerator);
		if (FAILED(hr)) {
			loger_ << "failed in coCreateInstance mic list \n";
			assert(false);
			return grt::mic_list{};
		}
		release_helper enumerator_releaser{ pMMDeviceEnumerator };

		IMMDeviceCollection *pCollection = NULL;
		hr = pMMDeviceEnumerator->EnumAudioEndpoints(
			eCapture, DEVICE_STATE_ACTIVE,
			&pCollection);
		if (FAILED(hr)) {
			loger_ << "failed in deviceenumerator mic list \n";
			assert(false);
			return grt::mic_list{};
		}
		release_helper collection_releaser{ pCollection };
		UINT  count;
		hr = pCollection->GetCount(&count);
		if (FAILED(hr)) {
			loger_ << "failed in getCount mic list \n";
			assert(false);
			return grt::mic_list{};
		}
	
		grt::mic_list list;
		list.reserve(count);
		// Each loop prints the name of an endpoint device.
		for (ULONG i = 0; i < count; i++)
		{
			IMMDevice *pEndpoint = NULL;
			// Get pointer to endpoint number i.
			hr = pCollection->Item(i, &pEndpoint);
			if (FAILED(hr)) {
				loger_ << "failed in collection item mic list \n";
				continue;
			}
			release_helper endpoint_releaser{ pEndpoint };
			LPWSTR pwszID = NULL;
			// Get the endpoint ID string.
			hr = pEndpoint->GetId(&pwszID);
			if (FAILED(hr)) {
				loger_ << "failed in getid mic list \n";
				continue;
			}
			IPropertyStore *pProps = NULL;
			hr = pEndpoint->OpenPropertyStore(
					STGM_READ, &pProps);
			if (FAILED(hr)) {
				loger_ << "failed in open property mic list \n";
				continue;
			}
			release_helper props_release(pProps);
			PROPVARIANT varName;
			// Initialize container for property value.
			PropVariantInit(&varName);
			
			// Get the endpoint's friendly-name property.
			hr = pProps->GetValue(
				PKEY_Device_FriendlyName, &varName);

			if (FAILED(hr)) {
				loger_ << "failed in GetValue mic list \n";
				continue;
			}

			grt::device_info info;
			const std::wstring id = pwszID;
			info.id_ = { id.begin(), id.end() };
			info.index_ = i;
			info.kind_ = "mic";
			const std::wstring name = varName.pwszVal;
			info.name_ = { name.begin(), name.end() };
			list.push_back(info);

			CoTaskMemFree(pwszID);
		}

		return list;

	}

	IMMDevice*  get_device(grt::device_info device) {
		if (device.id_.empty()) return nullptr;
		IMMDeviceEnumerator *pMMDeviceEnumerator = nullptr;

		auto hr = CoInitialize(0);
	/*	if (FAILED(hr)) {
			assert(false);
			return nullptr;
		}*/
		// activate a device enumerator
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator),
			(void**)&pMMDeviceEnumerator);
		
		if (FAILED(hr))
		{
			assert(false);
			return nullptr;
		}
		release_helper enumerator_releaser{ pMMDeviceEnumerator };
		const std::wstring id{ device.id_.begin(), device.id_.end() };
		IMMDevice* imDevice = nullptr;
		pMMDeviceEnumerator->GetDevice(id.c_str(), &imDevice);
		return imDevice;

	}


	IMMDevice* getDefaultDevice(){

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
			return nullptr;
		}
		release_helper enumerator_releaser{ pMMDeviceEnumerator };
		// get the default render endpoint
		IMMDevice *ppMMDevice = nullptr;
		hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &ppMMDevice);
		
		if (FAILED(hr))
		{
			loger_ << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = " << hr << std::endl;
		}

		return ppMMDevice;
	}


}//namespace detail

namespace AudioCapture {

	grt::mic_list get_mic_list() {
		return detail::get_mic_list();
	}

	bool AudioCaptureRaw::setConfiguration(grt::device_info device)
	{
		IMMDevice *m_pMMDevice = detail::get_device(device);
		if (m_pMMDevice == nullptr) {
			loger_ << "Audio setConfiguration failed to get selected device trying for default device \n";
			m_pMMDevice  = detail::getDefaultDevice();
			if (m_pMMDevice == nullptr) {
				loger_ << "Audio setConfiguration failed to get default device \n";
				assert(false);
				return false;
			}
		}
		 
		// activate an (the default, for us, since we want loopback) IAudioClient
		auto hr = m_pMMDevice->Activate(__uuidof(IAudioClient),
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
		if (FAILED(hr))
		{
			loger_ << "IAudioClient::GetBufferSize failed: hr = " << hr << std::endl;
			pAudioClient_->Release();
			return false;
		}

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
	//HRESULT AudioCaptureRaw::getDefaultDevice(IMMDevice **ppMMDevice)
	//{
	//	HRESULT hr = S_OK;
	//	IMMDeviceEnumerator *pMMDeviceEnumerator;

	//	hr = CoInitialize(0);

	//	// activate a device enumerator
	//	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
	//		__uuidof(IMMDeviceEnumerator),
	//		(void**)&pMMDeviceEnumerator);

	//	if (FAILED(hr))
	//	{
	//		loger_ << "CoCreateInstance(IMMDeviceEnumerator) failed: hr =" << hr << std::endl;
	//		return hr;
	//	}

	//	// get the default render endpoint
	//	hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, ppMMDevice);
	//	pMMDeviceEnumerator->Release();

	//	if (FAILED(hr))
	//	{
	//		loger_ << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = " << hr << std::endl;
	//		return hr;
	//	}

	//	return S_OK;
	//}

	AudioCaptureRaw::~AudioCaptureRaw() {
		if (continueCapture) stop();
		if (pAudioCaptureClient_) pAudioCaptureClient_->Release();
	}

	void AudioCaptureRaw::stop() {
		assert(continueCapture);
		continueCapture = false;
		if (t_.joinable()) t_.join();
	}

	bool AudioCaptureRaw::start(callback* cb, grt::device_info device) {
		assert(cb);
		assert(continueCapture == false);
		if(onceAudioInit == false)
				onceAudioInit = setConfiguration(device);
		//assert(onceAudioInit);
		if (onceAudioInit) {
			continueCapture = true;
			t_ = std::thread{ &AudioCaptureRaw::startThread, this, cb, device };
		}
			
		return onceAudioInit;
	}

	std::string AudioCaptureRaw::logs() {
		const auto log = loger_.str();
		/*loger_.clear();
		assert(log != loger_.str());*/
		return log;
	}

	void AudioCaptureRaw::startThread(callback* cb, grt::device_info device)
	{
		while (continueCapture) {
			if (!onceAudioInit)
			{
				onceAudioInit = setConfiguration(device);
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