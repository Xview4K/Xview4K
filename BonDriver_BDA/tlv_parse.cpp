#pragma once

#include "tlv_parse.h"
#include "mmtp.h"

//Crypto++
#include <cryptlib.h>
#include <hex.h>
#include <filters.h>
#include <aes.h>
#include <ccm.h>
#include <vector>

#define AES_KEY_SIZE 16
#define AES_BLOCK_SIZE 16

#define NOTIFY_CONTENT_UPDATE_CODE 0xBBBB
#define SCRAMBLE_DETECT_CODE 0xDCBA
#define XML_SUB_UPDATE_CODE 0xAAAA


extern HWND m_hWnd;
extern CFrameBuffer* video_frame_buffer;
extern CFrameBuffer* audio_frame_buffer;

//ACAS
extern A_CAS_CARD* acas;
extern A_CAS_ID           casid;
extern CARD_SCRAMBLEKEY caskey;
extern A_CAS_INIT_STATUS is;
extern A_CAS_ECM_RESULT res;

void juian_date(UINT16 mjd, INT* Year, INT* Month, INT* Day);
std::wstring ReplaceString(std::wstring String1, std::wstring String2, std::wstring String3);

void CTlvParse::debugPrintf(LPCWSTR pszFormat, ...)
{
	//va_list argp;
	//WCHAR pszBuf[256];
	//va_start(argp, pszFormat);
	//vswprintf_s(pszBuf, pszFormat, argp);
	//va_end(argp);
	//OutputDebugString(pszBuf);
}

void CTlvParse::debugPrintf2(LPCWSTR pszFormat, ...)
{
	//va_list argp;
	//WCHAR pszBuf[256];
	//va_start(argp, pszFormat);
	//vswprintf_s(pszBuf, pszFormat, argp);
	//va_end(argp);
	//OutputDebugString(pszBuf);
}

void CTlvParse::debugPrintf3(LPCWSTR pszFormat, ...)
{
	//va_list argp;
	//WCHAR pszBuf[256];
	//va_start(argp, pszFormat);
	//vswprintf_s(pszBuf, pszFormat, argp);
	//va_end(argp);
	//OutputDebugString(pszBuf);
}

void debugPrintf4(LPCWSTR pszFormat, ...)
{
	va_list argp;
	WCHAR pszBuf[256];
	va_start(argp, pszFormat);
	vswprintf_s(pszBuf, pszFormat, argp);
	va_end(argp);
	OutputDebugString(pszBuf);
}

inline void CTlvParse::swapByteOrder(unsigned short& us)
{
	us = (us >> 8) |
		(us << 8);
}

inline void CTlvParse::swapByteOrder(unsigned int& ui)
{
	ui = (ui >> 24) |
		((ui << 8) & 0x00FF0000) |
		((ui >> 8) & 0x0000FF00) |
		(ui << 24);
}

inline void CTlvParse::swapByteOrder(unsigned long long& ull)
{
	ull = (ull >> 56) |
		((ull << 40) & 0x00FF000000000000) |
		((ull << 24) & 0x0000FF0000000000) |
		((ull << 8) & 0x000000FF00000000) |
		((ull >> 8) & 0x00000000FF000000) |
		((ull >> 24) & 0x0000000000FF0000) |
		((ull >> 40) & 0x000000000000FF00) |
		(ull << 56);
}


CTlvParse::CTlvParse()
{
	Initialize();

	lpTlvBuf = NULL;
	lpTlvPacketBuf = new BYTE[65536];
	for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
		lpXmlSubBuf[i] = new BYTE[65536];
	}
	res.return_code = 0;
}

CTlvParse::~CTlvParse()
{
	SAFE_DELETE_ARRAY(lpTlvPacketBuf);
	for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
		SAFE_DELETE_ARRAY(lpXmlSubBuf[i]);
	}
}

void CTlvParse::Initialize()
{
	packetTypeIPV4 = 0;
	packetTypeIPV6 = 0;
	packetTypeCompressed = 0;
	packetTypeControl = 0;
	packetTypeNull = 0;
	count = 0;
	error = 0;

	ulCurSeekPos = 0;
	bCurPacketType = 0;
	ulTlvPacketSize = 0;
	ulFillBuffSize = 0;
	process_id = 0;

	uiVideo_PID = 0;
	uiAudio_PID = 0;
	uiStpp_PID = 0;
	uiCID = 0;
	setup = FALSE;
	scramble = FALSE;
	scramble_notified = FALSE;

	memset(video_asset_type, 0, sizeof(video_asset_type));
	memset(audio_asset_type, 0, sizeof(audio_asset_type));

	content[0].contentTitleStr.clear();
	content[0].contentInfoStr.clear();
	content[0].duration = 0;
	content[0].duration_val = 0;
	content[0].event_id = 0;
	content[0].free_CA_mode = 0;
	content[0].start_time = 0;
	content[0].start_time_val = 0;

	content[1].contentTitleStr.clear();
	content[1].contentInfoStr.clear();
	content[1].duration = 0;
	content[1].duration_val = 0;
	content[1].event_id = 0;
	content[1].free_CA_mode = 0;
	content[1].start_time = 0;
	content[1].start_time_val = 0;

	for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
		ulXmlSubBufSize[i] = 0;
	}

	uiXmlSubBufWritePos = 0;
	uiXmlSubBufReadPos = 0;

	XmlSubUpdate = FALSE;
	XmlSubUpdate_notified = FALSE;

	disbale_XmlSubUpdate_notify = FALSE;

	mmtp_read_notified = FALSE;

	disable_scramble_notify = FALSE;

	memset(&content_check, 0, sizeof(content_check));
}

void CTlvParse::InitializeSubtitle()
{
	for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
		ulXmlSubBufSize[i] = 0;
	}

	uiXmlSubBufWritePos = 0;
	uiXmlSubBufReadPos = 0;

	XmlSubUpdate = FALSE;
	XmlSubUpdate_notified = FALSE;
}

HRESULT CTlvParse::findTlvPacketHead(INT resumeNo, INT* retNo)
{
	ULONG ulRestBytes;
	BYTE data[2] = { 0, 0 };

	static ULONG static_ulRestBytes;
	static BYTE static_data[2];

	*retNo = 0;

	if (resumeNo == 0) {
		ulRestBytes = ulFillBuffSize - ulCurSeekPos + 1;
	}
	else {
		ulRestBytes = static_ulRestBytes;
		data[0] = static_data[0];
		data[1] = static_data[1];
		goto RESUME_POINT;
	}

	while (ulRestBytes-- > 0) {
		data[0] = lpTlvBuf[ulCurSeekPos++];
		if (ulCurSeekPos >= ulFillBuffSize) {
			ulCurSeekPos = 0;
			*retNo = 1;
			static_ulRestBytes = ulRestBytes;
			static_data[0] = data[0];
			static_data[1] = data[1];
			return E_ABORT;
		RESUME_POINT:
			do {} while (0);
		}
		if (data[1] == 0x7F) {
			switch (data[0]) {
			case 0x01:
				++packetTypeIPV4;
				bCurPacketType = 0x01;
				return S_OK;
			case 0x02:
				++packetTypeIPV6;
				bCurPacketType = 0x02;
				return S_OK;
			case 0x03:
				++packetTypeCompressed;
				bCurPacketType = 0x03;
				return S_OK;
			case 0xFE:
				++packetTypeControl;
				bCurPacketType = 0xFE;
				return S_OK;
			case 0xFF:
				++packetTypeNull;
				bCurPacketType = 0xFF;
				return S_OK;
			default:
				if (setup) {
					++error;
				}
				break;
			}
		}
		data[1] = data[0];
	}

	if (setup) {
		++error;
	}

	return E_FAIL;
}

HRESULT CTlvParse::getTlvPacketSize(INT resumeNo, INT* retNo)
{
	BYTE data[2] = { 0, 0 };
	static BYTE static_data[2];

	*retNo = 0;

	if (resumeNo) {
		data[0] = static_data[0];
		data[1] = static_data[1];
		if (resumeNo == 1) {
			goto RESUME_POINT0;
		}
		else if (resumeNo == 2) {
			goto RESUME_POINT1;
		}
	}

	data[0] = lpTlvBuf[ulCurSeekPos++];
	if (ulCurSeekPos >= ulFillBuffSize) {
		ulCurSeekPos = 0;
		*retNo = 1;
		static_data[0] = data[0];
		static_data[1] = data[1];
		return E_ABORT;
	RESUME_POINT0:
		do {} while (0);
	}

	data[1] = lpTlvBuf[ulCurSeekPos++];
	if (ulCurSeekPos >= ulFillBuffSize) {
		ulCurSeekPos = 0;
		*retNo = 2;
		static_data[0] = data[0];
		static_data[1] = data[1];
		return E_ABORT;
	RESUME_POINT1:
		do {} while (0);
	}

	ulTlvPacketSize = (UINT16)(((UINT16)(data[0] << 8) & 0xFF00) | (UINT16)(data[1] & 0xFF));

	//if ((bCurPacketType != 0xFF) && (ulTlvPacketSize > 1500)) {
	//	return E_FAIL;
	//}

	//if (ulTlvPacketSize > 4297) {
	//	return E_FAIL;
	//}

	return S_OK;
}

HRESULT CTlvParse::seekPacket()
{
	++count;
	ulCurSeekPos += ulTlvPacketSize;

	if (ulCurSeekPos >= ulFillBuffSize) {
		ulCurSeekPos -= ulFillBuffSize;
		return E_ABORT;
	}

	return S_OK;
}

HRESULT CTlvParse::readPacket(INT resumeNo, INT* retNo)
{
	ULONG ulRestBytes;
	UINT uiReadByte;

	static ULONG static_ulRestBytes;
	static UINT static_uiReadByte;

	*retNo = 0;

	if (resumeNo == 0) {
		ulRestBytes = ulFillBuffSize - ulCurSeekPos;
		uiReadByte = ulTlvPacketSize;
	}
	else {
		ulRestBytes = static_ulRestBytes;
		uiReadByte = static_uiReadByte;
		if (resumeNo == 1) {
			goto RESUME_POINT0;
		}
		else if (resumeNo == 2) {
			goto RESUME_POINT1;
		}
	}

	if (ulRestBytes > uiReadByte) {
		memcpy(lpTlvPacketBuf, &lpTlvBuf[ulCurSeekPos], uiReadByte);
	}
	else if (ulRestBytes == uiReadByte) {
		memcpy(lpTlvPacketBuf, &lpTlvBuf[ulCurSeekPos], ulRestBytes);
		*retNo = 1;
		static_ulRestBytes = ulRestBytes;
		static_uiReadByte = uiReadByte;
		return E_ABORT;
	RESUME_POINT0:
		do {} while (0);
	}
	else {
		memcpy(lpTlvPacketBuf, &lpTlvBuf[ulCurSeekPos], ulRestBytes);
		*retNo = 2;
		static_ulRestBytes = ulRestBytes;
		static_uiReadByte = uiReadByte;
		return E_ABORT;
	RESUME_POINT1:
		memcpy(&lpTlvPacketBuf[ulRestBytes], &lpTlvBuf[0], uiReadByte - ulRestBytes);
	}

	ulCurSeekPos += uiReadByte;

	if (ulCurSeekPos >= ulFillBuffSize) {
		ulCurSeekPos -= ulFillBuffSize;
	}

	++count;

	return S_OK;
}

HRESULT CTlvParse::setPacketBufPtr(LPBYTE lpBuf)
{
	this->lpTlvBuf = lpBuf;
	return S_OK;
}

HRESULT CTlvParse::setPacketBufSize(size_t size)
{
	this->ulFillBuffSize = size;
	return S_OK;
}

HRESULT CTlvParse::parseScheduler()
{
	HRESULT hr;
	INT resumeNo;
	static INT retNo = 0;
	ULONGLONG tick = GetTickCount64();

	while (1) {
		switch (process_id) {
		case PID_FIND_TLV_PACKET_HEAD: // findTlvPacketHead
			resumeNo = retNo;
			hr = findTlvPacketHead(resumeNo, &retNo);
			if (hr == S_OK) {
				retNo = 0;
				process_id = PID_GET_TLV_PACKET_SIZE;
			}
			else if (hr == E_ABORT) {
				process_id = PID_FIND_TLV_PACKET_HEAD;
				return S_OK;
			}
			else {
				retNo = 0;
				process_id = PID_FIND_TLV_PACKET_HEAD;
			}
			break;
		case PID_GET_TLV_PACKET_SIZE: //getTlvPacketSize
			resumeNo = retNo;
			hr = getTlvPacketSize(resumeNo, &retNo);
			if (hr == S_OK) {
				retNo = 0;
				process_id = PID_READ_SEEK_PACKET;
			}
			else if (hr == E_ABORT) {
				process_id = PID_GET_TLV_PACKET_SIZE;
				return S_OK;
			}
			else if (hr == E_FAIL) {
				retNo = 0;
				process_id = PID_FIND_TLV_PACKET_HEAD;
				++error;
				continue;
			}
			break;
		case PID_READ_SEEK_PACKET: // readPacket seekPacket
//			if (bCurPacketType == 0x03 || (bCurPacketType == 0xFE)) {
			if (bCurPacketType == 0x03) {
				resumeNo = retNo;
				hr = readPacket(resumeNo, &retNo);
				if (hr == S_OK) {
					retNo = 0;
					process_id = PID_PARSE_PACKET;
				}
				else if (hr == E_ABORT) {
					process_id = PID_READ_SEEK_PACKET;
					return S_OK;
				}
			}
			else {
				hr = seekPacket();
				if (hr == S_OK) {
					retNo = 0;
					process_id = PID_FIND_TLV_PACKET_HEAD;
				}
				else if (hr == E_ABORT) {
					retNo = 0;
					process_id = PID_FIND_TLV_PACKET_HEAD;
					return S_OK;
				}
			}
			break;
		case PID_PARSE_PACKET: // parsePacket
			hr = parsePacket();
			retNo = 0;
			process_id = PID_FIND_TLV_PACKET_HEAD;
			if (content_check.update) { // 新しい番組情報を取得したかチェックし通知する
				PostMessage(m_hWnd, WM_NOTIFY, NOTIFY_CONTENT_UPDATE_CODE, 0);
				content_check.update = FALSE;
			}
			if ((disable_scramble_notify == FALSE) && (scramble_notified == FALSE) && (setup == TRUE) && (scramble == TRUE) && (content[0].event_id != 0) && (content[1].event_id != 0)) { // スクランブル放送検出通知
				PostMessage(m_hWnd, WM_NOTIFY, SCRAMBLE_DETECT_CODE, 0);
				scramble_notified = TRUE;
			}
			if ((disbale_XmlSubUpdate_notify == FALSE) && (XmlSubUpdate_notified == FALSE) && (XmlSubUpdate == TRUE)) { // 字幕通知
				PostMessage(m_hWnd, WM_NOTIFY, XML_SUB_UPDATE_CODE, 0);
				XmlSubUpdate_notified = TRUE;
			}
			break;
		default:
			break;
		}

		if ((GetTickCount64() - tick) >= 1000) { // Timeout 1sec
			return E_FAIL;
		}

	}

	return S_OK;
}

HRESULT CTlvParse::parsePacket()
{
	UINT32 packet_counter = 0;
	UINT pos = 0;
	//	WCHAR szWork[256];

	//debugPrintf(L"\n");

	//	debugPrintf2(L"packet_type:0x%02X\n", bCurPacketType);

	if ((bCurPacketType != 0x03) && (bCurPacketType != 0xFE)) {
		return E_ABORT;
	}

	if (bCurPacketType == 0xFE) {
		Signaling_packet_typedef* sig_packet = (Signaling_packet_typedef*)lpTlvPacketBuf;
		swapByteOrder(sig_packet->ushort0);
		swapByteOrder(sig_packet->table_id_extension);
		//		printf("FE size:%d tbl_id:%02X tbl_id_ext:%04X\n", ulTlvPacketSize, sig_packet->table_id, sig_packet->table_id_extension);
		return S_OK;
	}


	compressed_ip_packet_typedef* cip = (compressed_ip_packet_typedef*)lpTlvPacketBuf;
	swapByteOrder(cip->ushort0);
	if ((pos += sizeof(compressed_ip_packet_typedef)) >= ulTlvPacketSize) {
		return S_OK;
	}

	if (cip->CID >= 5) {
		return E_ABORT;
	}

	//debugPrintf(L"*CID cid:0x%03X sn:%d CID_hType:0x%02X\n", cip->CID, cip->SN, cip->CID_header_type);

	if ((cip->CID_header_type != 0x60) && (cip->CID_header_type != 0x61)) {
		return E_ABORT;
	}

	if (cip->CID_header_type == 0x60) { // IPv6 Full Header
		IPv6_header_wo_length_typedef* ipv6 = (IPv6_header_wo_length_typedef*)(lpTlvPacketBuf + pos);
		swapByteOrder(ipv6->ulong0);
		if ((pos += sizeof(IPv6_header_wo_length_typedef) + sizeof(UDP_header_wo_length_typedef)) >= ulTlvPacketSize) {
			//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
			return E_FAIL;
		}
		//debugPrintf2(L"*IPV6 version:%d trafiic_class:%d flow_label:%d next_header:%d hop_limit:%d\n", ipv6->version, ipv6->trafiic_class, ipv6->flow_label, ipv6->next_header, ipv6->hop_limit);
	}

	// IPv6 Compressed Header
	MMTP_packet_typedef* mmtp = (MMTP_packet_typedef*)(lpTlvPacketBuf + pos);
	swapByteOrder(mmtp->packet_id);
	swapByteOrder(mmtp->timestamp);
	swapByteOrder(mmtp->packet_sequence_number);
	if ((pos += sizeof(MMTP_packet_typedef)) >= ulTlvPacketSize) {
		//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
		return E_FAIL;
	}
	//debugPrintf2(L"*MMTP ver:%d pcFlag:%d FEC_Type:%d extension_flag:%d RAP_flag:%d payload_type:0x%02X PID:0x%04X timestamp:0x%08X psn:0x%08X\n", mmtp->version, mmtp->packet_counter_flag, mmtp->FEC_type, mmtp->extension_flag, mmtp->RAP_flag, mmtp->payload_type, mmtp->packet_id, mmtp->timestamp, mmtp->packet_sequence_number);

	if (mmtp->packet_counter_flag) {
		packet_counter = *(UINT32*)(lpTlvPacketBuf + pos);
		swapByteOrder(packet_counter);
		if ((pos += sizeof(UINT32)) >= ulTlvPacketSize) {
			//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
			return E_FAIL;
		}
	}

	int Scramble_control = 0;
	extension_typedef* ext;
	header_extension_typedef* header_ext;

	if (mmtp->extension_flag) {
		ext = (extension_typedef*)(lpTlvPacketBuf + pos);
		swapByteOrder(ext->extension_type);
		swapByteOrder(ext->extension_length);
		if ((pos += sizeof(extension_typedef)) >= ulTlvPacketSize) {
			//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
			return E_FAIL;
		}
		if (ext->extension_length) {
			header_ext = (header_extension_typedef*)(lpTlvPacketBuf + pos);
			swapByteOrder(header_ext->extension_type);
			swapByteOrder(header_ext->extension_length);
			if ((pos += sizeof(header_extension_typedef) + header_ext->extension_length - 1) >= ulTlvPacketSize) {
				//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
				return E_FAIL;
			}
			 if ((header_ext->extension_type & 0x7FFF) == 0x0001) {
				 Scramble_control = header_ext->MMT_scramble_control;
//				OutputDebug(L"init:%d ext_length:%d auth:%d ident:%d\n", header_ext->MMT_scramble_init_control, header_ext->extension_length, header_ext->message_auth_control, header_ext->scramble_ident_control);
				if (header_ext->MMT_scramble_control == 2) {
					if (cip->CID == uiCID) {
						//scramble = TRUE;
						//return E_ABORT;
					}
					//					printf("Scramble Even sic:%d mac:%d msic:%d\n", header_ext->scramble_ident_control, header_ext->message_auth_control, header_ext->MMT_scramble_init_control);
				}
				else if (header_ext->MMT_scramble_control == 3) {
					if (cip->CID == uiCID) {
						//scramble = TRUE;
						//return E_ABORT;
					}
					//					printf("Scramble Odd  sic:%d mac:%d msic:%d\n", header_ext->scramble_ident_control, header_ext->message_auth_control, header_ext->MMT_scramble_init_control);
				}
			}
		}

		//debugPrintf(L"*EXT ext_type:%d ext_length:%d\n", ext->extension_type, ext->extension_length);
	}

	if ((mmtp->payload_type == 0)) {  // MPU

		if (setup == 0) {
			return E_ABORT;
		}

		if (cip->CID != uiCID) {
			return E_ABORT;
		}

		if ((mmtp->packet_id != uiVideo_PID) && (mmtp->packet_id != uiAudio_PID) && (mmtp->packet_id != uiStpp_PID)) {
			return E_ABORT;
		}

		if (scramble == TRUE) {
			return E_ABORT;
		}

		MMTP_payload_typedef* mmtp_payload = (MMTP_payload_typedef*)(lpTlvPacketBuf + pos);
		swapByteOrder(mmtp_payload->payload_length);
		swapByteOrder(mmtp_payload->MPU_sequence_number);
		if ((pos += sizeof(MMTP_payload_typedef)) >= ulTlvPacketSize) {
			//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
			return E_FAIL;
		}


		//debugPrintf(L"*MPU payload_length:%d frag_type:%d timed_flag:%d frag_idc:%d aggregate_flag:%d frag_counter:%d MPU_seq_num:0x%04X\n", mmtp_payload->payload_length, mmtp_payload->fragment_type, mmtp_payload->timed_flag, mmtp_payload->fragmentation_indicator, mmtp_payload->aggregation_flag, mmtp_payload->fragment_counter, mmtp_payload->MPU_sequence_number);
		static UINT32 total = 0;

		if (mmtp_payload->fragment_type == 2) { // MFU
			uint8_t* MFU_data_byte;
			UINT16 data_length_enc;
			if (mmtp_payload->timed_flag == 1) { // aggregation == 0 or 1
				if (mmtp_payload->aggregation_flag == 0) { // aggregation == 0

					//if encrypted
					if (res.return_code == 1 && Scramble_control > 0) //=1,return_code=0 if no ECM
					{
						MFU_data_byte = (lpTlvPacketBuf + pos);
						data_length_enc = mmtp_payload->payload_length;
						//swapByteOrder(data_length);//swapByteOrder(mmtp_payload->payload_length);
						data_length_enc = data_length_enc - sizeof(MMTP_payload_typedef) + sizeof(UINT16);
						uint8_t iv[AES_KEY_SIZE] = { 0x00 };
						int i, minsize = 0x1000;//minsize to avoid memory error in memcpy
						uint8_t key[AES_KEY_SIZE];
						std::vector<uint8_t> in(minsize);
						std::vector<uint8_t> out(minsize);

						for (i = 0; i < 2; i++) {
							iv[i] = (mmtp->packet_id >> (8 - i * 8)) & 0xFF;
						}
						for (i = 2; i < 2 + 4; i++) {
							iv[i] = (mmtp->packet_sequence_number >> (24 - (i - 2) * 8)) & 0xFF;
						}
						if ((Scramble_control & 1) == 0)//even
						{
							for (i = AES_KEY_SIZE; i < 2 * AES_KEY_SIZE; i++) {
								key[i - AES_KEY_SIZE] = res.scramble_key[i];
							}
						}
						else//odd
						{
							for (i = 0; i < AES_KEY_SIZE; i++) {
								key[i] = res.scramble_key[i];
							}
						}
						//uint8_t* wp;
						//wp = (lpTlvPacketBuf + pos);


						//swapByteOrder(data_length);
						CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryption;//AES or Camellia
						decryption.SetKeyWithIV(key, AES_KEY_SIZE, iv);
						memcpy(in.data(), MFU_data_byte, data_length_enc);
						size_t size = data_length_enc;
						decryption.ProcessData(out.data(), in.data(), size);
						if (out[0] != 0x00) {
							//printf("Decryption error, Re-insert IC card. packet#:0x%x\n", mmtp->packet_sequence_number);
							//scramble = TRUE;
							//return E_ABORT;
						}
						else {//recover original data and write decrypted out.data()
							memcpy(lpTlvPacketBuf + pos, out.data(), data_length_enc);
							header_ext->MMT_scramble_control = 0;
						}
					}
					MFU_payload_typedef* mfu = (MFU_payload_typedef*)(lpTlvPacketBuf + pos);
					swapByteOrder(mfu->movie_fragment_sequence_number);
					swapByteOrder(mfu->sample_number);
					swapByteOrder(mfu->offset);
					if ((pos += sizeof(MFU_payload_typedef)) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}

					//debugPrintf(L"*MFU movie_frag_seq_no:0x%08X sample_no:0x%08X offset:0x%08X priority:%d dep_count:%d\n", mfu->movie_fragment_sequence_number, mfu->sample_number, mfu->offset, mfu->priority, mfu->dependency_counter);

					UINT16 data_length = mmtp_payload->payload_length - (sizeof(MFU_payload_typedef) + sizeof(MMTP_payload_typedef) - sizeof(UINT16));

					if ((mmtp_payload->fragmentation_indicator == 0) || (mmtp_payload->fragmentation_indicator == 1)) {
						if (mmtp->packet_id == uiVideo_PID) {
							//OutputDebug(L"V1:");
							//for (int i = 0; i < 16; i++) {
							//	OutputDebug(L"%02X ", *(LPBYTE)(lpTlvPacketBuf + pos + i));
							//}
							//OutputDebug(L"\n");

							LPBYTE wp = (LPBYTE)(lpTlvPacketBuf + pos);
							wp[0] = 0, wp[1] = 0, wp[2] = 0, wp[3] = 1;
							video_frame_buffer->Write(wp, data_length);
							return S_OK;
						}
						else if (mmtp->packet_id == uiAudio_PID) {
							BYTE header[3];
							header[0] = 0x56;
							header[1] = ((data_length >> 8) & 0x1F) | 0xE0;
							header[2] = data_length & 0xFF;
							LPBYTE wp = LPBYTE(lpTlvPacketBuf + pos);
							audio_frame_buffer->Write(header, 3);
							audio_frame_buffer->Write(wp, data_length);
							return S_OK;
						}
						else if ((disbale_XmlSubUpdate_notify == FALSE) && (mmtp->packet_id == uiStpp_PID)) {
							XmlSubUpdate_notified = FALSE;
							memcpy(lpXmlSubBuf[uiXmlSubBufWritePos], (lpTlvPacketBuf + pos), data_length);
							ulXmlSubBufSize[uiXmlSubBufWritePos] = data_length;

							if (mmtp_payload->fragmentation_indicator == 0) {
								swapByteOrder(*(UINT16*)lpXmlSubBuf[uiXmlSubBufWritePos]); // sequence number
								swapByteOrder(*(UINT16*)&lpXmlSubBuf[uiXmlSubBufWritePos][5]); // packet size
								if ((ulXmlSubBufSize[uiXmlSubBufWritePos] - 7) == (*(UINT16*)&lpXmlSubBuf[uiXmlSubBufWritePos][5])) { // xml data length == packet size ?
									if (++uiXmlSubBufWritePos >= XML_SUB_BUFFER_NUM) {
										uiXmlSubBufWritePos = 0;
									}
									XmlSubUpdate = TRUE;
								}
							}

							return S_OK;
						}
					}
					else {
						if (mmtp->packet_id == uiVideo_PID) {
							//OutputDebug(L"V2:");
							//for (int i = 0; i < 16; i++) {
							//	OutputDebug(L"%02X ", *(LPBYTE)(lpTlvPacketBuf + pos + i));
							//}
							//OutputDebug(L"\n");

							LPBYTE wp = LPBYTE(lpTlvPacketBuf + pos);
							video_frame_buffer->Write(wp, data_length);
							return S_OK;
						}
						else if ((disbale_XmlSubUpdate_notify == FALSE) && (mmtp->packet_id == uiStpp_PID)) {
							memcpy(&lpXmlSubBuf[uiXmlSubBufWritePos][ulXmlSubBufSize[uiXmlSubBufWritePos]], (lpTlvPacketBuf + pos), data_length);
							ulXmlSubBufSize[uiXmlSubBufWritePos] += data_length;

							if (mmtp_payload->fragmentation_indicator == 3) {
								swapByteOrder(*(UINT16*)lpXmlSubBuf[uiXmlSubBufWritePos]); // sequence number
								swapByteOrder(*(UINT16*)&lpXmlSubBuf[uiXmlSubBufWritePos][5]); // packet size
								if ((ulXmlSubBufSize[uiXmlSubBufWritePos] - 7) == (*(UINT16*)&lpXmlSubBuf[uiXmlSubBufWritePos][5])) { // xml data length == packet size ?
									if (++uiXmlSubBufWritePos >= XML_SUB_BUFFER_NUM) {
										uiXmlSubBufWritePos = 0;
									}
									XmlSubUpdate = TRUE;
								}
							}

							return S_OK;
						}

					}
				}
				else {  // aggregation == 1
					//if encrypted
					if (res.return_code == 1 && Scramble_control > 0) //=1,return_code=0 if no ECM
					{
						MFU_data_byte = (lpTlvPacketBuf + pos);
						data_length_enc = mmtp_payload->payload_length;
						//swapByteOrder(data_length);//swapByteOrder(mmtp_payload->payload_length);
						data_length_enc = data_length_enc - sizeof(MMTP_payload_typedef) + sizeof(UINT16);
						uint8_t iv[AES_KEY_SIZE] = { 0x00 };
						int i, minsize = 0x1000;//minsize to avoid memory error in memcpy
						uint8_t key[AES_KEY_SIZE];
						std::vector<uint8_t> in(minsize);
						std::vector<uint8_t> out(minsize);

						for (i = 0; i < 2; i++) {
							iv[i] = (mmtp->packet_id >> (8 - i * 8)) & 0xFF;
						}
						for (i = 2; i < 2 + 4; i++) {
							iv[i] = (mmtp->packet_sequence_number >> (24 - (i - 2) * 8)) & 0xFF;
						}
						if ((Scramble_control & 1) == 0)//even
						{
							for (i = AES_KEY_SIZE; i < 2 * AES_KEY_SIZE; i++) {
								key[i - AES_KEY_SIZE] = res.scramble_key[i];
							}
						}
						else//odd
						{
							for (i = 0; i < AES_KEY_SIZE; i++) {
								key[i] = res.scramble_key[i];
							}
						}
						//uint8_t* wp;
						//wp = (lpTlvPacketBuf + pos);


						//swapByteOrder(data_length);
						CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryption;//AES or Camellia
						decryption.SetKeyWithIV(key, AES_KEY_SIZE, iv);
						memcpy(in.data(), MFU_data_byte, data_length_enc);
						size_t size = data_length_enc;
						decryption.ProcessData(out.data(), in.data(), size);
						if (out[0] != 0x00) {
							//printf("Decryption error, Re-insert IC card. packet#:0x%x\n", mmtp->packet_sequence_number);
							//scramble = TRUE;
							//return E_ABORT;
						}
						else {//recover original data and write decrypted out.data()
							memcpy(lpTlvPacketBuf + pos, out.data(), data_length_enc);
							header_ext->MMT_scramble_control = 0;
						}
					}

					INT16 payload_length_count = mmtp_payload->payload_length - (sizeof(MMTP_payload_typedef) - sizeof(UINT16));
					do {
						MFU_AGGREGATE_payload_typedef* mfu = (MFU_AGGREGATE_payload_typedef*)(lpTlvPacketBuf + pos);
						swapByteOrder(mfu->data_unit_length);
						swapByteOrder(mfu->movie_fragment_sequence_number);
						swapByteOrder(mfu->sample_number);
						swapByteOrder(mfu->offset);
						if ((pos += sizeof(MFU_AGGREGATE_payload_typedef)) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}


						//debugPrintf(L"*MFU du_length:%d movie_frag_seq_no:0x%08X sample_no:0x%08X offset:0x%08X priority:%d dep_count:%d\n", mfu->data_unit_length, mfu->movie_fragment_sequence_number, mfu->sample_number, mfu->offset, mfu->priority, mfu->dependency_counter);

						UINT16 data_length = mfu->data_unit_length - sizeof(MFU_AGGREGATE_payload_typedef) + sizeof(UINT16);
						//debugPrintf2(L"*dl:%lu\n", data_length);
						//debugPrintf(L"count:%d\n", payload_length_count);

						if ((mmtp_payload->fragmentation_indicator == 0) || (mmtp_payload->fragmentation_indicator == 1)) {
							if (mmtp->packet_id == uiVideo_PID) {
								//OutputDebug(L"V3:");
								//for (int i = 0; i < 16; i++) {
								//	OutputDebug(L"%02X ", *(LPBYTE)(lpTlvPacketBuf + pos + i));
								//}
								//OutputDebug(L"\n");

								LPBYTE wp = (LPBYTE)(lpTlvPacketBuf + pos);
								wp[0] = 0, wp[1] = 0, wp[2] = 0, wp[3] = 1;
								video_frame_buffer->Write(wp, data_length);
							}
						}
						if ((pos += mfu->data_unit_length - sizeof(MFU_AGGREGATE_payload_typedef) + sizeof(UINT16)) >= ulTlvPacketSize) {
							return S_OK;
						}

						payload_length_count -= mfu->data_unit_length + sizeof(UINT16);
					} while (payload_length_count > 0);
					return S_OK;
				}
			}





			else { // aggregation == 0 or 1
				if (mmtp_payload->aggregation_flag == 0) { // aggregation == 0

					//if encrypted
					if (res.return_code == 1 && Scramble_control > 0) //=1,return_code=0 if no ECM
					{
						MFU_data_byte = (lpTlvPacketBuf + pos);
						data_length_enc = mmtp_payload->payload_length;
						//swapByteOrder(data_length);//swapByteOrder(mmtp_payload->payload_length);
						data_length_enc = data_length_enc - sizeof(MMTP_payload_typedef) + sizeof(UINT16);
						uint8_t iv[AES_KEY_SIZE] = { 0x00 };
						int i, minsize = 0x1000;//minsize to avoid memory error in memcpy
						uint8_t key[AES_KEY_SIZE];
						std::vector<uint8_t> in(minsize);
						std::vector<uint8_t> out(minsize);

						for (i = 0; i < 2; i++) {
							iv[i] = (mmtp->packet_id >> (8 - i * 8)) & 0xFF;
						}
						for (i = 2; i < 2 + 4; i++) {
							iv[i] = (mmtp->packet_sequence_number >> (24 - (i - 2) * 8)) & 0xFF;
						}
						if ((Scramble_control & 1) == 0)//even
						{
							for (i = AES_KEY_SIZE; i < 2 * AES_KEY_SIZE; i++) {
								key[i - AES_KEY_SIZE] = res.scramble_key[i];
							}
						}
						else//odd
						{
							for (i = 0; i < AES_KEY_SIZE; i++) {
								key[i] = res.scramble_key[i];
							}
						}
						//uint8_t* wp;
						//wp = (lpTlvPacketBuf + pos);


						//swapByteOrder(data_length);
						CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryption;//AES or Camellia
						decryption.SetKeyWithIV(key, AES_KEY_SIZE, iv);
						memcpy(in.data(), MFU_data_byte, data_length_enc);
						size_t size = data_length_enc;
						decryption.ProcessData(out.data(), in.data(), size);
						if (out[0] != 0x00) {
							//printf("Decryption error, Re-insert IC card. packet#:0x%x\n", mmtp->packet_sequence_number);
							//scramble = TRUE;
							//return E_ABORT;
						}
						else {//recover original data and write decrypted out.data()
							memcpy(lpTlvPacketBuf + pos, out.data(), data_length_enc);
							header_ext->MMT_scramble_control = 0;
						}
					}
					MFU_Nontimed_payload_typedef* mfu = (MFU_Nontimed_payload_typedef*)(lpTlvPacketBuf + pos);
					swapByteOrder(mfu->item_id);
					if ((pos += sizeof(MFU_Nontimed_payload_typedef)) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}

				}
				else {  // aggregation == 1
					//if encrypted
					if (res.return_code == 1 && Scramble_control > 0) //=1,return_code=0 if no ECM
					{
						MFU_data_byte = (lpTlvPacketBuf + pos);
						data_length_enc = mmtp_payload->payload_length;
						//swapByteOrder(data_length);//swapByteOrder(mmtp_payload->payload_length);
						data_length_enc = data_length_enc - sizeof(MMTP_payload_typedef) + sizeof(UINT16);
						uint8_t iv[AES_KEY_SIZE] = { 0x00 };
						int i, minsize = 0x1000;//minsize to avoid memory error in memcpy
						uint8_t key[AES_KEY_SIZE];
						std::vector<uint8_t> in(minsize);
						std::vector<uint8_t> out(minsize);

						for (i = 0; i < 2; i++) {
							iv[i] = (mmtp->packet_id >> (8 - i * 8)) & 0xFF;
						}
						for (i = 2; i < 2 + 4; i++) {
							iv[i] = (mmtp->packet_sequence_number >> (24 - (i - 2) * 8)) & 0xFF;
						}
						if ((Scramble_control & 1) == 0)//even
						{
							for (i = AES_KEY_SIZE; i < 2 * AES_KEY_SIZE; i++) {
								key[i - AES_KEY_SIZE] = res.scramble_key[i];
							}
						}
						else//odd
						{
							for (i = 0; i < AES_KEY_SIZE; i++) {
								key[i] = res.scramble_key[i];
							}
						}
						//uint8_t* wp;
						//wp = (lpTlvPacketBuf + pos);


						//swapByteOrder(data_length);
						CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryption;//AES or Camellia
						decryption.SetKeyWithIV(key, AES_KEY_SIZE, iv);
						memcpy(in.data(), MFU_data_byte, data_length_enc);
						size_t size = data_length_enc;
						decryption.ProcessData(out.data(), in.data(), size);
						if (out[0] != 0x00) {
							//printf("Decryption error, Re-insert IC card. packet#:0x%x\n", mmtp->packet_sequence_number);
							//scramble = TRUE;
							//return E_ABORT;
						}
						else {//recover original data and write decrypted out.data()
							memcpy(lpTlvPacketBuf + pos, out.data(), data_length_enc);
							header_ext->MMT_scramble_control = 0;
						}
					}

					INT16 payload_length_count = mmtp_payload->payload_length - (sizeof(MMTP_payload_typedef) - sizeof(UINT16));
					do {
						MFU_Nontimed_AGGREGATE_payload_typedef* mfu = (MFU_Nontimed_AGGREGATE_payload_typedef*)(lpTlvPacketBuf + pos);
						swapByteOrder(mfu->data_unit_length);
						if ((pos += sizeof(MFU_Nontimed_AGGREGATE_payload_typedef)) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}


						//debugPrintf(L"*MFU du_length:%d movie_frag_seq_no:0x%08X sample_no:0x%08X offset:0x%08X priority:%d dep_count:%d\n", mfu->data_unit_length, mfu->movie_fragment_sequence_number, mfu->sample_number, mfu->offset, mfu->priority, mfu->dependency_counter);

						UINT16 data_length = mfu->data_unit_length - sizeof(MFU_Nontimed_AGGREGATE_payload_typedef) + sizeof(UINT16);
						//debugPrintf2(L"*dl:%lu\n", data_length);
						//debugPrintf(L"count:%d\n", payload_length_count);

						if ((mmtp_payload->fragmentation_indicator == 0) || (mmtp_payload->fragmentation_indicator == 1)) {
							if (mmtp->packet_id == uiVideo_PID) {
								//OutputDebug(L"V3:");
								//for (int i = 0; i < 16; i++) {
								//	OutputDebug(L"%02X ", *(LPBYTE)(lpTlvPacketBuf + pos + i));
								//}
								//OutputDebug(L"\n");

								LPBYTE wp = (LPBYTE)(lpTlvPacketBuf + pos);
								wp[0] = 0, wp[1] = 0, wp[2] = 0, wp[3] = 1;
								video_frame_buffer->Write(wp, data_length);
							}
						}
						if ((pos += mfu->data_unit_length - sizeof(MFU_Nontimed_AGGREGATE_payload_typedef) + sizeof(UINT16)) >= ulTlvPacketSize) {
							return S_OK;
						}

						payload_length_count -= mfu->data_unit_length + sizeof(UINT16);
					} while (payload_length_count > 0);
					return S_OK;
				}
			}








		}
	}
	else if (mmtp->payload_type == 0x02) { // Signalling message
		SIGNALLING_message_typedef* sig = (SIGNALLING_message_typedef*)(lpTlvPacketBuf + pos);
		if ((pos += sizeof(SIGNALLING_message_typedef)) >= ulTlvPacketSize) {
			//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
			return E_FAIL;
		}
		//debugPrintf3(L"PID:0x%04X size:%lu pos:%d\n", mmtp->packet_id, ulTlvPacketSize, pos);
		//debugPrintf3(L"*SIG frag_idc:%d len_extising:%d aggregate_flag:%d fragment_counter:%d\n", sig->fragmentation_indicator, sig->length_extension_flag, sig->aggregation_flag, sig->fragment_counter);

		if (sig->aggregation_flag != 0) {
			MessageBox(NULL, TEXT("Not implemented yet: sig.aggregation_flag != 0"), TEXT("Error"), MB_OK);
			return E_NOTIMPL;
		}

		UINT16 message_id = *(UINT16*)(lpTlvPacketBuf + pos);
		swapByteOrder(message_id);
		if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
			//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
			return E_FAIL;
		}
		if (message_id == 0x0000) { // PA message table
			PA_message_typedef* PA_msg = (PA_message_typedef*)(lpTlvPacketBuf + pos);
			swapByteOrder(PA_msg->length);
			if ((pos += sizeof(PA_message_typedef)) >= ulTlvPacketSize) {
				//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
				return E_FAIL;
			}

			//debugPrintf3(L"*PA message_id:0x%04X version:%d length:%lu number_of_tables:%d\n", message_id, PA_msg->version, PA_msg->length, PA_msg->number_of_tables);

			if (PA_msg->number_of_tables > 0) {
				if ((pos += PA_msg->number_of_tables * 32) >= ulTlvPacketSize) {
					//					OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
					return S_OK;
				}

				//printf("not:%d\n", PA_msg->number_of_tables);
				//for (int i = 0; i < PA_msg->number_of_tables; i++) {
				//	PATable_typedef* pa_tbl = (PATable_typedef*)(pos + lpTlvPacketBuf);
				//	swapByteOrder(pa_tbl->length);
				//	printf("table_id:%02X length:%d\n", pa_tbl->table_id, pa_tbl->length);
				//	if ((pos += sizeof(PATable_typedef)) >= ulTlvPacketSize) {
				//		return S_OK;
				//	}
				//}
			}

			BYTE table_id = *(BYTE*)(lpTlvPacketBuf + pos);
			if (table_id == 0x20) { // MPT
				MPTable_typedef* mpt = (MPTable_typedef*)(lpTlvPacketBuf + pos);
				swapByteOrder(mpt->length);
				UINT16 sid = *(UINT16*)(lpTlvPacketBuf + pos + sizeof(MPTable_typedef));
				swapByteOrder(sid);
				if ((pos += sizeof(MPTable_typedef) + mpt->MMT_package_id_length) >= ulTlvPacketSize) {
					//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
					return E_FAIL;
				}
				//debugPrintf3(L"*MPTable table_id:0x%02X version:%d length:%d MPT_mode:%d MMT_package_id_length:%d\n", mpt->table_id, mpt->version, mpt->length, mpt->MPT_mode, mpt->MMT_package_id_length);

				//printf("packet_id_len:%d\n", mpt->MMT_package_id_length);
				//printf("mpt:%02X\n", mpt->table_id);

				UINT16 MPT_description_length = *(UINT16*)(lpTlvPacketBuf + pos);
				swapByteOrder(MPT_description_length);
				//			printf("->MPTdesctiption_length:%d\n", MPT_description_length);
				if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
					//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
					return E_FAIL;
				}
				if (MPT_description_length > 0) {
					if ((pos += MPT_description_length) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					/*
									int description_length = MPT_description_length;
									do {
										MMTSI_typedef_struct* mmtsi = (MMTSI_typedef_struct*)(lpTlvPacketBuf + pos);
										swapByteOrder(mmtsi->descriptor_tag);
										printf("des_typ:%04X len:%d\n", mmtsi->descriptor_tag, mmtsi->descriptor_length);
										OutputDebug(L"des_typ:%04X len:%d\n", mmtsi->descriptor_tag, mmtsi->descriptor_length);
										if (mmtsi->descriptor_tag == 0x8005) {
											MessageBox(NULL, L"8005", L"8005", MB_OK);
										}
										if (mmtsi->descriptor_tag == 0x8004) { // Access Control Descriptor
											if ((pos += sizeof(MMTSI_typedef_struct)) >= ulTlvPacketSize) {
												return S_OK;
											}
											UINT16 CA_system_ID = *(UINT16*)(pos + lpTlvPacketBuf);
											swapByteOrder(CA_system_ID);
											//printf("CA_sys_ID:%04X\n", CA_system_ID);
											if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
												return S_OK;
											}
											BYTE location_type = *(BYTE*)(pos + lpTlvPacketBuf);
											//printf("location:%d\n", location_type);
											if ((pos += sizeof(BYTE)) >= ulTlvPacketSize) {
												return S_OK;
											}
											if (location_type == 0) {
												UINT16 packet_id = *(UINT16*)(pos + lpTlvPacketBuf);
												swapByteOrder(packet_id);
												//							printf("packet_id:%04X\n", packet_id);
												if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
													return S_OK;
												}
											}
											else {
												return S_OK;
											}

										}
										else {
											if ((pos += sizeof(MMTSI_typedef_struct) + mmtsi->descriptor_length) >= ulTlvPacketSize) {
												return S_OK;
											}
										}
										description_length -= sizeof(MMTSI_typedef_struct) + mmtsi->descriptor_length;
									} while (description_length > 0);
									*/
				}

				//debugPrintf3(L"->MPTdesctiption_length:%d\n", MPT_description_length);

				//if (setup) {
				//	return E_ABORT;
				//}

				BYTE number_of_assets = *(BYTE*)(lpTlvPacketBuf + pos);
				if ((pos += sizeof(BYTE)) >= ulTlvPacketSize) {
					//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
					return E_FAIL;
				}
				//debugPrintf3(L"->number_of_assets:%d\n", number_of_assets);

				for (int i = 0; i < number_of_assets; i++) {
					Asset_typedef* asset = (Asset_typedef*)(lpTlvPacketBuf + pos);
					swapByteOrder(asset->asset_id_scheme);
					UINT16 asset_id = *(UINT16*)(lpTlvPacketBuf + pos + sizeof(Asset_typedef));
					swapByteOrder(asset_id);
					if ((pos += sizeof(Asset_typedef) + asset->asset_id_length) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					//debugPrintf3(L"*Asset identifier_type:%d id_scheme:%lu id_length:%d\n", asset->identifier_type, asset->asset_id_scheme, asset->asset_id_length);

					Asset2_typedef* asset2 = (Asset2_typedef*)(lpTlvPacketBuf + pos);

					//debugPrintf3(L"*Asset2 type:%c%c%c%c clock_relation_flag:%d location_count:%d location_type:%d\n", asset2->asset_type[0], asset2->asset_type[1], asset2->asset_type[2], asset2->asset_type[3], asset2->asset_clock_relation_flag, asset2->location_count, asset2->location_type);
					if ((pos += sizeof(Asset2_typedef)) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}

					UINT16 pid = 0;
					BYTE URL_Len;
					for (int j = 0; j < asset2->location_count; j++) {
						switch (asset2->location_type) {
						case 0:
							pid = *(UINT16*)(lpTlvPacketBuf + pos);
							swapByteOrder(pid);
							if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							break;
						case 1: // IPv4
							if ((pos += 10) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							pid = *(UINT16*)(lpTlvPacketBuf + pos);
							swapByteOrder(pid);
							if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							break;
						case 2: // IPv6
							if ((pos += 34) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							pid = *(UINT16*)(lpTlvPacketBuf + pos);
							swapByteOrder(pid);
							if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							break;
						case 3: // MPEG-2 TS
							if ((pos += 6) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							break;
						case 4: // IPv6 MPEG-2 TS
							if ((pos += 36) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							break;
						case 5: // URL
							URL_Len = *(BYTE*)(lpTlvPacketBuf + pos);
							if ((pos += sizeof(BYTE) + URL_Len) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							break;
						default:
							break;
						}
						//debugPrintf3(L"->PID:0x%04X\n", pid);
					}

					//					OutputDebug(L"ASSET:%c%c%c%c PID:%04X LOC_TYPE:%d LOC_CNT:%d\n", asset2->asset_type[0], asset2->asset_type[1], asset2->asset_type[2], asset2->asset_type[3], pid, asset2->location_type, asset2->location_count);

					if ((video_asset_type[0] == 0) && ((strncmp((const char*)asset2->asset_type, "hev1", 4) == 0) || strncmp((const char*)asset2->asset_type, "hvc1", 4) == 0)) {
						strncpy_s((char*)video_asset_type, 5, (const char*)asset2->asset_type, _TRUNCATE);
					}

					if ((audio_asset_type[0] == 0) && (strncmp((const char*)asset2->asset_type, "mp4a", 4) == 0)) {
						strncpy_s((char*)audio_asset_type, 5, (const char*)asset2->asset_type, _TRUNCATE);
					}

					UINT16 asset_descriptor_length = *(UINT16*)(lpTlvPacketBuf + pos);
					swapByteOrder(asset_descriptor_length);
					//debugPrintf3(L"->asset_descriptor_length:%d\n", asset_descriptor_length);
					//					OutputDebug(L"\n->asset_descriptor_length:%d\n", asset_descriptor_length);

					UINT16 asset_descriptor_bytes = *(UINT16*)(lpTlvPacketBuf + pos + sizeof(UINT16));
					swapByteOrder(asset_descriptor_bytes);
					//					OutputDebug(L"->asset_descriptor_bytes:%04X\n", asset_descriptor_bytes);

					if (asset_descriptor_bytes == 0x8011) { // MH-Stream
						UINT8 descriptor_length = *(UINT8*)(lpTlvPacketBuf + pos + sizeof(UINT16) + sizeof(UINT16));
						//					OutputDebug(L"->descriptor_length:%d\n", descriptor_length);

						UINT16 component_tag = *(UINT16*)(lpTlvPacketBuf + pos + sizeof(UINT16) + sizeof(UINT16) + sizeof(UINT8));
						swapByteOrder(component_tag);
						if ((uiVideo_PID == 0) && (component_tag == 0x0000)) {
							uiVideo_PID = pid;
						}
						else  if ((uiAudio_PID == 0) && (component_tag == 0x0010)) {
							uiAudio_PID = pid;
						}
						else if ((uiStpp_PID == 0) && ((component_tag >= 0x0030) && (component_tag <= 0x0037))) {
							uiStpp_PID = pid;
						}

						if (uiVideo_PID && uiAudio_PID) {
							setup = 1;
						}

						//OutputDebug(L"->component_tag:%04X pid:%04X\n", component_tag, pid);
						/*
						LPBYTE lpb = (lpTlvPacketBuf + pos + sizeof(UINT16) + sizeof(UINT16) + sizeof(UINT8) + sizeof(UINT16));
						INT count = 0;
						do {
							UINT16 descriptor_tag = *(UINT16*)(lpb + count);
							swapByteOrder(descriptor_tag);
							count += sizeof(UINT16);
							BYTE descriptor_length = *(BYTE*)(lpb + count);
							count += sizeof(BYTE);
							OutputDebug(L"des_tag:%04X len:%d\n",  descriptor_tag, descriptor_length);
							for (int i = 0; i < descriptor_length; i++) {
								OutputDebug(L"%02X ", lpb[count + i]);
							}
							OutputDebug(L"\n");
							count += descriptor_length;
						} while ((asset_descriptor_length - 5) > count);
						*/
					}

					if ((pos += sizeof(UINT16) + asset_descriptor_length) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
				}
			}
			else if (table_id == 0x80) { // PLT
				return S_OK;

				PLTable_typedef* plt = (PLTable_typedef*)(lpTlvPacketBuf + pos);
				swapByteOrder(plt->length);
				if ((pos += sizeof(PLTable_typedef)) >= ulTlvPacketSize) {
					return E_FAIL;
				}

				UINT16 sid = *(UINT16*)(pos + lpTlvPacketBuf);
				swapByteOrder(sid);

				UINT16 pid;
				BYTE URL_Len;
				BYTE location_type = *(BYTE*)(pos + lpTlvPacketBuf + sizeof(UINT16));

				if ((pos += sizeof(UINT16) + sizeof(BYTE)) >= ulTlvPacketSize) {
					return E_FAIL;
				}

				switch (location_type) {
				case 0:
					pid = *(UINT16*)(lpTlvPacketBuf + pos);
					swapByteOrder(pid);
					if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					break;
				case 1: // IPv4
					if ((pos += 10) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					pid = *(UINT16*)(lpTlvPacketBuf + pos);
					swapByteOrder(pid);
					if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					break;
				case 2: // IPv6
					if ((pos += 34) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					pid = *(UINT16*)(lpTlvPacketBuf + pos);
					swapByteOrder(pid);
					if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
						OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					break;
				case 3: // MPEG-2 TS
					if ((pos += 6) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					break;
				case 4: // IPv6 MPEG-2 TS
					if ((pos += 36) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					break;
				case 5: // URL
					URL_Len = *(BYTE*)(lpTlvPacketBuf + pos);
					if ((pos += sizeof(BYTE) + URL_Len) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					break;
				default:
					break;
				}

				//				OutputDebug(L"sid:%04X loca_type:%d pid:%04X\n", sid, location_type, pid);
			}

		}
		else if (message_id == 0x8000) { // M2 message
			M2_message_typedef* m2_msg = (M2_message_typedef*)(lpTlvPacketBuf + pos);
			swapByteOrder(m2_msg->length);
			if ((pos += sizeof(M2_message_typedef)) >= ulTlvPacketSize) {
				//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
				return E_FAIL;
			}

			if (mmtp->packet_id == 0x8006) {
				//				OutputDebug(L"MH-CDT %d\n", ulTlvPacketSize);
			}

			//			OutputDebug(L"m2_msg->table_id:%02X\n", m2_msg->table_id);

			if (m2_msg->table_id == 0x82)
			{
				INT section_length = m2_msg->length - sizeof(M2_message_typedef) - 8;// 8 is for "the first 7 bytes are non essential datathe first 7 bytes are non essential data"
				uint8_t* signaling_data_byte = (lpTlvPacketBuf + pos + 7);//the first 7 bytes are non essential data
				if (*(signaling_data_byte + 5) != past_signaling_data_byte) {
					int r = acas->proc_ecm(acas, &res, signaling_data_byte, section_length);
					past_signaling_data_byte = *(signaling_data_byte + 5);//The first 4 bytes always same
				}

				return S_OK;
			}
			else if (m2_msg->table_id == 0x8B) { // MH-EIT (present and next program of self-stream)
				// CRC32チェック　不一致ならリターン
				//LPBYTE crc32_start_p = (LPBYTE)(lpTlvPacketBuf + pos - 1);
				//INT crc32_len = m2_msg->length - 4;
				//UINT32 crc32_val = *(UINT32*)(LPBYTE)(crc32_start_p + crc32_len);
				//swapByteOrder(crc32_val);
				//UINT32 crc32_calc = crc32(crc32_start_p, crc32_len);
				//if (crc32_calc != crc32_val) {
				//	return S_OK;
				//}
				//				OutputDebug(L"1:%08X 2:%08X len:%d%s\n", crc32_calc, crc32_val, crc32_len, crc32_calc != crc32_val ? "x" : "o");

				MHEVT_InfoTable_typedef* evtTable = (MHEVT_InfoTable_typedef*)(lpTlvPacketBuf + pos);
				swapByteOrder(evtTable->ushort0);
				swapByteOrder(evtTable->service_id);
				swapByteOrder(evtTable->tlv_stream_id);
				swapByteOrder(evtTable->original_network_id);
				swapByteOrder(evtTable->event_id);
				swapByteOrder(evtTable->ulong0);
				swapByteOrder(evtTable->ushort1);
				if ((pos += sizeof(MHEVT_InfoTable_typedef)) >= ulTlvPacketSize) {
					//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
					return E_FAIL;
				}

				// service_id（番組の種類）をCIDにより決定する。CID<=1なら若番、それ以外なら老番のservice_idに決定する
				if (content_check.complete == FALSE) {
					content_check.service_id_list[content_check.service_count++] = evtTable->service_id;
					if (content_check.service_count >= 4) {
						UINT16 min_id = 0xFFFF;
						UINT16 max_id = 0x0000;
						for (int i = 0; i < content_check.service_count; i++) {
							if (uiCID <= 1) {
								min_id = min(content_check.service_id_list[i], min_id);
							}
							else {
								max_id = max(content_check.service_id_list[i], max_id);
							}
						}
						if (uiCID <= 1) {
							content_check.service_id = min_id;
						}
						else {
							content_check.service_id = max_id;
						}
						content_check.complete = TRUE;
					}
					else {
						return E_ABORT;
					}
				}

				// 決定したservice_idでなければ無視する
				if (evtTable->service_id != content_check.service_id) {
					return E_ABORT;
				}

				//wsprintf(sz, L"service_id:%04X\n", content_check.service_id);
				//OutputDebugString(sz);

				// 2個の番組情報（ただいまの番組・このあと番組）を格納し管理する
				if (content[0].event_id == 0) {
					content[0].event_id = evtTable->event_id;
					content[0].free_CA_mode = evtTable->free_CA_mode;
					content_check.current_idx = 0;
				}
				else if (content[0].event_id == evtTable->event_id) {
					content_check.current_idx = 0;
					return E_ABORT;
				}
				else if (content[1].event_id == 0) {
					content[1].event_id = evtTable->event_id;
					content[1].free_CA_mode = evtTable->free_CA_mode;
					content_check.current_idx = 1;
					content_check.update = TRUE;
				}
				else if (content[1].event_id == evtTable->event_id) {
					content_check.current_idx = 1;
					return E_ABORT;
				}
				else { // 新しい番組情報
					INT start_time_val0, start_time_val1;
					start_time_val0 = content[0].start_time_val;
					start_time_val1 = content[1].start_time_val;
					if (abs(start_time_val0 - start_time_val1) > (12 * 3600)) {
						if (start_time_val0 < start_time_val1) {
							start_time_val0 += 24 * 3600;
						}
						else {
							start_time_val1 += 24 * 3600;
						}
					}
					if (start_time_val0 < start_time_val1) {
						content[0].event_id = evtTable->event_id;
						content[0].free_CA_mode = evtTable->free_CA_mode;
						content_check.current_idx = 0;
						content[0].contentTitleStr.clear();
						content[0].contentInfoStr.clear();
					}
					else {
						content[1].event_id = evtTable->event_id;
						content[1].free_CA_mode = evtTable->free_CA_mode;
						content_check.current_idx = 1;
						content[1].contentTitleStr.clear();
						content[1].contentInfoStr.clear();
					}
					content_check.update = TRUE;
				}

				INT section_length = evtTable->section_length - (sizeof(MHEVT_InfoTable_typedef) - 2) - sizeof(UINT32);

				content[content_check.current_idx].start_time = evtTable->start_time;
				content[content_check.current_idx].duration = evtTable->duration;

				char hour[3];
				char minute[3];
				char second[3];
				char* e;

				sprintf(hour, "%02X", (UINT)((evtTable->start_time >> 16) & 0xFF));
				sprintf(minute, "%02X", (UINT)((evtTable->start_time >> 8) & 0xFF));
				sprintf(second, "%02X", (UINT)(evtTable->start_time & 0xFF));

				INT start_time_val = strtol(hour, &e, 10) * 3600 + strtol(minute, &e, 10) * 60 + strtol(second, &e, 10);
				content[content_check.current_idx].start_time_val = start_time_val;

				sprintf(hour, "%02X", (UINT)((evtTable->duration >> 16) & 0xFF));
				sprintf(minute, "%02X", (UINT)((evtTable->duration >> 8) & 0xFF));
				sprintf(second, "%02X", (UINT)(evtTable->duration & 0xFF));

				INT duration_val = strtol(hour, &e, 10) * 3600 + strtol(minute, &e, 10) * 60 + strtol(second, &e, 10);
				content[content_check.current_idx].duration_val = duration_val;

				INT year;
				INT month;
				INT day;
				UINT16 mjd = (UINT16)((evtTable->start_time >> 24) & 0xFFFF);

				juian_date(mjd, &year, &month, &day);

				const WCHAR* week[] = { L"水", L"木", L"金" , L"土" , L"日" , L"月" , L"火" };

				WCHAR sz[256];
				WCHAR time_content[100];
				INT end_time_val = start_time_val + duration_val;
				if (end_time_val >= (24 * 3600)) {
					end_time_val -= 24 * 3600;
				}
				wsprintf(sz, L"放送日時 %d/%d/%d（%s） %02d:%02d ～ %02d:%02d", year, month, day, week[mjd % 7], (start_time_val / 3600) % 60, (start_time_val / 60) % 60, (end_time_val / 3600) % 60, (end_time_val / 60) % 60);
				wsprintf(time_content, L"%02d:%02d～%02d:%02d ", (start_time_val / 3600) % 60, (start_time_val / 60) % 60, (end_time_val / 3600) % 60, (end_time_val / 60) % 60);

				content[content_check.current_idx].contentTitleStr.append(time_content);
				content[content_check.current_idx].contentInfoStr.append(sz);
				content[content_check.current_idx].contentInfoStr.append(L"\r\n\r\n");

				do {
					UINT16 descriptor_tag = *(UINT16*)(lpTlvPacketBuf + pos);
					swapByteOrder(descriptor_tag);
					//OutputDebug(L"desc_tag:%04X\n", descriptor_tag);
					if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					section_length -= sizeof(UINT16);

					if (descriptor_tag == 0xF001) { // MH-Short event descriptor
						MHShort_EventDescriptor_typedef* evtDesc = (MHShort_EventDescriptor_typedef*)(lpTlvPacketBuf + pos);
						swapByteOrder(evtDesc->descriptor_length);
						if ((pos += sizeof(MHShort_EventDescriptor_typedef)) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}
						section_length -= sizeof(UINT16);

						LPCCH lpContentTitle = (LPCCH)(lpTlvPacketBuf + pos);

						// ARIB外字から変換処理
						char* find_addr = (char*)lpContentTitle;
						const BYTE find_2K[] = { 0xF0, 0x9F, 0x86, 0x9D, ' ', '2', 'K', ' ' };
						const BYTE find_4K[] = { 0xF0, 0x9F, 0x86, 0x9E, ' ', '4', 'K', ' ' };
						const BYTE find_8K[] = { 0xF0, 0x9F, 0x86, 0x9F, ' ', '8', 'K', ' ' };
						const BYTE find_5_1ch[] = { 0xF0, 0x9F, 0x86, 0xA0, '5', '.', '1', ' ' };
						const BYTE find_7_1ch[] = { 0xF0, 0x9F, 0x86, 0xA1, '7', '.', '1', ' ' };
						const BYTE find_22_1ch[] = { 0xF0, 0x9F, 0x86, 0xA2, '2', '2', '.', '1' };
						const BYTE find_60P[] = { 0xF0, 0x9F, 0x86, 0xA3, '6', '0', 'P', ' ' };
						const BYTE find_120P[] = { 0xF0, 0x9F, 0x86, 0xA4, '1', '2', '0', 'P' };
						const BYTE find_hdr[] = { 0xF0, 0x9F, 0x86, 0xA7, 'H', 'D', 'R', ' ' };
						const LPCBYTE find_outer[] = { find_2K, find_4K, find_8K, find_5_1ch, find_7_1ch, find_22_1ch, find_60P, find_120P, find_hdr };

						for (int i = 0; i < (evtDesc->event_name_length - 3); i++) {
							for (int j = 0; j < (sizeof(find_outer) / sizeof(find_outer[0])); j++) {
								if (memcmp(&find_addr[i], find_outer[j], 4) == 0) {
									memcpy(&find_addr[i], &find_outer[j][4], 4);
									i += 4;
								}
							}
						}

						if ((pos += evtDesc->event_name_length) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}

						UINT16 text_length = *(UINT16*)(lpTlvPacketBuf + pos);
						swapByteOrder(text_length);
						if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}

						LPCCH lpContentInfo = (LPCCH)(lpTlvPacketBuf + pos);

						if ((pos += text_length) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}

						section_length -= evtDesc->descriptor_length;

						if (DWORD wlen = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentTitle, evtDesc->event_name_length, NULL, 0)) {
							WCHAR* wc = new WCHAR[wlen + 1ULL];
							MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentTitle, evtDesc->event_name_length, wc, wlen);
							wc[wlen] = 0;
							std::wstring str = wc;
							content[content_check.current_idx].contentTitleStr.append(str);
							content[content_check.current_idx].contentInfoStr.append(ReplaceString(str, L"\r", L"\r\n"));
							content[content_check.current_idx].contentInfoStr.append(L"\r\n\r\n");
							//wsprintf(wstring, L"%s sid:%04X cn:%d evtID:%02X SC:%d SG:%d orgNetID:%04X time:%04X du:%04X\n", wc, evtTable->service_id, evtTable->current_next_indicator, evtTable->event_id, evtTable->free_CA_mode, evtTable->segment_last_section_number, evtTable->original_network_id, (evtTable->start_time >> 8) & 0xFFFF, (evtTable->duration >> 8) & 0xFFFF);
							//OutputDebugString(wstring);
							SAFE_DELETE_ARRAY(wc);
						}

						if (DWORD wlen1 = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentInfo, text_length, NULL, 0)) {
							WCHAR* wc1 = new WCHAR[wlen1 + 1ULL];
							MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentInfo, text_length, wc1, wlen1);
							wc1[wlen1] = 0;
							std::wstring str = wc1;
							content[content_check.current_idx].contentInfoStr.append(ReplaceString(str, L"\r", L"\r\n"));
							content[content_check.current_idx].contentInfoStr.append(L"\r\n\r\n");
							//wsprintf(wstring, L"%s\n", wc1);
							//OutputDebugString(wstring);
							SAFE_DELETE_ARRAY(wc1);
						}

					}
					else if (descriptor_tag == 0xF002) { // MH-Extended event descriptor
						MHExtended_EventDescriptor_typedef* evtExtDesc = (MHExtended_EventDescriptor_typedef*)(lpTlvPacketBuf + pos);
						swapByteOrder(evtExtDesc->descriptor_length);
						swapByteOrder(evtExtDesc->length_of_items);
						if ((pos += sizeof(MHExtended_EventDescriptor_typedef)) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}
						section_length -= sizeof(UINT16);

						LPCCH lpContentTitle = (LPCCH)(lpTlvPacketBuf + pos);

						if ((pos += evtExtDesc->item_description_length) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}

						UINT16 text_length = *(UINT16*)(lpTlvPacketBuf + pos);
						swapByteOrder(text_length);
						if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}

						LPCCH lpContentInfo = (LPCCH)(lpTlvPacketBuf + pos);

						if ((pos += text_length) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}

						UINT16 text_length2 = *(UINT16*)(lpTlvPacketBuf + pos);
						swapByteOrder(text_length2);
						if ((pos += sizeof(UINT16) + text_length2) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}

						section_length -= evtExtDesc->descriptor_length + text_length2;

						//wsprintf(wstring, L"%d/%d %d\n", evtExtDesc->descriptor_number, evtExtDesc->last_descriptor_number, text_length2);
						//OutputDebugString(wstring);

						if (DWORD wlen = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentTitle, evtExtDesc->item_description_length, NULL, 0)) {
							WCHAR* wc = new WCHAR[wlen + 1ULL];
							MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentTitle, evtExtDesc->item_description_length, wc, wlen);
							wc[wlen] = 0;
							std::wstring str = wc;
							content[content_check.current_idx].contentInfoStr.append(ReplaceString(str, L"\r", L"\r\n"));
							content[content_check.current_idx].contentInfoStr.append(L"\r\n");
							//wsprintf(wstring, L"%s\n", wc);
							//OutputDebugString(wstring);
							SAFE_DELETE_ARRAY(wc);
						}

						if (DWORD wlen1 = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentInfo, text_length, NULL, 0)) {
							WCHAR* wc1 = new WCHAR[wlen1 + 1ULL];
							MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentInfo, text_length, wc1, wlen1);
							wc1[wlen1] = 0;
							std::wstring str = wc1;
							content[content_check.current_idx].contentInfoStr.append(ReplaceString(str, L"\r", L"\r\n"));
							content[content_check.current_idx].contentInfoStr.append(L"\r\n\r\n");
							//wsprintf(wstring, L"%s\n", wc1);
							//OutputDebugString(wstring);
							SAFE_DELETE_ARRAY(wc1);
						}

						if (evtExtDesc->descriptor_number >= evtExtDesc->last_descriptor_number) {
							return S_OK;
						}

						if (text_length2 > 0) {
						}

					}
					else { // 0x8010: Video Component 0x8014: Audio Component
						BYTE descriptor_length = *(BYTE*)(lpTlvPacketBuf + pos);
						//if (descriptor_tag == 0x803F) {
						//	OutputDebug(L"0x803F %d\n", descriptor_length);
						//	for (int i = 0; i < descriptor_length; i++) {
						//		OutputDebug(L"%02X ", *(BYTE*)(lpTlvPacketBuf + pos + 1 + i));
						//	}
						//	OutputDebug(L"\n");
						//}
						if ((pos += sizeof(BYTE) + descriptor_length) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}
						section_length -= (sizeof(BYTE) + descriptor_length);
					}
				} while (section_length > 0);
				return S_OK;
			}
			else if ((m2_msg->table_id >= 0x8C) && (m2_msg->table_id <= 0x94)) { // MH-EIT (present and next program of self-stream)
				return S_OK;
				// CRC32チェック　不一致ならリターン
			//				LPBYTE crc32_start_p = lpTlvPacketBuf + pos - 1;
			//				UINT crc32_len = m2_msg->length - 4;
			//				UINT32 crc32_val = *(UINT32*)(crc32_start_p + crc32_len);
			//				swapByteOrder(crc32_val);
			//				UINT32 crc32_calc = crc32(crc32_start_p, crc32_len);
			//				if (crc32_calc != crc32_val) {
			////					return S_OK;
			//				}
							//				OutputDebug(L"1:%08X 2:%08X len:%d%s\n", crc32_calc, crc32_val, crc32_len, crc32_calc != crc32_val ? "x" : "o");

				MHEVT_Schedule_InfoTable_typedef* evtTable = (MHEVT_Schedule_InfoTable_typedef*)(lpTlvPacketBuf + pos);
				swapByteOrder(evtTable->ushort0);
				swapByteOrder(evtTable->service_id);
				swapByteOrder(evtTable->tlv_stream_id);
				swapByteOrder(evtTable->original_network_id);
				if ((pos += sizeof(MHEVT_Schedule_InfoTable_typedef)) >= ulTlvPacketSize) {
					//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
					return E_FAIL;
				}

				// 決定したservice_idでなければ無視する
				if ((content_check.complete == TRUE) && (evtTable->service_id != content_check.service_id)) {
					return E_ABORT;
				}

				OutputDebug(L"\ntable_id:%02X last_table_id:%02X section_number:%02X last_section_number:%02X segment_last:%02X m2_length:%d sec_len:%d size:%d\n", m2_msg->table_id, evtTable->last_table_id, evtTable->section_number, evtTable->last_section_number, evtTable->segment_last_section_number, m2_msg->length, evtTable->section_length, ulTlvPacketSize);

				INT section_length = evtTable->section_length - (sizeof(MHEVT_Schedule_InfoTable_typedef) - 2) - sizeof(UINT32);

				do {
					MHEVT_Schedule2_InfoTable_typedef* evtTable2 = (MHEVT_Schedule2_InfoTable_typedef*)(lpTlvPacketBuf + pos);
					swapByteOrder(evtTable2->event_id);
					swapByteOrder(evtTable2->ulong0);
					swapByteOrder(evtTable2->ushort1);
					if ((pos += sizeof(MHEVT_Schedule2_InfoTable_typedef)) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}

					char hour[3];
					char minute[3];
					char second[3];
					char* e;

					sprintf(hour, "%02X", (UINT)((evtTable2->start_time >> 16) & 0xFF));
					sprintf(minute, "%02X", (UINT)((evtTable2->start_time >> 8) & 0xFF));
					sprintf(second, "%02X", (UINT)(evtTable2->start_time & 0xFF));

					INT start_time_val = strtol(hour, &e, 10) * 3600 + strtol(minute, &e, 10) * 60 + strtol(second, &e, 10);

					sprintf(hour, "%02X", (UINT)((evtTable2->duration >> 16) & 0xFF));
					sprintf(minute, "%02X", (UINT)((evtTable2->duration >> 8) & 0xFF));
					sprintf(second, "%02X", (UINT)(evtTable2->duration & 0xFF));

					INT duration_val = strtol(hour, &e, 10) * 3600 + strtol(minute, &e, 10) * 60 + strtol(second, &e, 10);

					INT year;
					INT month;
					INT day;
					UINT16 mjd = (UINT16)((evtTable2->start_time >> 24) & 0xFFFF);

					juian_date(mjd, &year, &month, &day);

					const WCHAR* week[] = { L"水", L"木", L"金" , L"土" , L"日" , L"月" , L"火" };

					WCHAR sz[256];
					WCHAR time_content[100];
					INT end_time_val = start_time_val + duration_val;
					if (end_time_val >= (24 * 3600)) {
						end_time_val -= 24 * 3600;
					}
					wsprintf(sz, L"放送日時 %d/%d/%d（%s） %02d:%02d ～ %02d:%02d sid:%04X eid:%04X loop_len:%d size:%d", year, month, day, week[mjd % 7], (start_time_val / 3600) % 60, (start_time_val / 60) % 60, (end_time_val / 3600) % 60, (end_time_val / 60) % 60, evtTable->service_id, evtTable2->event_id, evtTable2->descriptors_loop_length, ulTlvPacketSize);
					wsprintf(time_content, L"%02d:%02d～%02d:%02d ", (start_time_val / 3600) % 60, (start_time_val / 60) % 60, (end_time_val / 3600) % 60, (end_time_val / 60) % 60);

#define DAYDAY 13
#define enableble 1
					if ((day == DAYDAY) || (enableble)) {
						OutputDebugString(sz);
						OutputDebugString(L"\n");
					}


					INT descriptor_loop_length = evtTable2->descriptors_loop_length;
					do {
						UINT16 descriptor_tag = *(UINT16*)(lpTlvPacketBuf + pos);
						swapByteOrder(descriptor_tag);
						//						OutputDebug(L"desc_tag:%04X\n", descriptor_tag);
						if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
							//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
							return E_FAIL;
						}
						descriptor_loop_length -= sizeof(UINT16);

						if (descriptor_tag == 0xF001) { // MH-Short event descriptor
							MHShort_EventDescriptor_typedef* evtDesc = (MHShort_EventDescriptor_typedef*)(lpTlvPacketBuf + pos);
							swapByteOrder(evtDesc->descriptor_length);
							if ((pos += sizeof(MHShort_EventDescriptor_typedef)) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							descriptor_loop_length -= sizeof(UINT16);

							LPCCH lpContentTitle = (LPCCH)(lpTlvPacketBuf + pos);

							// ARIB外字から変換処理
							char* find_addr = (char*)lpContentTitle;
							const BYTE find_2K[] = { 0xF0, 0x9F, 0x86, 0x9D, ' ', '2', 'K', ' ' };
							const BYTE find_4K[] = { 0xF0, 0x9F, 0x86, 0x9E, ' ', '4', 'K', ' ' };
							const BYTE find_8K[] = { 0xF0, 0x9F, 0x86, 0x9F, ' ', '8', 'K', ' ' };
							const BYTE find_5_1ch[] = { 0xF0, 0x9F, 0x86, 0xA0, '5', '.', '1', ' ' };
							const BYTE find_7_1ch[] = { 0xF0, 0x9F, 0x86, 0xA1, '7', '.', '1', ' ' };
							const BYTE find_22_1ch[] = { 0xF0, 0x9F, 0x86, 0xA2, '2', '2', '.', '1' };
							const BYTE find_60P[] = { 0xF0, 0x9F, 0x86, 0xA3, '6', '0', 'P', ' ' };
							const BYTE find_120P[] = { 0xF0, 0x9F, 0x86, 0xA4, '1', '2', '0', 'P' };
							const BYTE find_hdr[] = { 0xF0, 0x9F, 0x86, 0xA7, 'H', 'D', 'R', ' ' };
							const LPCBYTE find_outer[] = { find_2K, find_4K, find_8K, find_5_1ch, find_7_1ch, find_22_1ch, find_60P, find_120P, find_hdr };

							for (int i = 0; i < (evtDesc->event_name_length - 3); i++) {
								for (int j = 0; j < (sizeof(find_outer) / sizeof(find_outer[0])); j++) {
									if (memcmp(&find_addr[i], find_outer[j], 4) == 0) {
										memcpy(&find_addr[i], &find_outer[j][4], 4);
										i += 4;
									}
								}
							}

							if ((pos += evtDesc->event_name_length) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}

							UINT16 text_length;// = *(UINT16*)(lpTlvPacketBuf + pos);
							memcpy(&text_length, (lpTlvPacketBuf + pos), sizeof(UINT16));
							swapByteOrder(text_length);
							if ((pos += sizeof(UINT16)) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}

							LPCCH lpContentInfo = (LPCCH)(lpTlvPacketBuf + pos);

							if ((pos += text_length) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}

							descriptor_loop_length -= evtDesc->descriptor_length;

							if (DWORD wlen = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentTitle, evtDesc->event_name_length, NULL, 0)) {
								if (wlen > 40) {
									wlen = 40;
								}
								WCHAR* wc = new WCHAR[wlen + 1ULL];
								MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentTitle, evtDesc->event_name_length, wc, wlen);
								wc[wlen] = 0;
								//wsprintf(wstring, L"%s sid:%04X cn:%d evtID:%02X SC:%d SG:%d orgNetID:%04X time:%04X du:%04X\n", wc, evtTable->service_id, evtTable->current_next_indicator, evtTable->event_id, evtTable->free_CA_mode, evtTable->segment_last_section_number, evtTable->original_network_id, (evtTable->start_time >> 8) & 0xFFFF, (evtTable->duration >> 8) & 0xFFFF);
								if ((day == DAYDAY) || (enableble)) {
									OutputDebugString(wc);
									OutputDebugString(L"\n");
								}
								SAFE_DELETE_ARRAY(wc);
							}

							if (DWORD wlen1 = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentInfo, text_length, NULL, 0)) {
								if (wlen1 > 80) {
									wlen1 = 80;
								}
								WCHAR* wc1 = new WCHAR[wlen1 + 1ULL];
								MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lpContentInfo, text_length, wc1, wlen1);
								wc1[wlen1] = 0;
								if ((day == DAYDAY) || (enableble)) {
									OutputDebugString(wc1);
									OutputDebugString(L"\n");
								}
								SAFE_DELETE_ARRAY(wc1);
							}

						}
						else {
							BYTE descriptor_length = *(BYTE*)(lpTlvPacketBuf + pos);
							if ((pos += sizeof(BYTE) + descriptor_length) >= ulTlvPacketSize) {
								//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
								return E_FAIL;
							}
							descriptor_loop_length -= (sizeof(BYTE) + descriptor_length);
						}
					} while (descriptor_loop_length > 0);
					section_length -= sizeof(MHEVT_Schedule2_InfoTable_typedef) + evtTable2->descriptors_loop_length;
				} while (section_length > 0);
				return S_OK;
			}
			//else if ((m2_msg->table_id == 0x82) || (m2_msg->table_id == 0x83)) {
				//				puts("ECM");
			//}
			else if ((m2_msg->table_id == 0x84) || (m2_msg->table_id == 0x85)) {
				//				puts("EMM");
			}
			else if ((m2_msg->table_id == 0x87) || (m2_msg->table_id == 0x88)) {
				//				puts("DCM");
			}
			else if ((m2_msg->table_id == 0x89) || (m2_msg->table_id == 0x8A)) {
				//				puts("DMM");
			}

			return S_OK;
		}
		else if (message_id == 0x8001) { // CA message
			return S_OK;

			CA_message_typedef* ca_msg = (CA_message_typedef*)(pos + lpTlvPacketBuf);
			swapByteOrder(ca_msg->length);
			if ((pos += sizeof(CA_message_typedef)) >= ulTlvPacketSize) {
				//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
				return E_FAIL;
			}

			printf("%d\n", ca_msg->length);

			CATable_typedef* ca_tbl = (CATable_typedef*)(pos + lpTlvPacketBuf);
			swapByteOrder(ca_tbl->length);
			if ((pos += sizeof(CATable_typedef)) >= ulTlvPacketSize) {
				//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
				return E_FAIL;
			}
			int ca_table_length = ca_tbl->length;
			if (ca_tbl->table_id == 0x86) {
				puts("CAT");
				do {
					MMTSI_typedef_struct* mmtsi = (MMTSI_typedef_struct*)(pos + lpTlvPacketBuf);
					swapByteOrder(mmtsi->descriptor_tag);
					printf("tag:%04X pid:%04X\n", mmtsi->descriptor_tag, mmtp->packet_id);
					if (mmtsi->descriptor_tag == 0x8005) { // Scramble Descriptor
						BYTE scramble_system_id = *(BYTE*)(pos + sizeof(MMTSI_typedef_struct) + lpTlvPacketBuf + 1);
						if (scramble_system_id == 1) {
							puts("scramble_system_id:AES");
						}
						else if (scramble_system_id == 2) {
							puts("scramble_system_id:Camelia");
						}
					}
					if ((pos += sizeof(MMTSI_typedef_struct) + mmtsi->descriptor_length) >= ulTlvPacketSize) {
						//OutputDebug(L"parsePacket pos exceed ulTlvPacketSize line at:%d\n", __LINE__);
						return E_FAIL;
					}
					ca_table_length -= sizeof(MMTSI_typedef_struct) + mmtsi->descriptor_length;
				} while (ca_table_length > 0);
			}

			//			MessageBox(NULL, L"STP", L"STOP", MB_OK);

			return S_OK;
		}
		else if (message_id == 0x8002) {  // M2 short message
			if (mmtp->packet_id == 0x8005) { // MH-TOT
//				OutputDebug(L"MH-TOT\n");
				MHTime_Offset_table_typedef* mh_time = (MHTime_Offset_table_typedef*)(pos + lpTlvPacketBuf);
				swapByteOrder(mh_time->length);
				swapByteOrder(mh_time->ushort0);
				swapByteOrder(mh_time->ushort1);
				if ((pos += sizeof(MHTime_Offset_table_typedef)) >= ulTlvPacketSize) {
					return S_OK;
				}

				extern INT g_hour;
				extern INT g_minute;
				extern INT g_second;

				char hour[3];
				char minute[3];
				char second[3];
				char* e;

				sprintf(hour, "%02X", mh_time->JST_time[2]);
				sprintf(minute, "%02X", mh_time->JST_time[3]);
				sprintf(second, "%02X", mh_time->JST_time[4]);

				g_hour = strtol(hour, &e, 10);
				g_minute = strtol(minute, &e, 10);
				g_second = strtol(second, &e, 10);
			}
		}
	}

	return S_OK;
}

void juian_date(UINT16 mjd, INT* Year, INT* Month, INT* Day)
{
	int k = 0;

	*Year = (INT)floor((mjd - 15078.2) / 365.25);
	*Month = (INT)floor((mjd - 14956.1 - floor(*Year * 365.25)) / 30.6001);
	*Day = mjd - 14956 - (INT)floor(*Year * 365.25) - (INT)floor(*Month * 30.6001);
	if ((*Month == 14) || (*Month == 15)) {
		k = 1;
	}

	*Year = *Year + k + 1900;
	*Month = *Month - 1 - k * 12;
}

/*
	wstring中の特定文字列をwstringで置換する
*/
std::wstring ReplaceString
(
	std::wstring String1  // 置き換え対象
	, std::wstring String2  // 検索対象
	, std::wstring String3  // 置き換える内容
)
{
	std::wstring::size_type  Pos(String1.find(String2));

	while (Pos != std::string::npos)
	{
		String1.replace(Pos, String2.length(), String3);
		Pos = String1.find(String2, Pos + String3.length());
	}

	return String1;
}
