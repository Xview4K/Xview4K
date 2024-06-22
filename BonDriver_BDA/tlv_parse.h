#pragma once

#include "mmtp.h"
#include "frame_buffer.h"
#include "crc32.h"

#include "streams.h"

#include <string>
#include <iostream>
#include <common.h>

#include "a_cas_card.h"
#include "a_cas_card_error_code.h"

// for packet scheduler
#define PID_FIND_TLV_PACKET_HEAD 0
#define PID_GET_TLV_PACKET_SIZE 1
#define PID_READ_SEEK_PACKET 2
#define PID_PARSE_PACKET 3

// for subtitle
#define XML_SUB_BUFFER_NUM 20

typedef struct {
	UINT16 event_id;
	UINT64 start_time;
	UINT32 duration;
	INT start_time_val;
	INT duration_val;
	BOOL free_CA_mode;
	std::wstring contentTitleStr;
	std::wstring contentInfoStr;
}content_typedef;

typedef struct {
	UINT16 service_id;
	UINT16 service_id_list[4];
	INT service_count;
	INT check_count;
	INT current_idx;
	BOOL complete;
	BOOL update;
}content_check_typedef;


class CTlvParse {
public:
	CTlvParse();
	virtual ~CTlvParse();
	void Initialize();
	void InitializeSubtitle();
	HRESULT findTlvPacketHead(INT resumeNo, INT *retNo);
	HRESULT getTlvPacketSize(INT resumeNo, INT* retNo);
	HRESULT seekPacket();
	HRESULT readPacket(INT resumeNo, INT* retNo);
	HRESULT parsePacket();
	HRESULT setPacketBufPtr(LPBYTE lpBuf);
	HRESULT setPacketBufSize(size_t size);
	HRESULT parseScheduler();

	void debugPrintf(LPCWSTR pszFormat, ...);
	void debugPrintf2(LPCWSTR pszFormat, ...);
	void debugPrintf3(LPCWSTR pszFormat, ...);

	inline void swapByteOrder(unsigned short& us);
	inline void swapByteOrder(unsigned int& ui);
	inline void swapByteOrder(unsigned long long& ull);

	ULONGLONG packetTypeIPV4;
	ULONGLONG packetTypeIPV6;
	ULONGLONG packetTypeCompressed;
	ULONGLONG packetTypeControl;
	ULONGLONG packetTypeNull;
	ULONGLONG count;
	ULONGLONG error;

	ULONG ulCurSeekPos;
	BYTE bCurPacketType;
	ULONG ulTlvPacketSize;
	ULONG ulFillBuffSize;
	INT process_id;

	UINT16 uiVideo_PID;
	UINT16 uiAudio_PID;
	UINT16 uiStpp_PID;
	UINT16 uiCID;
	BOOL setup;
	BOOL scramble;
	BOOL scramble_notified;

	BYTE video_asset_type[5];
	BYTE audio_asset_type[5];

	content_typedef content[2];
	content_check_typedef content_check;

	LPBYTE lpXmlSubBuf[XML_SUB_BUFFER_NUM];
	UINT uiXmlSubBufWritePos;
	UINT uiXmlSubBufReadPos;
	ULONG ulXmlSubBufSize[XML_SUB_BUFFER_NUM];
	BOOL XmlSubUpdate;
	BOOL XmlSubUpdate_notified;

	BOOL disbale_XmlSubUpdate_notify;

	BOOL mmtp_read_notified;

	BOOL disable_scramble_notify;

	//A_CAS_CARD* acas;
	//A_CAS_ID           casid;
	//CARD_SCRAMBLEKEY caskey;
	//A_CAS_INIT_STATUS is;
	//A_CAS_ECM_RESULT res;
	uint8_t past_signaling_data_byte;

protected:
	LPBYTE lpTlvBuf = NULL;
	LPBYTE lpTlvPacketBuf = NULL;
	CCritSec m_Lock;
};

