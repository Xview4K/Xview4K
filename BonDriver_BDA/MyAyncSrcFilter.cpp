#include "stdafx.h"
#include <exception>

CUnknown* WINAPI CMyAsyncSrc::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr, INT media_type) {
	CMyAsyncSrc* pNewFilter = NULL;
	try {
		pNewFilter = new CMyAsyncSrc(pUnk, phr, media_type);
	}
	catch (std::bad_alloc) {
		*phr = E_OUTOFMEMORY;
	}
	return dynamic_cast<CUnknown*>(pNewFilter);
}

CMyAsyncSrc::CMyAsyncSrc(IUnknown* pUnk, HRESULT* phr, INT media_type) :
	CBaseFilter(FILTER_NAME, pUnk, &m_lock, CLSID_MyAsyncSrc, NULL)
{
	m_pOutPin = new CAsyncOutPin(phr, this, &m_lock, media_type);
}

CMyAsyncSrc::~CMyAsyncSrc() {
	if (m_pOutPin) {
		delete m_pOutPin;
		m_pOutPin = NULL;
	}
}

int CMyAsyncSrc::GetPinCount() {
	return 1;
}
CBasePin* CMyAsyncSrc::GetPin(int n) {
	if (GetPinCount() > 0 && n == 0) {
		return m_pOutPin;
	}
	return NULL;
}

HRESULT CMyAsyncSrc::Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) {
	return m_pOutPin->CBasePin::Connect(pReceivePin, pmt);
}
