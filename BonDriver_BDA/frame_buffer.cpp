#include <common.h>
#include <stdio.h>

#include "frame_buffer.h"

CFrameBuffer::CFrameBuffer(ULONG frameBufferSize)
{
	lpBuf = new BYTE[frameBufferSize];
	buffer_size = frameBufferSize;
	Initialize();
}

CFrameBuffer::~CFrameBuffer()
{
	SAFE_DELETE_ARRAY(lpBuf);
}

void CFrameBuffer::Initialize()
{
	writePos = 0;
	readPos = 0;

	countOverflow = 0;
	countUnderflow = 0;

	llTotalReadBytes = 0;
	llTotalWriteBytes = 0;
}

void CFrameBuffer::Write(LPBYTE p_src, UINT size)
{
	CAutoLock lock_it(&m_WriteLock);

	ULONG ulRestBytes = buffer_size - writePos;
	UINT uiWriteByte = size;

	if ((size == 0) || ((llTotalWriteBytes - llTotalReadBytes) >= buffer_size)) {
		return;
	}

	if (ulRestBytes > uiWriteByte) {
		memcpy(&lpBuf[writePos], p_src, uiWriteByte);
	}
	else if (ulRestBytes == uiWriteByte) {
		memcpy(&lpBuf[writePos], p_src, ulRestBytes);
	}
	else {
		memcpy(&lpBuf[writePos], p_src, ulRestBytes);
		memcpy(&lpBuf[0], &p_src[ulRestBytes], uiWriteByte - ulRestBytes);
	}

	writePos += uiWriteByte;
	llTotalWriteBytes += uiWriteByte;

	if (writePos >= buffer_size) {
		writePos -= buffer_size;
	}
}

void CFrameBuffer::Read(LPBYTE p_dst, UINT size)
{
	CAutoLock lock_it(&m_ReadLock);

	ULONG ulRestBytes = buffer_size - readPos;
	UINT uiReadByte = size;

	if (size == 0) {
		return;
	}

	if (ulRestBytes > uiReadByte) {
		memcpy(p_dst, &lpBuf[readPos], uiReadByte);
	}
	else if (ulRestBytes == uiReadByte) {
		memcpy(p_dst, &lpBuf[readPos], ulRestBytes);
	}
	else {
		memcpy(p_dst, &lpBuf[readPos], ulRestBytes);
		memcpy(&p_dst[ulRestBytes], &lpBuf[0], uiReadByte - ulRestBytes);
	}

	readPos += uiReadByte;
	llTotalReadBytes += uiReadByte;

	if (readPos >= buffer_size) {
		readPos -= buffer_size;
	}
}
