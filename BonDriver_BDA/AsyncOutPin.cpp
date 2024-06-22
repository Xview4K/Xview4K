#include "stdafx.h"

#include "frame_buffer.h"
#include "tlv_parse.h"

CAsyncOutPin::CAsyncOutPin(HRESULT* phr, CBaseFilter* pFilter, CCritSec* pLock, INT media_type)
	: CBasePin(OUTPUT_PIN_NAME, pFilter, pLock, phr, OUTPUT_PIN_NAME, PINDIR_OUTPUT)
	, m_bQueriedForAsyncReader(false), m_hFile(INVALID_HANDLE_VALUE)
{
	m_pFilter = dynamic_cast<CMyAsyncSrc*>(pFilter);
	m_mt.majortype = MEDIATYPE_Stream;
	if (media_type == 0) {
		m_mt.subtype = MEDIASUBTYPE_NULL;
	}
	else if (media_type == 1) {
		m_mt.subtype = MEDIASUBTYPE_NULL;
	}
	else {
		m_mt.subtype = MEDIASUBTYPE_NULL;
	}
	m_MediaType = media_type;
	//	m_hFile = CreateFile(FILENAME, GENERIC_READ
	//		, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	m_hWait = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_Flush = false;
}

CAsyncOutPin::~CAsyncOutPin() {
	CloseHandle(m_hFile);
	CloseHandle(m_hWait);
}

STDMETHODIMP CAsyncOutPin::NonDelegatingQueryInterface(REFIID riid, void** ppv) {
	CheckPointer(ppv, E_POINTER);
	if (riid == IID_IAsyncReader) {
		DbgLog((LOG_TRACE, 5, TEXT("QI , riid=IID_IAsyncReader")));
		m_bQueriedForAsyncReader = true;
		return GetInterface(dynamic_cast<IAsyncReader*>(this), ppv);
	}
	return __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CAsyncOutPin::Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) {
	CheckPointer(m_pFilter, E_UNEXPECTED);
	DbgLog((LOG_TRACE, 5, TEXT("Connect")));
	return m_pFilter->Connect(pReceivePin, pmt);
}

HRESULT CAsyncOutPin::GetMediaType(int iPosition, CMediaType* pMediaType) {
	if (iPosition < 0)
		return E_INVALIDARG;
	if (iPosition > 0)
		return VFW_S_NO_MORE_ITEMS;
	CheckPointer(pMediaType, E_POINTER);
	CheckPointer(m_pFilter, E_UNEXPECTED);
	DbgLog((LOG_TRACE, 5, TEXT("GetMediaType")));
	*pMediaType = m_mt;
	return S_OK;
}

HRESULT CAsyncOutPin::CheckMediaType(const CMediaType* pType) {
	CAutoLock lck(m_pLock); // STATE LOCK
	if (m_mt.majortype == pType->majortype && m_mt.subtype == pType->subtype) {
		DbgLog((LOG_TRACE, 5, TEXT("CheckMediaType S_OK")));
		return S_OK;
	}
	DbgLog((LOG_TRACE, 5, TEXT("CheckMediaType S_FALSE")));
	return S_FALSE;
}

HRESULT CAsyncOutPin::CheckConnect(IPin* pPin) {
	DbgLog((LOG_TRACE, 5, TEXT("CheckConnect")));
	m_bQueriedForAsyncReader = false;
	return CBasePin::CheckConnect(pPin);
}

HRESULT CAsyncOutPin::CompleteConnect(IPin* pReceivePin) {
	if (m_bQueriedForAsyncReader) {
		DbgLog((LOG_TRACE, 5, TEXT("CompleteConnect")));
		return CBasePin::CompleteConnect(pReceivePin);
	}
	DbgLog((LOG_TRACE, 5, TEXT("CompleteConnect VFW_E_NO_TRANSPORT")));
	return VFW_E_NO_TRANSPORT;
}

HRESULT CAsyncOutPin::BreakConnect() {
	DbgLog((LOG_TRACE, 5, TEXT("BreakConnect")));
	m_bQueriedForAsyncReader = false;
	return CBasePin::BreakConnect();
}

// IAsyncReader ...
STDMETHODIMP CAsyncOutPin::RequestAllocator(IMemAllocator* pPreferred
	, ALLOCATOR_PROPERTIES* pProps, IMemAllocator** ppActual)
{
	puts("RequestAllocator");

	HRESULT hr = NOERROR;
	CheckPointer(pPreferred, E_POINTER);
	CheckPointer(pProps, E_POINTER);
	CheckPointer(ppActual, E_POINTER);
	DbgLog((LOG_TRACE, 5, TEXT("RequestAllocator")));

	ALLOCATOR_PROPERTIES Actual;
	IMemAllocator* pAlloc = NULL;

	if (m_MediaType == 0) {
		printf("V cbBuffer:%ld cBuffers:%ld cbAlign:%ld cbPrefix:%ld\n", pProps->cbBuffer, pProps->cBuffers, pProps->cbAlign, pProps->cbPrefix);
	}
	else {
		//		pProps->cbBuffer = 8192;
		printf("A cbBuffer:%ld cBuffers:%ld cbAlign:%ld cbPrefix:%ld\n", pProps->cbBuffer, pProps->cBuffers, pProps->cbAlign, pProps->cbPrefix);
	}

	if (pPreferred) {
		hr = pPreferred->SetProperties(pProps, &Actual);
		pPreferred->AddRef();
		*ppActual = pPreferred;
		return hr;
	}

	hr = InitAllocator(&pAlloc);
	if (FAILED(hr)) {
		return hr;
	}
	hr = pAlloc->SetProperties(pProps, &Actual);
	if (SUCCEEDED(hr)) {
		*ppActual = pAlloc;
		return hr; // S__OK
	}
	pAlloc->Release();
	if (SUCCEEDED(hr)) {
		hr = VFW_E_BADALIGN;
	}
	return hr;
}

STDMETHODIMP CAsyncOutPin::Request(IMediaSample* pSample, DWORD_PTR dwUser) {
	puts("Request");

	CheckPointer(pSample, E_POINTER);
	DbgLog((LOG_TRACE, 5, TEXT("Request pSample=%p dwUser=%lu"), pSample, dwUser));
	REFERENCE_TIME tStart, tStop;
	if (m_Flush) {
		// フラッシュ状態のときは、これ以上要求を受け付けない.
		SetEvent(m_hWait);
		return VFW_E_WRONG_STATE;
	}
	HRESULT hr = pSample->GetTime(&tStart, &tStop);
	if (FAILED(hr)) {
		SetEvent(m_hWait);
		return hr;
	}
	LONGLONG llPos = tStart / UNITS;
	LONG     lLength = (LONG)((tStop - tStart) / UNITS);
	LONGLONG llTotal = 0;
	LONGLONG llAvailable = 0;

	BYTE* pBuffer = NULL;
	hr = pSample->GetPointer(&pBuffer);
	if (FAILED(hr)) {
		SetEvent(m_hWait);
		return hr;
	}
	DWORD readsize;
	hr = ReadData(llPos, lLength, pBuffer, &readsize);
	if (FAILED(hr)) {
		SetEvent(m_hWait);
		return E_FAIL;
	}
	pSample->SetActualDataLength(readsize);
	Samples s = { pSample,dwUser };
	{
		CAutoLock mylock(&m_DataLock);
		m_Data.push(s);
		hr = S_OK;
		SetEvent(m_hWait);
		DbgLog((LOG_TRACE, 5, TEXT("Wrote %lu"), dwUser));
	}
	return hr;
}

STDMETHODIMP CAsyncOutPin::WaitForNext(DWORD dwTimeout
	, IMediaSample** ppSample, DWORD_PTR* pdwUser)
{
	puts("WaitForNext");

	HRESULT hr = S_OK;
	CheckPointer(ppSample, E_POINTER);
	bool e = false;
	*ppSample = NULL;
	*pdwUser = 0;
	DbgLog((LOG_TRACE, 5, TEXT("WaitForNext timeout=%d ppSample=%p pdwUser=%p dwUser=%lu flush=%d")
		, dwTimeout, ppSample, pdwUser, *pdwUser, m_Flush));
	if (WaitForSingleObject(m_hWait, dwTimeout) == WAIT_TIMEOUT) {
		DbgLog((LOG_TRACE, 5, TEXT("WAIT_TIMEOUT")));
		return VFW_E_TIMEOUT;
	}
	{
		CAutoLock mylock(&m_DataLock);
		DbgLog((LOG_TRACE, 5, TEXT("locked m_Flush=%d"), m_Flush));
		if (!m_Data.empty()) {
			Samples s = m_Data.front();
			*ppSample = s.pMs;
			*pdwUser = s.user;
			DbgLog((LOG_TRACE, 5, TEXT("s.user = %x"), s.user));
			m_Data.pop();
			if (m_Data.empty()) {
				ResetEvent(m_hWait);
				DbgLog((LOG_TRACE, 5, TEXT("empty")));
			}
			hr = S_OK;
		}
		else {
			if (m_Flush) {
				hr = VFW_E_WRONG_STATE;
			}
			else {
				// キューに何もない && フラッシュ状態でない は, ありえないはず...
				DbgLog((LOG_TRACE, 5, TEXT("E_FAIL")));
				hr = E_FAIL;
			}
		}
		DbgLog((LOG_TRACE, 5, TEXT("unlocked HRESULT=%X"), hr));
	}
	return hr;
}

STDMETHODIMP CAsyncOutPin::SyncReadAligned(IMediaSample* pSample) {
	puts("SyncReadAligned");

	DbgLog((LOG_TRACE, 5, TEXT("SyncReadAligned pSample=%p"), pSample));
	CheckPointer(pSample, E_POINTER);

	REFERENCE_TIME tStart, tStop;
	HRESULT hr = pSample->GetTime(&tStart, &tStop);
	LONGLONG llPos = tStart / UNITS;
	LONG     lLength = (LONG)((tStop - tStart) / UNITS);
	LONGLONG llTotal = 0;
	LONGLONG llAvailable = 0;

	BYTE* pBuffer = NULL;
	hr = pSample->GetPointer(&pBuffer);
	if (FAILED(hr)) {
		return hr;
	}
	hr = ReadData(llPos, lLength, pBuffer);
	return hr;
}
STDMETHODIMP CAsyncOutPin::SyncRead(LONGLONG llPosition, LONG lLength, BYTE* pBuffer) {
	//	puts("SyncRead");
	//	DbgLog((LOG_TRACE, 5, TEXT("SyncRead pos=%I64d len=%ld buf=%p"), llPosition, lLength, pBuffer));
	HRESULT hr = ReadData(llPosition, lLength, pBuffer);
	return hr;
}

// フラッシュ開始
// 未処理の読み取り要求をすべて中断する.
// 以後、EndFlushが呼ばれるまで、
// Request, WaitForNext メソッドは VFW_E_WRONG_STATE を戻り値として失敗する。
STDMETHODIMP CAsyncOutPin::BeginFlush(void) {
	//puts("BeginFlush");
	//DbgLog((LOG_TRACE, 5, TEXT("BeginFlush")));
	//CAutoLock mylock(&m_DataLock);
	//m_Flush = true;
	//SetEvent(m_hWait); // イベントシグナル ON にして WaitForNext の シグナル待ちを解放
	//DbgLog((LOG_TRACE, 5, TEXT("BeginFlush Done")));
	return S_OK;
}

// フラッシュ終了
// 以後、Request メソッドが使用可能になること.
STDMETHODIMP CAsyncOutPin::EndFlush(void) {
	//puts("EndFlush");
	//DbgLog((LOG_TRACE, 5, TEXT("EndFlush")));
	//{
	//	CAutoLock mylock(&m_DataLock);
	//	m_Flush = false;
	//	m_Data = std::queue<Samples>();
	//	ResetEvent(m_hWait);
	//}
	return S_OK;
}

// private methods...

HRESULT CAsyncOutPin::InitAllocator(IMemAllocator** ppAlloc) {
	puts("InitAllocator");

	CheckPointer(ppAlloc, E_POINTER);
	DbgLog((LOG_TRACE, 5, TEXT("InitAllocator")));
	HRESULT hr = NOERROR;
	CMemAllocator* pMemObject = NULL;
	*ppAlloc = NULL;
	pMemObject = new CMemAllocator(ALLOCATOR_NAME, NULL, &hr);
	if (pMemObject == NULL) {
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr)) {
		delete pMemObject;
		return hr;
	}
	hr = pMemObject->QueryInterface(IID_IMemAllocator, (void**)ppAlloc);
	if (FAILED(hr)) {
		delete pMemObject;
		return E_NOINTERFACE;
	}
	ASSERT(*ppAlloc != NULL);
	return S_OK;
}

extern  CFrameBuffer* video_frame_buffer;
extern  CFrameBuffer* audio_frame_buffer;

HRESULT CAsyncOutPin::ReadData(LONGLONG pos, LONG length, BYTE* pBuffer, DWORD* pReadSize) {
	CAutoLock mylock(&m_ReadLock);

	LONG readsize = 0;
	BOOL result = FALSE;


	if (m_MediaType == 0) { // media type Video
//		printf("V pos:%I64d len:%ld\n", pos - video_frame_buffer->pre_pos, length);

		if ((video_frame_buffer->llTotalReadBytes - video_frame_buffer->llTotalWriteBytes) < -(video_frame_buffer->buffer_size * 0.95)) { // Video Overflow
			video_frame_buffer->readPos += video_frame_buffer->buffer_size;
			video_frame_buffer->llTotalReadBytes += video_frame_buffer->buffer_size;
			if (video_frame_buffer->readPos >= video_frame_buffer->buffer_size) {
				video_frame_buffer->readPos -= video_frame_buffer->buffer_size;
			}
			++video_frame_buffer->countOverflow;
		}

		//		printf("W:%I64d R:%I64d D:%I64d\n", video_frame_buffer->llTotalWriteBytes, video_frame_buffer->llTotalReadBytes, video_frame_buffer->llTotalReadBytes - video_frame_buffer->llTotalWriteBytes);
#define VIDEO_READ_TIMEOUT 3000 // 2s
#define AUDIO_READ_TIMEOUT 3000 // 2s

		ULONGLONG tick = GetTickCount64();
	VIDEO_READ_WAIT_LOOP:
		readsize = length;
		if ((video_frame_buffer->llTotalReadBytes + readsize) >= video_frame_buffer->llTotalWriteBytes) { // Video Underflow
			if ((GetTickCount64() - tick) < VIDEO_READ_TIMEOUT) {
				goto VIDEO_READ_WAIT_LOOP;
			}
			OutputDebugString(L"VIDEO_READ_TIMEOUT\n");
			readsize = 0;
			++video_frame_buffer->countUnderflow;
		}
		video_frame_buffer->Read(pBuffer, length);
		result = 1;
	}
	else if (m_MediaType == 1) { // media_type Audio
//		printf("A pos:%I64d len:%ld\n", pos - audio_frame_buffer->pre_pos, length);

		//		printf("pos:%I64d len:%ld\n", pos, length);
		//		printf("W:%I64d R:%I64d D:%I64d\n", audio_frame_buffer->llTotalWriteBytes, audio_frame_buffer->llTotalReadBytes, audio_frame_buffer->llTotalReadBytes - audio_frame_buffer->llTotalWriteBytes);
		ULONGLONG tick = GetTickCount64();
	AUDIO_READ_WAIT_LOOP:
		readsize = length;
		if ((audio_frame_buffer->llTotalReadBytes + readsize) >= audio_frame_buffer->llTotalWriteBytes) { // Audio Underflow
			if ((GetTickCount64() - tick) < AUDIO_READ_TIMEOUT) {
				goto AUDIO_READ_WAIT_LOOP;
			}
			OutputDebugString(L"AUDIO_READ_TIMEOUT\n");
			readsize = 0;
			++audio_frame_buffer->countUnderflow;
		}
		//		printf("readsize:%ld\n", readsize);
		audio_frame_buffer->Read(pBuffer, readsize);
		result = 1;
	}

	if (pReadSize != NULL) {
		*pReadSize = readsize;
	}

	if (result == 0) {
		return E_FAIL;
	}

	return S_OK;
}

STDMETHODIMP CAsyncOutPin::Length(LONGLONG* pTotal, LONGLONG* pAvailable) {
	//	puts("Length");
	DbgLog((LOG_TRACE, 5, TEXT("Length")));
	//LARGE_INTEGER x;
	//GetFileSizeEx(m_hFile, &x);
	//*pTotal = x.QuadPart;
	//*pAvailable = x.QuadPart;
	if (m_MediaType == 0) { // media type Video
		*pTotal = video_frame_buffer->llTotalReadBytes - video_frame_buffer->llTotalWriteBytes;
		*pAvailable = video_frame_buffer->llTotalReadBytes - video_frame_buffer->llTotalWriteBytes;
	}
	else {
		*pTotal = audio_frame_buffer->llTotalReadBytes - audio_frame_buffer->llTotalWriteBytes;
		*pAvailable = audio_frame_buffer->llTotalReadBytes - audio_frame_buffer->llTotalWriteBytes;
	}

	//*pTotal = -1;
	//*pAvailable = -1;

	return S_OK;
}

