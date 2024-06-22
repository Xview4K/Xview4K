#pragma once

#include "stdafx.h"

#include<list>
#include<queue>

#define TEMPLATE_NAME	(L"My AsyncSrc Filter")
#define FILTER_NAME		(TEMPLATE_NAME)
#define OUTPUT_PIN_NAME (L"Output")
#define ALLOCATOR_NAME  (L"My Allocator")

// {B32CC60D-D546-4a90-8354-31F1487EB684}
DEFINE_GUID(CLSID_MyAsyncSrc,
	0xb32cc60d, 0xd546, 0x4a90, 0x83, 0x54, 0x31, 0xf1, 0x48, 0x7e, 0xb6, 0x84);

// ピンタイプの定義
const AMOVIESETUP_MEDIATYPE sudPinTypes = { &MEDIATYPE_Stream, &MEDIASUBTYPE_NULL };

// 入力ピンの情報
const AMOVIESETUP_PIN sudPins =
{
	OUTPUT_PIN_NAME,// 廃止
	FALSE,                // Is it rendered
	TRUE,                // Is it an output
	FALSE,                // Allowed none
	FALSE,                // Allowed many
	0,// 廃止
	0,// 廃止
	1,                    // Number of types
	&sudPinTypes          // Pin information
};

// フィルタ情報
const AMOVIESETUP_FILTER afFilterInfo = {
	&CLSID_MyAsyncSrc,      // フィルタのCLSID
	FILTER_NAME,			// フィルタ名
	MERIT_DO_NOT_USE,       // メリット値
	1,                      // ピン数
	&sudPins                // ピン情報
};

class CMyAsyncSrc;

// 読み取った 1 つのサンプル
struct Samples {
	IMediaSample* pMs;
	DWORD user;
};

// 非同期出力ピン
class CAsyncOutPin : public IAsyncReader, public CBasePin {
public:
	DECLARE_IUNKNOWN;
	CAsyncOutPin(HRESULT* phr, CBaseFilter* pFilter, CCritSec* pLock, INT media_type);
	~CAsyncOutPin();
	STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);

	// CBasePin
	STDMETHODIMP Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt);
	HRESULT GetMediaType(int iPosition, CMediaType* pMediaType);
	HRESULT CheckMediaType(const CMediaType* pType);
	HRESULT CheckConnect(IPin* pPin);
	HRESULT CompleteConnect(IPin* pReceivePin);
	HRESULT BreakConnect();
	// IAsyncReader
	STDMETHODIMP RequestAllocator(IMemAllocator* pPreferred, ALLOCATOR_PROPERTIES* pProps, IMemAllocator** ppActual);
	STDMETHODIMP Request(IMediaSample* pSample, DWORD_PTR dwUser);
	STDMETHODIMP WaitForNext(DWORD dwTimeout, IMediaSample** ppSample, DWORD_PTR* pdwUser);
	STDMETHODIMP SyncReadAligned(IMediaSample* pSample);
	STDMETHODIMP SyncRead(LONGLONG llPosition, LONG lLength, BYTE* pBuffer);
	STDMETHODIMP Length(LONGLONG* pTotal, LONGLONG* pAvailable);
	STDMETHODIMP BeginFlush(void);
	STDMETHODIMP EndFlush(void);
private:
	HRESULT InitAllocator(IMemAllocator** ppAlloc);
	HRESULT ReadData(LONGLONG pos, LONG length, BYTE* pBuffer, DWORD* pReadSize = NULL);
	bool			m_bQueriedForAsyncReader;
	CMyAsyncSrc* m_pFilter;
	CMediaType		m_mt;
	HANDLE			m_hFile;
	INT				m_MediaType; // specify Video:0 or Audio:1
	HANDLE			m_hWait;
	std::queue<Samples> m_Data;
	bool			m_Flush;
	CCritSec		m_DataLock;
	CCritSec		m_ReadLock;
};

// フィルタクラス
class CMyAsyncSrc : public CBaseFilter {
public:
	DECLARE_IUNKNOWN;
	CMyAsyncSrc(LPUNKNOWN pUnk, HRESULT* phr, INT media_type);
	~CMyAsyncSrc();
	static CUnknown* WINAPI CreateInstance(LPUNKNOWN, HRESULT*, INT);
	int GetPinCount();
	CBasePin* GetPin(int n);
	HRESULT Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt);
private:
	CAsyncOutPin* m_pOutPin;
	CCritSec		m_lock;
};
