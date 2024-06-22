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

// �s���^�C�v�̒�`
const AMOVIESETUP_MEDIATYPE sudPinTypes = { &MEDIATYPE_Stream, &MEDIASUBTYPE_NULL };

// ���̓s���̏��
const AMOVIESETUP_PIN sudPins =
{
	OUTPUT_PIN_NAME,// �p�~
	FALSE,                // Is it rendered
	TRUE,                // Is it an output
	FALSE,                // Allowed none
	FALSE,                // Allowed many
	0,// �p�~
	0,// �p�~
	1,                    // Number of types
	&sudPinTypes          // Pin information
};

// �t�B���^���
const AMOVIESETUP_FILTER afFilterInfo = {
	&CLSID_MyAsyncSrc,      // �t�B���^��CLSID
	FILTER_NAME,			// �t�B���^��
	MERIT_DO_NOT_USE,       // �����b�g�l
	1,                      // �s����
	&sudPins                // �s�����
};

class CMyAsyncSrc;

// �ǂݎ���� 1 �̃T���v��
struct Samples {
	IMediaSample* pMs;
	DWORD user;
};

// �񓯊��o�̓s��
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

// �t�B���^�N���X
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
