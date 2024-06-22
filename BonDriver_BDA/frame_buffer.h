#pragma once

#include <Windows.h>
#include "streams.h"

#define VIDEO_FRAME_BUFFER_SIZE (1048576 * 30)
#define AUDIO_FRAME_BUFFER_SIZE (102400 * 3)

class CFrameBuffer
{
public:
	CFrameBuffer(ULONG frameBufferSize);
	~CFrameBuffer();

	void Initialize();
	void Write(LPBYTE p_src, UINT size);
	void Read(LPBYTE p_dst, UINT size);
	
	INT countOverflow;
	INT countUnderflow;
	LPBYTE lpBuf;
	LONGLONG buffer_size;
	LONGLONG writePos;
	LONGLONG readPos;
	LONGLONG llTotalReadBytes;
	LONGLONG llTotalWriteBytes;

protected:
	CCritSec m_WriteLock;
	CCritSec m_ReadLock;
};
