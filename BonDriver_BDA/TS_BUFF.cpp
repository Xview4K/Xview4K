#include "common.h"

#include "TS_BUFF.h"

TS_DATA::TS_DATA(void)
	: pbyBuff(NULL),
	Size(0)
{
}

TS_DATA::TS_DATA(BYTE* data, size_t size, BOOL copy)
{
	if (copy) {
		pbyBuff = new BYTE[size];
		memcpy(pbyBuff, data, size);
	}
	else {
		pbyBuff = data;
	}
	Size = size;
}

TS_DATA::~TS_DATA(void) {
	SAFE_DELETE_ARRAY(pbyBuff);
}

TS_BUFF::TS_BUFF(void)
	: TempBuff(NULL),
	TempOffset(0),
	BuffSize(0),
	MaxCount(0)
{
	::InitializeCriticalSection(&cs);
}

TS_BUFF::~TS_BUFF(void)
{
	Purge();
	SAFE_DELETE_ARRAY(TempBuff);
	::DeleteCriticalSection(&cs);
}

void TS_BUFF::SetSize(size_t buffSize, size_t maxCount)
{
	Purge();
	SAFE_DELETE_ARRAY(TempBuff);
	if (buffSize) {
		TempBuff = new BYTE[buffSize];
	}
	BuffSize = buffSize;
	MaxCount = maxCount;
}

void TS_BUFF::Purge(void)
{
	// ��MTS�o�b�t�@
	::EnterCriticalSection(&cs);
	while (!List.empty()) {
		SAFE_DELETE(List.front());
		List.pop();
	}
	TempOffset = 0;
	::LeaveCriticalSection(&cs);
}

void TS_BUFF::Add(TS_DATA *pItem)
{
	::EnterCriticalSection(&cs);
	while (List.size() >= MaxCount) {
		// �I�[�o�[�t���[�Ȃ�Â����̂�����
		SAFE_DELETE(List.front());
		List.pop();
	}
	List.push(pItem);
	::LeaveCriticalSection(&cs);
}

BOOL TS_BUFF::AddData(BYTE *pbyData, size_t size)
{
	BOOL ret = false;
	while (size) {
		TS_DATA *pItem = NULL;
		::EnterCriticalSection(&cs);
		if (TempBuff) {
			// ini�t�@�C����BuffSize���w�肳��Ă���ꍇ�͂��̃T�C�Y�ɍ��킹��
			size_t copySize = (BuffSize > TempOffset + size) ? size : BuffSize - TempOffset;
			memcpy(TempBuff + TempOffset, pbyData, copySize);
			TempOffset += copySize;
			size -= copySize;
			pbyData += copySize;

			if (TempOffset >= BuffSize) {
				// �e���|�����o�b�t�@�̃f�[�^��ǉ�
				pItem = new TS_DATA(TempBuff, TempOffset, FALSE);
				TempBuff = new BYTE[BuffSize];
				TempOffset = 0;
			}
		}
		else {
			// BuffSize���w�肳��Ă��Ȃ��ꍇ�͏㗬����󂯎�����T�C�Y�ł��̂܂ܒǉ�
			pItem = new TS_DATA(pbyData, size, TRUE);
			size = 0;
		}

		if (pItem) {
			// FIFO�֒ǉ�
			while (List.size() >= MaxCount) {
				// �I�[�o�[�t���[�Ȃ�Â����̂�����
				SAFE_DELETE(List.front());
				List.pop();
			}
			List.push(pItem);
			ret = TRUE;
		}
		::LeaveCriticalSection(&cs);
	}
	return ret;
}

TS_DATA * TS_BUFF::Get(void)
{
	TS_DATA *ts = NULL;
	::EnterCriticalSection(&cs);
	if (!List.empty()) {
		ts = List.front();
		List.pop();
	}
	::LeaveCriticalSection(&cs);
	return ts;
}

size_t TS_BUFF::Size(void)
{
	return List.size();
}
