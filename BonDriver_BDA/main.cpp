
#include "BonTuner.h"

#include "tlv_parse.h"

#include "MyAsyncSrcFilter.h"

#include "frame_buffer.h"

#include "resource2.h"

#include "LAVSplitter.h"

#include "mvrInterfaces.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <stdio.h>

#include <commctrl.h>

#include <ShellScalingApi.h>

#include <xmllite.h>

#include <atlstr.h>

#include <WinUser.h>

HRESULT AddToRot(IUnknown* pUnkGraph, DWORD* pdwRegister)
{
	IMoniker* pMoniker;
	IRunningObjectTable* pROT;
	if (FAILED(GetRunningObjectTable(0, &pROT))) {
		return E_FAIL;
	}
	WCHAR wsz[256];
	wsprintfW(wsz, L"FilterGraph %08p pid %08x", (DWORD_PTR)pUnkGraph, GetCurrentProcessId());
	HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
	if (SUCCEEDED(hr)) {
		hr = pROT->Register(0, pUnkGraph, pMoniker, pdwRegister);
		pMoniker->Release();
	}
	pROT->Release();
	return hr;
}

void RemoveFromRot(DWORD pdwRegister)
{
	IRunningObjectTable* pROT;
	if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
		pROT->Revoke(pdwRegister);
		pROT->Release();
	}
}


WCHAR debug_info_message[2048];

WCHAR channel_title_str[MAX_PATH];

CCritSec m_ThreadTimeoutLock;
CCritSec m_Timer1HzLock;
CCritSec m_SubtitleLock;
CCritSec m_ClockLock;
CCritSec m_ManipulateUILock;

HINSTANCE ghInst;

DWORD dwRegister = NULL;


CFrameBuffer* video_frame_buffer = NULL;
CFrameBuffer* audio_frame_buffer = NULL;

#define LAV_START_TO_PROCESS_READ_SIZE 46080
#define LAV_AUDIO_READ_SIZE 35840
#define LAV_VIDEO_READ_SIZE 131072*8/////////////////////////////////


////ACAS
//A_CAS_CARD* acas;
//A_CAS_ID           casid;
//CARD_SCRAMBLEKEY caskey;
//A_CAS_INIT_STATUS is;
//A_CAS_ECM_RESULT res;

// BonDriverで使用
ULONGLONG g_count = 0;
INT g_record = 0;
LONG Frequency;
LONG tsid;
LONG cid;

// timer1Hz callbackで使用
UINT timer = NULL;
ULONGLONG pre_count = 0;
LONG elapsed = 0;
LONG pre_elapsed = 0;
int write_error = 0;
int set_exit = 0;

// フルスクリーンチェック用
int full_screen = 0;


// 字幕用
ULONGLONG time_to_start_graph = 2000;

FT_Library library;
FT_Library library2;
FT_Face face;
FT_Face face2;
FT_Stroker stroker;
FT_Stroker stroker2;

HBITMAP bitmap_sub = NULL;
LPVOID ppv_sub = NULL;

HBITMAP bitmap_clock = NULL;
LPVOID ppv_clock = NULL;

HBITMAP bitmap_ui = NULL;
LPVOID ppv_ui = NULL;


UINT timer_subtitle_thread[XML_SUB_BUFFER_NUM];
INT timer_subtitle_thread_index = 0;

INT g_hour = -1;
INT g_minute = -1;
INT g_second = -1;

// 録画再生用
POINT g_point = {};
INT ui_screen = 0;
HANDLE hFileMMTP = NULL;
LPBYTE buffer0 = NULL;
LPBYTE buffer1 = NULL;
//#define BUFFER_SIZE (96256 * 2)
#define BUFFER_SIZE 131072*8//////////////////////////////////////////////////////

OPENFILENAME ofn;
TCHAR szOfnFile[MAX_PATH] = { 0 };
TCHAR szOfnPath[MAX_PATH] = { 0 };
INT bf = 0;
UINT timerMMTP = NULL;
UINT cid_MMTP = 1;

LONG mmtp_start_time = 0;
LONG mmtp_end_time = 0;
LONG mmtp_duration_time = 0;
LONG mmtp_current_time = 0;

LARGE_INTEGER file_seek;
LARGE_INTEGER file_seek_current;
LARGE_INTEGER file_seek_end;
LARGE_INTEGER file_seek_first_TOT;
LARGE_INTEGER file_seek_last_TOT;

IBaseFilter* pLAVVideoDecoder = NULL;
IBaseFilter* pLAVAudioDecoder = NULL;

INT hideCursor = 0;
INT manipulate_lock = 0;

#define	COMMAND_MANIPULATE_UI_INIT 0
#define COMMAND_MANIPULATE_UI_CHANGE_SEEKBAR_POS 1
#define	COMMAND_MANIPULATE_UI_LBUTTON_RELEASE 2
#define COMMAND_MANIPULATE_UI_CURRENT_TIME 3

#define PANEL_X 640LL
#define PANEL_Y 1790LL
#define PANEL_WIDTH 2560LL
#define PANEL_HEIGHT 300LL

#define SEEK_BAR_X_LIMIT_MAX (PANEL_X + 2430LL)
#define SEEK_BAR_X_LIMIT_MIN (PANEL_X + 65LL)
#define SEEK_BAR_Y_POS (PANEL_Y + 190LL)
#define SEEK_BAR_WIDTH 65LL
#define SEEK_BAR_HEIGHT 65LL

#define PLAY_ICON_X_POS (PANEL_X + 1220LL)
#define PLAY_ICON_Y_POS (PANEL_Y + 45LL)
#define PLAY_ICON_WIDTH 120LL
#define PLAY_ICON_HEIGHT 120LL

#define PREV_ICON_X_POS (PANEL_X + 925LL)
#define PREV_ICON_Y_POS (PANEL_Y + 45LL)
#define PREV_ICON_WIDTH 160LL
#define PREV_ICON_HEIGHT 120LL

#define NEXT_ICON_X_POS (PANEL_X + 1475LL)
#define NEXT_ICON_Y_POS (PANEL_Y + 45LL)
#define NEXT_ICON_WIDTH 160LL
#define NEXT_ICON_HEIGHT 120LL

#define ELAPSED_TIME_X (PANEL_X + 65LL)
#define ELAPSED_REST_TIME_Y (PANEL_Y + 150LL)
#define ELAPSED_BACK_WIDTH 330LL
#define REST_TIME_X1 (PANEL_X + 2145LL)
#define REST_TIME_X2 (PANEL_X + 2250LL)
#define REST_BACK_WIDTH1 370LL
#define REST_BACK_WIDTH2 260LL



#pragma pack(push, 1)

struct {
	INT command;
	INT width;
	INT height;
	BOOL top_most;  // 常に最前面
	BOOL title_bar; // タイトルバー表示用
	BOOL subtile; //字幕表示
	BOOL clock; //時計表示
	RECT WindowRect; // WindowRect保存用
	INT volume_command;
	INT level;
	INT select_command_ch;
	INT select_ch;
}display_size = \
{
	//ID_40044, 1920, 1080, FALSE, TRUE, FALSE, FALSE, {},
	ID_40044, 1920, 1080, FALSE, FALSE, FALSE, FALSE, {},
		ID_50002, -500, // ボリューム 大
		ID_40002, 0 // NHK
};

#pragma pack(pop)

#define CHANNEL_STATUS_HALT 0
#define CHANNEL_STATUS_TUNER_OPENING 1
#define CHANNEL_STATUS_TUNER_OPEN_FAILED 2
#define CHANNEL_STATUS_SELECTING 3
#define CHANNEL_STATUS_RECORD_FILE_LOADING 4
#define CHANNEL_STATUS_BUFFERING 5
#define CHANNEL_STATUS_SELECT_FAILED 6
#define CHANNEL_STATUS_SCRAMBLE 7
//audio_delay in milli seconds, no delay set for funcFileOpen. ID_40008 is BS8K
LONGLONG		audio_delay[] = { 500,		2200,		300,	600,		2000,		500,	500,		500,	600,		500,		500 };
DWORD channel_to_id_table[] = { ID_40012 , ID_40002, ID_40003, ID_40004, ID_40005, ID_40006, ID_40007, ID_40008, ID_40009, ID_40010, ID_40011 };
INT channel_number = sizeof(channel_to_id_table) / sizeof(channel_to_id_table[0]);
INT channel_status = 0; // 選局状態 0:なし 1:チューナーオープン 2:選局中 3:バッファリング中 4:選局失敗 5:スクランブル放送
HMENU hmenu;
NMHDR nmhdr;


UINT timer_thread = NULL;
DWORD threadID;
HANDLE hThread = NULL; // チャンネル切り替えスレッド

#define THREAD_CHANNLE_SELECT_START_CODE 0xABCD
#define SCRAMBLE_DETECT_CODE 0xDCBA
#define XML_SUB_UPDATE_CODE 0xAAAA
#define NOTIFY_CONTENT_UPDATE_CODE 0xBBBB
#define MMTP_PACKET_READ_CODE 0xCCCC


BOOL channel_select_thread_running = FALSE;


CBonTuner* tuner = NULL;
CTlvParse* tlv = NULL;

IGraphBuilder* pGraphBuilder = NULL;
IMediaControl* pMediaControl = NULL;
ICaptureGraphBuilder2* pCaptureGraphBuilder2 = NULL;
IBaseFilter* pVideo = NULL;
IBaseFilter* pAudio = NULL;
IBasicAudio* pBasicAudio = NULL;
CMyAsyncSrc* myAsyncVideoSrc = NULL;
IBaseFilter* pVideoSrc = NULL;
CMyAsyncSrc* myAsyncAudioSrc = NULL;
IBaseFilter* pAudioSrc = NULL;
IVideoWindow* pVideoWindow = NULL;
IMadVROsdServices* pMadVROsdServices = NULL;
IMadVRCommand* pMadVRCommand = NULL;

CLAVSplitter* pSplitterAudio = NULL;
IBaseFilter* pSplitterAudioSrc = NULL;

CLAVSplitter* pSplitterVideo = NULL;
IBaseFilter* pSplitterVideoSrc = NULL;


HWND m_hWnd = NULL;
HWND hdlg = NULL;
HWND hdlgSelectCID = NULL;
HWND hdlgContent = NULL;
HWND hTab = NULL;
HWND hEdit = NULL;

TCHAR titleBarText[_MAX_FNAME];

static void CALLBACK Timer1HzCallBack(unsigned int timerID, unsigned int msg, unsigned int usrParam, unsigned int dw1, unsigned int dw2);
static void CALLBACK TimerMMTPCallBack(unsigned int timerID, unsigned int msg, unsigned int usrParam, unsigned int dw1, unsigned int dw2);
static DWORD WINAPI ThreadMMTPStarCaptureGraph(LPVOID lpVoid);
static DWORD WINAPI ThreadMMTPStarCaptureGraphSeek(LPVOID lpVoid);

static HRESULT startCaptureGraph();
static HRESULT CreateGraphs();
static HRESULT ReleaseGraphs();


//CID
INT_PTR CALLBACK DlgProcSelectCID(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
	{
	}
	break;
	case WM_COMMAND:
	{
		switch (LOWORD(wParam)) {
		case IDC_BUTTON1:
			ShowWindow(hWnd, SW_HIDE);
			PostQuitMessage(0);
			break;
		}
	}
	break;
	case WM_SETICON:
	{
		DefWindowProc(hWnd, msg, wParam, lParam);
	}
	break;
	case WM_CLOSE:
	{
		ShowWindow(hWnd, SW_HIDE);
		PostQuitMessage(0);
	}
	break;
	}
	return 0;
}


//デバッグ情報
INT_PTR CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	HFONT hFont;

	switch (msg) {
	case WM_CREATE:
	{
	}
	break;
	case WM_SETICON:
	{

		DefWindowProc(hWnd, msg, wParam, lParam);
	}
	break;
	case WM_PAINT:
	{
		if (IsWindowVisible(hWnd) == FALSE) {
			break;
		}

		RECT rect = {};
		GetClientRect(hWnd, &rect);
		rect.top += 10;
		rect.left += 10;
		hFont = CreateFont(21, 10, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, NULL);

		InvalidateRect(hWnd, &rect, FALSE);
		hdc = BeginPaint(hWnd, &ps);
		SelectObject(hdc, hFont);
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, GetSysColor(COLOR_MENU));
		DrawText(hdc, debug_info_message, -1, &rect, DT_TOP | DT_NOCLIP);
		SelectObject(hdc, GetStockObject(SYSTEM_FONT));
		DeleteObject(hFont);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_CLOSE:
	{
		ShowWindow(hWnd, SW_HIDE);

		//　デバッグ情報チェックを外す
		UINT uState = GetMenuState(hmenu, ID_POPUPMENU0_50000, MF_BYCOMMAND);
		if (uState & MFS_CHECKED) {
			CheckMenuItem(hmenu, ID_POPUPMENU0_50000, MF_BYCOMMAND | MFS_UNCHECKED);
		}
	}
	break;
	}
	return 0;
}


//番組情報
INT_PTR CALLBACK DlgContentProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_NOTIFY:
	{
		//		WCHAR sz[100];
		switch (((LPNMHDR)lParam)->idFrom)
		{
		case IDC_TAB1:
		{
			if (((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
				if ((TabCtrl_GetCurSel(hTab) == 0) && (tlv != NULL)) {
					INT start_time_val0, start_time_val1;
					start_time_val0 = tlv->content[0].start_time_val;
					start_time_val1 = tlv->content[1].start_time_val;
					if (abs(start_time_val0 - start_time_val1) > (12 * 3600)) {
						if (start_time_val0 < start_time_val1) {
							start_time_val0 += 24 * 3600;
						}
						else {
							start_time_val1 += 24 * 3600;
						}
					}
					if (start_time_val0 < start_time_val1) {
						SetWindowText(hEdit, tlv->content[0].contentInfoStr.data());
					}
					else {
						SetWindowText(hEdit, tlv->content[1].contentInfoStr.data());
					}
				}
				else if ((TabCtrl_GetCurSel(hTab) == 1) && (tlv != NULL)) {
					INT start_time_val0, start_time_val1;
					start_time_val0 = tlv->content[0].start_time_val;
					start_time_val1 = tlv->content[1].start_time_val;
					if (abs(start_time_val0 - start_time_val1) > (12 * 3600)) {
						if (start_time_val0 < start_time_val1) {
							start_time_val0 += 24 * 3600;
						}
						else {
							start_time_val1 += 24 * 3600;
						}
					}
					if (start_time_val0 > start_time_val1) {
						SetWindowText(hEdit, tlv->content[0].contentInfoStr.data());
					}
					else {
						SetWindowText(hEdit, tlv->content[1].contentInfoStr.data());
					}
				}
				HideCaret(hEdit);
			}
		}
		break;
		}
	}
	break;
	case WM_CLOSE:
	{
		ShowWindow(hWnd, SW_HIDE);

		//　番組情報チェックを外す
		UINT uState = GetMenuState(hmenu, ID_POPUPMENU0_40011, MF_BYCOMMAND);
		if (uState & MFS_CHECKED) {
			CheckMenuItem(hmenu, ID_POPUPMENU0_40011, MF_BYCOMMAND | MFS_UNCHECKED);
		}
	}
	break;
	}
	return 0;
}

static int Manipulate_UI(LONGLONG posX, LONGLONG posY, INT command)
{
	CAutoLock lock_it(&m_ManipulateUILock);

	if (manipulate_lock) {
		return 1;
	}

	ULONGLONG pixel_addr;
	UINT32* p = (UINT32*)ppv_ui;
	UINT32* pPixel;
	INT offsetX;
	INT offsetY;
	BYTE glyph_pixel_val;
	BYTE fgA, fgR, fgG, fgB;
	BYTE bgA, bgR, bgG, bgB;
	BYTE tA, tR, tG, tB;
	DOUBLE alpha_ratio;
	FT_Bitmap* bm;

	RECT rect = {};
	RECT rect2 = {};
	RECT rect3 = {};
	RECT rect4 = {};

	INT ret_val = 0;
	INT back_to_seek_play = 0;
	static INT seek_play = 0;

	static LONG rest_time = 0;

	static INT hit = 0;
	static INT play = 1;
	static INT play_hit = 0;

	static LONGLONG seek_pos_x = SEEK_BAR_X_LIMIT_MIN;

	static LPVOID pLockedResourceImageSeekBar = NULL;
	static LPVOID pLockedResourceImageSeekBar2 = NULL;
	static LPVOID pLockedResourceImageSeekPoint = NULL;

	UINT32* pImageSeekBarSrcPixel;
	UINT32* pImageSeekBarSrcPixel2;
	UINT32* pImageSeekPointSrcPixel;

	if (pLockedResourceImageSeekBar == NULL) {
		HGLOBAL hLoadedResourceImage = NULL;
		HRSRC hResourceImage = NULL;

		hResourceImage = FindResource(ghInst, MAKEINTRESOURCE(IDR_RAW_IMAGE_SEEKBAR_2560x300), L"RAW_IMAGE");

		if (hResourceImage)
		{
			hLoadedResourceImage = LoadResource(ghInst, hResourceImage);
			if (hLoadedResourceImage)
			{
				pLockedResourceImageSeekBar = LockResource(hLoadedResourceImage);
				if (pLockedResourceImageSeekBar)
				{
				}
			}
		}
	}

	pImageSeekBarSrcPixel = (UINT32*)pLockedResourceImageSeekBar;

	if (pLockedResourceImageSeekBar2 == NULL) {
		HGLOBAL hLoadedResourceImage = NULL;
		HRSRC hResourceImage = NULL;

		hResourceImage = FindResource(ghInst, MAKEINTRESOURCE(IDR_RAW_IMAGE_SEEKBAR2_2560x300), L"RAW_IMAGE");

		if (hResourceImage)
		{
			hLoadedResourceImage = LoadResource(ghInst, hResourceImage);
			if (hLoadedResourceImage)
			{
				pLockedResourceImageSeekBar2 = LockResource(hLoadedResourceImage);
				if (pLockedResourceImageSeekBar2)
				{
				}
			}
		}
	}

	pImageSeekBarSrcPixel2 = (UINT32*)pLockedResourceImageSeekBar2;

	if (pLockedResourceImageSeekPoint == NULL) {
		HGLOBAL hLoadedResourceImage = NULL;
		HRSRC hResourceImageSeekPoint = NULL;

		hResourceImageSeekPoint = FindResource(ghInst, MAKEINTRESOURCE(IDR_RAW_IMAGE_SEEKPOINT_65x65), L"RAW_IMAGE");

		if (hResourceImageSeekPoint)
		{
			hLoadedResourceImage = LoadResource(ghInst, hResourceImageSeekPoint);
			if (hLoadedResourceImage)
			{
				pLockedResourceImageSeekPoint = LockResource(hLoadedResourceImage);
				if (pLockedResourceImageSeekPoint)
				{
				}
			}
		}
	}

	rect.left = PANEL_X;
	rect.right = rect.left + PANEL_WIDTH;
	rect.top = PANEL_Y;
	rect.bottom = rect.top + PANEL_HEIGHT;

	if (command == COMMAND_MANIPULATE_UI_INIT) {
		seek_pos_x = SEEK_BAR_X_LIMIT_MIN;

		for (LONGLONG y = rect.top; y < rect.bottom; y++) {
			for (LONGLONG x = rect.left; x < rect.right; x++) {
				pixel_addr = (2159LL - y) * 3840LL + x;
				if (pixel_addr < (3840 * 2160)) {
					p[pixel_addr] = pImageSeekBarSrcPixel[PANEL_WIDTH * (y - rect.top) + (x - rect.left)];
				}
			}
		}

		// プレイアイコン再生中(一時停止)
		rect4.left = PLAY_ICON_X_POS;
		rect4.right = rect4.left + PLAY_ICON_WIDTH;
		rect4.top = PLAY_ICON_Y_POS;
		rect4.bottom = rect4.top + PLAY_ICON_HEIGHT;

		for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
			for (LONGLONG x = rect4.left; x < rect4.right; x++) {
				pixel_addr = (2159LL - y) * 3840LL + x;
				if (pixel_addr < (3840 * 2160)) {
					p[pixel_addr] = pImageSeekBarSrcPixel2[PANEL_WIDTH * ((y - rect4.top) + (PLAY_ICON_Y_POS - PANEL_Y)) + (x - rect4.left + PLAY_ICON_X_POS - PANEL_X)];
				}
			}
		}

		pImageSeekPointSrcPixel = (UINT32*)pLockedResourceImageSeekPoint;

		rect2.left = seek_pos_x;
		rect2.right = rect2.left + SEEK_BAR_WIDTH;
		rect2.top = SEEK_BAR_Y_POS;
		rect2.bottom = rect2.top + SEEK_BAR_HEIGHT;

		for (long y = rect2.top; y < rect2.bottom; y++) {
			for (long x = rect2.left; x < rect2.right; x++) {
				pixel_addr = (2159LL - y) * 3840LL + x;
				if (pixel_addr < (3840 * 2160)) {

					pPixel = &p[pixel_addr];

					bgA = (*pPixel >> 24) & 0xFF;
					bgR = (*pPixel >> 16) & 0xFF;
					bgG = (*pPixel >> 8) & 0xFF;
					bgB = (*pPixel >> 0) & 0xFF;

					fgA = (*pImageSeekPointSrcPixel >> 24) & 0xFF;
					fgR = (*pImageSeekPointSrcPixel >> 16) & 0xFF;
					fgG = (*pImageSeekPointSrcPixel >> 8) & 0xFF;
					fgB = (*pImageSeekPointSrcPixel >> 0) & 0xFF;

					alpha_ratio = (DOUBLE)fgA / 255.0;

					tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
					tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
					tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
					tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

					*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);

					pImageSeekPointSrcPixel++;
				}
			}
		}

		rest_time = mmtp_duration_time - mmtp_current_time;

		hit = 0;
		play = 1;
		play_hit = 0;
		seek_play = 0;
		ui_screen = 1;
	}

	pImageSeekPointSrcPixel = (UINT32*)pLockedResourceImageSeekPoint;

	rect.left = PANEL_X;
	rect.right = rect.left + PANEL_WIDTH;
	rect.top = PANEL_Y;
	rect.bottom = rect.top + PANEL_HEIGHT;

	rect2.left = seek_pos_x;
	rect2.right = rect2.left + SEEK_BAR_WIDTH;
	rect2.top = SEEK_BAR_Y_POS;
	rect2.bottom = rect2.top + SEEK_BAR_HEIGHT;

	rect3.left = rect2.left - rect.left;
	rect3.right = rect3.left + SEEK_BAR_WIDTH;
	rect3.top = rect2.top - rect.top;
	rect3.bottom = rect3.top + SEEK_BAR_HEIGHT;

	if (command == COMMAND_MANIPULATE_UI_CHANGE_SEEKBAR_POS) {
		if ((hit == 1) && (seek_play == 0)) {
		SILIDE_SEEKBAR:
			//			OutputDebug(L"Slide SeekBar\n");

			file_seek.QuadPart = 0;
			SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_CURRENT);

			if (file_seek_current.QuadPart >= file_seek_end.QuadPart) {
				play = 0;
				// プレイアイコン一時停止
				rect4.left = PLAY_ICON_X_POS;
				rect4.right = rect4.left + PLAY_ICON_WIDTH;
				rect4.top = PLAY_ICON_Y_POS;
				rect4.bottom = rect4.top + PLAY_ICON_HEIGHT;

				if (timer) {
					timeKillEvent(timer);
					timer = 0;
				}
				pMediaControl->Pause();

				UINT32* pImageSrc = play ? pImageSeekBarSrcPixel2 : pImageSeekBarSrcPixel;

				for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
					for (LONGLONG x = rect4.left; x < rect4.right; x++) {
						pixel_addr = (2159LL - y) * 3840LL + x;
						if (pixel_addr < (3840 * 2160)) {
							p[pixel_addr] = pImageSrc[PANEL_WIDTH * ((y - rect4.top) + (PLAY_ICON_Y_POS - PANEL_Y)) + (x - rect4.left + PLAY_ICON_X_POS - PANEL_X)];
						}
					}
				}

				play_hit = 1;
			}

			for (LONGLONG y = rect2.top; y < rect2.bottom; y++) {
				for (LONGLONG x = rect2.left; x < rect2.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSeekBarSrcPixel[PANEL_WIDTH * (rect3.top + y - rect2.top) + rect3.left + (x - rect2.left)];
					}
				}
			}

			if (seek_play == 0) {
				seek_pos_x = posX - 30;
			}
			else if (seek_play == 1) {
				seek_pos_x = SEEK_BAR_X_LIMIT_MIN - 30;
			}
			else if (seek_play == 2) {
				seek_pos_x = SEEK_BAR_X_LIMIT_MAX;
			}

			seek_pos_x = seek_pos_x < SEEK_BAR_X_LIMIT_MIN ? SEEK_BAR_X_LIMIT_MIN : seek_pos_x, seek_pos_x = seek_pos_x > SEEK_BAR_X_LIMIT_MAX ? SEEK_BAR_X_LIMIT_MAX : seek_pos_x;
			rect2.left = seek_pos_x;
			rect2.right = rect2.left + SEEK_BAR_WIDTH;
			ret_val = 1;

			// シークバー進捗赤
			rect4.left = SEEK_BAR_X_LIMIT_MIN;
			rect4.right = rect4.left + ((seek_pos_x - SEEK_BAR_X_LIMIT_MIN - 30) + SEEK_BAR_WIDTH);
			rect4.top = SEEK_BAR_Y_POS;
			rect4.bottom = rect4.top + SEEK_BAR_HEIGHT;

			for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
				for (LONGLONG x = rect4.left; x < rect4.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSeekBarSrcPixel2[PANEL_WIDTH * ((y - rect4.top) + (SEEK_BAR_Y_POS - PANEL_Y)) + (x - rect4.left + SEEK_BAR_X_LIMIT_MIN - PANEL_X)];
					}
				}
			}

			// シークバー残り灰
			rect4.left = seek_pos_x + 30;
			rect4.right = SEEK_BAR_X_LIMIT_MAX;
			rect4.top = SEEK_BAR_Y_POS;
			rect4.bottom = rect4.top + SEEK_BAR_HEIGHT;

			for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
				for (LONGLONG x = rect4.left; x < rect4.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSeekBarSrcPixel[PANEL_WIDTH * ((y - rect4.top) + (SEEK_BAR_Y_POS - PANEL_Y)) + (x - rect4.left + SEEK_BAR_X_LIMIT_MIN - PANEL_X)];
					}
				}
			}

			mmtp_current_time = mmtp_duration_time * (DOUBLE)(seek_pos_x - SEEK_BAR_X_LIMIT_MIN) / (DOUBLE)(SEEK_BAR_X_LIMIT_MAX - SEEK_BAR_X_LIMIT_MIN);

			rest_time = mmtp_duration_time - mmtp_current_time;

			for (long y = rect2.top; y < rect2.bottom; y++) {
				for (long x = rect2.left; x < rect2.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {

						pPixel = &p[pixel_addr];

						bgA = (*pPixel >> 24) & 0xFF;
						bgR = (*pPixel >> 16) & 0xFF;
						bgG = (*pPixel >> 8) & 0xFF;
						bgB = (*pPixel >> 0) & 0xFF;

						fgA = (*pImageSeekPointSrcPixel >> 24) & 0xFF;
						fgR = (*pImageSeekPointSrcPixel >> 16) & 0xFF;
						fgG = (*pImageSeekPointSrcPixel >> 8) & 0xFF;
						fgB = (*pImageSeekPointSrcPixel >> 0) & 0xFF;

						alpha_ratio = (DOUBLE)fgA / 255.0;

						tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
						tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
						tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
						tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

						*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);

						pImageSeekPointSrcPixel++;
					}
				}
			}

			goto DISPLAY_FONT;
		}
		else if ((posX >= SEEK_BAR_X_LIMIT_MIN) && (posX <= (SEEK_BAR_X_LIMIT_MAX + 40)) && (posY >= rect2.top) && (posY <= rect2.bottom)) {
			//			OutputDebug(L"Jump SeekBar\n");
			hit = 1;
			for (LONGLONG y = rect2.top; y < rect2.bottom; y++) {
				for (LONGLONG x = rect2.left; x < rect2.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSeekBarSrcPixel[2560 * (rect3.top + y - rect2.top) + rect3.left + (x - rect2.left)];
					}
				}
			}

			seek_pos_x = posX - 30LL;
			seek_pos_x = seek_pos_x < SEEK_BAR_X_LIMIT_MIN ? SEEK_BAR_X_LIMIT_MIN : seek_pos_x, seek_pos_x = seek_pos_x > SEEK_BAR_X_LIMIT_MAX ? SEEK_BAR_X_LIMIT_MAX : seek_pos_x;
			rect2.left = seek_pos_x;
			rect2.right = rect2.left + SEEK_BAR_WIDTH;
			ret_val = 1;


			// シークバー進捗赤
			rect4.left = SEEK_BAR_X_LIMIT_MIN;
			rect4.right = rect4.left + ((seek_pos_x - SEEK_BAR_X_LIMIT_MIN - 30) + SEEK_BAR_WIDTH);
			rect4.top = SEEK_BAR_Y_POS;
			rect4.bottom = rect4.top + SEEK_BAR_HEIGHT;

			for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
				for (LONGLONG x = rect4.left; x < rect4.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSeekBarSrcPixel2[PANEL_WIDTH * ((y - rect4.top) + (SEEK_BAR_Y_POS - PANEL_Y)) + (x - rect4.left + SEEK_BAR_X_LIMIT_MIN - PANEL_X)];
					}
				}
			}

			// シークバー残り灰
			rect4.left = seek_pos_x + 30;
			rect4.right = SEEK_BAR_X_LIMIT_MAX;
			rect4.top = SEEK_BAR_Y_POS;
			rect4.bottom = rect4.top + SEEK_BAR_HEIGHT;

			for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
				for (LONGLONG x = rect4.left; x < rect4.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSeekBarSrcPixel[PANEL_WIDTH * ((y - rect4.top) + (SEEK_BAR_Y_POS - PANEL_Y)) + (x - rect4.left + SEEK_BAR_X_LIMIT_MIN - PANEL_X)];
					}
				}
			}


			mmtp_current_time = mmtp_duration_time * (DOUBLE)(seek_pos_x - SEEK_BAR_X_LIMIT_MIN) / (DOUBLE)(SEEK_BAR_X_LIMIT_MAX - SEEK_BAR_X_LIMIT_MIN);

			rest_time = mmtp_duration_time - mmtp_current_time;

			for (long y = rect2.top; y < rect2.bottom; y++) {
				for (long x = rect2.left; x < rect2.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {

						pPixel = &p[pixel_addr];

						bgA = (*pPixel >> 24) & 0xFF;
						bgR = (*pPixel >> 16) & 0xFF;
						bgG = (*pPixel >> 8) & 0xFF;
						bgB = (*pPixel >> 0) & 0xFF;

						fgA = (*pImageSeekPointSrcPixel >> 24) & 0xFF;
						fgR = (*pImageSeekPointSrcPixel >> 16) & 0xFF;
						fgG = (*pImageSeekPointSrcPixel >> 8) & 0xFF;
						fgB = (*pImageSeekPointSrcPixel >> 0) & 0xFF;

						alpha_ratio = (DOUBLE)fgA / 255.0;

						tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
						tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
						tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
						tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

						*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);

						pImageSeekPointSrcPixel++;
					}
				}
			}

			goto DISPLAY_FONT;

		}
		else if ((posX >= PLAY_ICON_X_POS) && (posX <= (PLAY_ICON_X_POS + PLAY_ICON_WIDTH)) && (posY >= PLAY_ICON_Y_POS) && (posY <= (PLAY_ICON_Y_POS + PLAY_ICON_HEIGHT))) {
			if (play_hit == 0) {
				file_seek.QuadPart = 0;
				SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_CURRENT);

				if ((rest_time <= 0) || (file_seek_current.QuadPart >= file_seek_end.QuadPart)) {
					hit = 1;
					play_hit = 1;
					seek_play = 1;
					return 1;
				}

				//OutputDebug(L"Hit Play Icon\n");

				play ^= 1;

				if (play == 0) {
					tlv->disbale_XmlSubUpdate_notify = TRUE;

					long now_time = elapsed;

					while (now_time == elapsed) {
						Sleep(10);
					}

					if (timer) {
						timeKillEvent(timer);
						timer = 0;
					}
					pMediaControl->Pause();


				}
				else {
					pMediaControl->Run();
					if (!timer) {
						if ((timer = timeSetEvent(1000, 0, (LPTIMECALLBACK)Timer1HzCallBack, (DWORD)NULL, TIME_PERIODIC)) == 0) {
							printf("time event fail\n");
						}
					}
					tlv->disbale_XmlSubUpdate_notify = FALSE;
				}

				UINT32* pImageSrc = play ? pImageSeekBarSrcPixel2 : pImageSeekBarSrcPixel;

				// プレイアイコン
				rect4.left = PLAY_ICON_X_POS;
				rect4.right = rect4.left + PLAY_ICON_WIDTH;
				rect4.top = PLAY_ICON_Y_POS;
				rect4.bottom = rect4.top + PLAY_ICON_HEIGHT;

				for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
					for (LONGLONG x = rect4.left; x < rect4.right; x++) {
						pixel_addr = (2159LL - y) * 3840LL + x;
						if (pixel_addr < (3840 * 2160)) {
							p[pixel_addr] = pImageSrc[PANEL_WIDTH * ((y - rect4.top) + (PLAY_ICON_Y_POS - PANEL_Y)) + (x - rect4.left + PLAY_ICON_X_POS - PANEL_X)];
						}
					}
				}

				play_hit = 1;
				ret_val = 1;

				if ((pMadVROsdServices != NULL) && ui_screen) {
					pMadVROsdServices->OsdSetBitmap("ui", bitmap_ui, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
				}

				return ret_val;
			}
			else {
				ret_val = 1;
				return ret_val;
			}
		}
		else if ((posX >= PREV_ICON_X_POS) && (posX <= (PREV_ICON_X_POS + PREV_ICON_WIDTH)) && (posY >= PREV_ICON_Y_POS) && (posY <= (PREV_ICON_Y_POS + PREV_ICON_HEIGHT))) {
			hit = 1;
			play_hit = 1;
			seek_play = 1;
			return ret_val;
		}
		else if ((posX >= NEXT_ICON_X_POS) && (posX <= (NEXT_ICON_X_POS + NEXT_ICON_WIDTH)) && (posY >= NEXT_ICON_Y_POS) && (posY <= (NEXT_ICON_Y_POS + NEXT_ICON_HEIGHT))) {
			hit = 1;
			play_hit = 1;
			seek_play = 2;
			return ret_val;
		}
		else {
			return ret_val;
		}

	}
	else if (command == COMMAND_MANIPULATE_UI_LBUTTON_RELEASE) {
		play_hit = 0;

		if (hit == 1) {
		SEEK_PLAY:
			hit = 0;

			if (seek_play == 1) {
				back_to_seek_play = 1;
				goto SILIDE_SEEKBAR;
			}
			else if (seek_play == 2) {
				back_to_seek_play = 1;
				goto SILIDE_SEEKBAR;
			}
		BACK_TO_SEEK_PLAY:
			seek_play = 0;

			if (pBasicAudio != NULL) {
				pBasicAudio->put_Volume(-10000);
			}

			if (pMediaControl != NULL) {
				pMediaControl->Stop();
			}

			if (timerMMTP) {
				timeKillEvent(timerMMTP);
				timerMMTP = 0;
			}
			if (timer) {
				timeKillEvent(timer);
				timer = 0;
			}

			pGraphBuilder->RemoveFilter(pSplitterVideoSrc);
			pGraphBuilder->RemoveFilter(pSplitterAudioSrc);

			pSplitterVideoSrc = NULL;
			pSplitterAudioSrc = NULL;
			SAFE_DELETE(pSplitterVideo);
			SAFE_DELETE(pSplitterAudio);

			pGraphBuilder->RemoveFilter(pLAVVideoDecoder);
			pGraphBuilder->RemoveFilter(pLAVAudioDecoder);

			pLAVVideoDecoder = NULL;
			pLAVAudioDecoder = NULL;

			HRESULT hr;
			pSplitterVideo = new CLAVSplitter(NULL, &hr);
			pSplitterVideo->read_buffer_size = LAV_VIDEO_READ_SIZE;
			if (FAILED(hr = pGraphBuilder->AddFilter(pSplitterVideo, L"Video Splitter"))) {
				MessageBox(NULL, L"GraphBuilder AddFilter pSplitterVideo failed", L"Error", MB_OK | MB_TOPMOST);
				return E_FAIL;
			}

			if (FAILED(hr = pSplitterVideo->QueryInterface(__uuidof(IBaseFilter), (LPVOID*)&pSplitterVideoSrc))) {
				MessageBox(NULL, L"pSplitterVideoSrc QI from pSplitterVideo failed", L"Error", MB_OK | MB_TOPMOST);
				return E_FAIL;
			}

			pSplitterAudio = new CLAVSplitter(NULL, &hr);
			pSplitterAudio->read_buffer_size = LAV_AUDIO_READ_SIZE;
			if (FAILED(hr = pGraphBuilder->AddFilter(pSplitterAudio, L"Audio Splitter"))) {
				MessageBox(NULL, L"GraphBuilder AddFilter pSplitterAudio failed", L"Error", MB_OK | MB_TOPMOST);
				return E_FAIL;
			}

			if (FAILED(hr = pSplitterAudio->QueryInterface(__uuidof(IBaseFilter), (LPVOID*)&pSplitterAudioSrc))) {
				MessageBox(NULL, L"pSplitterAudioSrc QI from pSplitterAudio failed", L"Error", MB_OK | MB_TOPMOST);
				return E_FAIL;
			}

			file_seek.QuadPart = (file_seek_last_TOT.QuadPart - file_seek_first_TOT.QuadPart) * (DOUBLE)(seek_pos_x - SEEK_BAR_X_LIMIT_MIN) / (DOUBLE)(SEEK_BAR_X_LIMIT_MAX - SEEK_BAR_X_LIMIT_MIN);
			SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_BEGIN);

			bf = 0;
			tlv->Initialize();
			tlv->uiCID = cid_MMTP;
			tlv->disbale_XmlSubUpdate_notify = TRUE;
			tlv->InitializeSubtitle();
			video_frame_buffer->Initialize();
			audio_frame_buffer->Initialize();

			HANDLE thread = NULL;
			thread = CreateThread(NULL, 0, ThreadMMTPStarCaptureGraphSeek, NULL, 0, &threadID);

			if ((timerMMTP = timeSetEvent(5, 0, (LPTIMECALLBACK)TimerMMTPCallBack, (DWORD)NULL, TIME_PERIODIC)) == 0) {
				OutputDebug(L"time event fail\n");
			}

			// プレイアイコン再生中(一時停止)
			rect4.left = PLAY_ICON_X_POS;
			rect4.right = rect4.left + PLAY_ICON_WIDTH;
			rect4.top = PLAY_ICON_Y_POS;
			rect4.bottom = rect4.top + PLAY_ICON_HEIGHT;

			for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
				for (LONGLONG x = rect4.left; x < rect4.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSeekBarSrcPixel2[PANEL_WIDTH * ((y - rect4.top) + (PLAY_ICON_Y_POS - PANEL_Y)) + (x - rect4.left + PLAY_ICON_X_POS - PANEL_X)];
					}
				}
			}

			hit = 0;
			play = 1;
			play_hit = 0;
			manipulate_lock = 1;
		}
		return ret_val;
	}
	else if (command == COMMAND_MANIPULATE_UI_CURRENT_TIME) {
		for (LONGLONG y = rect2.top; y < rect2.bottom; y++) {
			for (LONGLONG x = rect2.left; x < rect2.right; x++) {
				pixel_addr = (2159LL - y) * 3840LL + x;
				if (pixel_addr < (3840 * 2160)) {
					p[pixel_addr] = pImageSeekBarSrcPixel[2560 * (rect3.top + y - rect2.top) + rect3.left + (x - rect2.left)];
				}
			}
		}

		LONGLONG current_pos_x = (SEEK_BAR_X_LIMIT_MAX - SEEK_BAR_X_LIMIT_MIN) * (DOUBLE)mmtp_current_time / (DOUBLE)mmtp_duration_time + SEEK_BAR_X_LIMIT_MIN;
		if (seek_pos_x < current_pos_x) {
			seek_pos_x = current_pos_x;
		}
		//		seek_pos_x = (SEEK_BAR_X_LIMIT_MAX - SEEK_BAR_X_LIMIT_MIN) * mmtp_play_process_rate + SEEK_BAR_X_LIMIT_MIN;
		seek_pos_x = seek_pos_x < SEEK_BAR_X_LIMIT_MIN ? SEEK_BAR_X_LIMIT_MIN : seek_pos_x, seek_pos_x = seek_pos_x > SEEK_BAR_X_LIMIT_MAX ? SEEK_BAR_X_LIMIT_MAX : seek_pos_x;
		rect2.left = seek_pos_x;
		rect2.right = rect2.left + SEEK_BAR_WIDTH;

		// シークバー進捗赤
		rect4.left = SEEK_BAR_X_LIMIT_MIN;
		rect4.right = rect4.left + ((seek_pos_x - SEEK_BAR_X_LIMIT_MIN - 30) + SEEK_BAR_WIDTH);
		rect4.top = SEEK_BAR_Y_POS;
		rect4.bottom = rect4.top + SEEK_BAR_HEIGHT;

		for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
			for (LONGLONG x = rect4.left; x < rect4.right; x++) {
				pixel_addr = (2159LL - y) * 3840LL + x;
				if (pixel_addr < (3840 * 2160)) {
					p[pixel_addr] = pImageSeekBarSrcPixel2[PANEL_WIDTH * ((y - rect4.top) + (SEEK_BAR_Y_POS - PANEL_Y)) + (x - rect4.left + SEEK_BAR_X_LIMIT_MIN - PANEL_X)];
				}
			}
		}

		for (LONGLONG y = rect2.top; y < rect2.bottom; y++) {
			for (LONGLONG x = rect2.left; x < rect2.right; x++) {
				pixel_addr = (2159LL - y) * 3840LL + x;
				if (pixel_addr < (3840 * 2160)) {

					pPixel = &p[pixel_addr];

					bgA = (*pPixel >> 24) & 0xFF;
					bgR = (*pPixel >> 16) & 0xFF;
					bgG = (*pPixel >> 8) & 0xFF;
					bgB = (*pPixel >> 0) & 0xFF;

					fgA = (*pImageSeekPointSrcPixel >> 24) & 0xFF;
					fgR = (*pImageSeekPointSrcPixel >> 16) & 0xFF;
					fgG = (*pImageSeekPointSrcPixel >> 8) & 0xFF;
					fgB = (*pImageSeekPointSrcPixel >> 0) & 0xFF;

					alpha_ratio = (DOUBLE)fgA / 255.0;

					tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
					tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
					tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
					tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

					*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);

					pImageSeekPointSrcPixel++;
				}
			}
		}

		if (hit == 1) {
			return ret_val;
		}

		rest_time = mmtp_duration_time - ++mmtp_current_time;

		file_seek.QuadPart = 0;
		SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_CURRENT);
		if ((rest_time < 0) || (file_seek_current.QuadPart >= file_seek_end.QuadPart)) {

			rest_time = 0;
			mmtp_current_time = mmtp_duration_time;

			play_hit = 0;
			hit = 0;
			play = 0;
			ui_screen = 1;

			// プレイアイコン一時停止
			rect4.left = PLAY_ICON_X_POS;
			rect4.right = rect4.left + PLAY_ICON_WIDTH;
			rect4.top = PLAY_ICON_Y_POS;
			rect4.bottom = rect4.top + PLAY_ICON_HEIGHT;

			if (timer) {
				timeKillEvent(timer);
				timer = 0;
			}

			pMediaControl->Pause();

			UINT32* pImageSrc = play ? pImageSeekBarSrcPixel2 : pImageSeekBarSrcPixel;

			for (LONGLONG y = rect4.top; y < rect4.bottom; y++) {
				for (LONGLONG x = rect4.left; x < rect4.right; x++) {
					pixel_addr = (2159LL - y) * 3840LL + x;
					if (pixel_addr < (3840 * 2160)) {
						p[pixel_addr] = pImageSrc[PANEL_WIDTH * ((y - rect4.top) + (PLAY_ICON_Y_POS - PANEL_Y)) + (x - rect4.left + PLAY_ICON_X_POS - PANEL_X)];
					}
				}
			}
		}
	}

DISPLAY_FONT:
	FT_Glyph glyph = NULL;
	FT_BitmapGlyph bitmapGlyph;

	FT_Error ft_error;

	ULONGLONG fontSize_Width = 50;
	ULONGLONG textOutline_Width = 4;

	INT letter_spacing = -12;

	for (int i = 0; i < 2; i++) {
		INT spacing = 0;
		INT wordCount = 8;

		if (i == 0) {
			offsetX = ELAPSED_TIME_X;
			offsetY = ELAPSED_REST_TIME_Y;

			rect.left = PANEL_X;
			rect.top = PANEL_Y;

			rect2.left = offsetX;
			rect2.right = rect2.left + ELAPSED_BACK_WIDTH;
			rect2.top = offsetY - fontSize_Width;
			rect2.bottom = rect2.top + fontSize_Width + 10;

			rect3.left = rect2.left - rect.left;
			rect3.top = rect2.top - rect.top;
		}

		if (i == 1) {
			if (mmtp_duration_time >= 3600) {
				offsetX = REST_TIME_X1;
			}
			else {
				offsetX = REST_TIME_X2;
			}
			offsetY = ELAPSED_REST_TIME_Y;

			rect.left = PANEL_X;
			rect.top = PANEL_Y;

			rect2.left = offsetX;
			if (mmtp_duration_time >= 3600) {
				rect2.right = rect2.left + REST_BACK_WIDTH1;
			}
			else {
				rect2.right = rect2.left + REST_BACK_WIDTH2;
			}
			rect2.top = offsetY - fontSize_Width;
			rect2.bottom = rect2.top + fontSize_Width + 10;

			rect3.left = rect2.left - rect.left;
			rect3.top = rect2.top - rect.top;
		}

		for (LONGLONG y = rect2.top; y < rect2.bottom; y++) {
			for (LONGLONG x = rect2.left; x < rect2.right; x++) {
				pixel_addr = (2159LL - y) * 3840LL + x;
				if (pixel_addr < (3840 * 2160)) {
					p[pixel_addr] = pImageSeekBarSrcPixel[PANEL_WIDTH * (rect3.top + y - rect2.top) + rect3.left + (x - rect2.left)];
				}
			}
		}

		FT_Set_Pixel_Sizes(face2, fontSize_Width, fontSize_Width);

		//  2 * 64 result in 2px outline
		FT_Stroker_Set(stroker2, textOutline_Width * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

		WCHAR clock_text[10];
		memset(clock_text, 0, sizeof(clock_text));

		if (i == 0) {
			if (mmtp_duration_time >= 3600) {
				wsprintf(clock_text, L"%02d：%02d：%02d", mmtp_current_time / 3600, (mmtp_current_time / 60) % 60, mmtp_current_time % 60);
				wordCount = 8;
			}
			else {
				wsprintf(clock_text, L"%02d：%02d", (mmtp_current_time / 60) % 60, mmtp_current_time % 60);
				wordCount = 5;
			}
		}
		if (i == 1) {
			if (mmtp_duration_time >= 3600) {
				wsprintf(clock_text, L"－%02d：%02d：%02d", rest_time / 3600, (rest_time / 60) % 60, rest_time % 60);
				wordCount = 9;
			}
			else {
				wsprintf(clock_text, L"－%02d：%02d", (rest_time / 60) % 60, rest_time % 60);
				wordCount = 6;
			}
		}

		for (int i = 0; i < wordCount; i++) {
			ft_error = FT_Load_Glyph(face2, FT_Get_Char_Index(face2, clock_text[i]), FT_LOAD_DEFAULT | FT_LOAD_PEDANTIC);
			if (ft_error) {
				break;
			}
			ft_error = FT_Get_Glyph(face2->glyph, &glyph);
			if (ft_error) {
				FT_Done_Glyph(glyph);
				glyph = NULL;
				break;
			}
			ft_error = FT_Glyph_StrokeBorder(&glyph, stroker2, false, true);
			if (ft_error) {
				FT_Done_Glyph(glyph);
				glyph = NULL;
				break;
			}
			ft_error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
			if (ft_error) {
				FT_Done_Glyph(glyph);
				glyph = NULL;
				break;
			}
			bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
			bm = &bitmapGlyph->bitmap;

			fgR = 80;
			fgG = 80;
			fgB = 80;
			fgA = 255;

			ft_error = FT_Load_Glyph(face2, FT_Get_Char_Index(face2, clock_text[i]), FT_LOAD_RENDER);
			if (ft_error) {
				FT_Done_Glyph(glyph);
				glyph = NULL;
				break;
			}

			for (int row = 0; row < bm->rows; row++) {
				for (int col = 0; col < bm->pitch; col++) {
					glyph_pixel_val = bm->buffer[bm->pitch * (bm->rows - 1 - row) + col];
					pixel_addr = ((2159LL + row - offsetY - ((LONGLONG)face2->glyph->metrics.height - (LONGLONG)face2->glyph->metrics.horiBearingY) / 64LL) - textOutline_Width) * 3840LL \
						+ col + spacing + offsetX + ((fontSize_Width - face2->glyph->bitmap.width) / 2LL) - textOutline_Width;

					if (pixel_addr >= (3840 * 2160)) {
						break;
					}

					pPixel = &p[pixel_addr];

					bgA = (*pPixel >> 24) & 0xFF;
					bgR = (*pPixel >> 16) & 0xFF;
					bgG = (*pPixel >> 8) & 0xFF;
					bgB = (*pPixel >> 0) & 0xFF;

					alpha_ratio = (DOUBLE)glyph_pixel_val / 255.0;

					tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
					tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
					tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
					tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

					*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);
				}
			}

			bm = &face2->glyph->bitmap;

			fgR = 255;
			fgG = 255;
			fgB = 255;
			fgA = 255;

			for (int row = 0; row < bm->rows; row++) {
				for (int col = 0; col < bm->pitch; col++) {
					glyph_pixel_val = bm->buffer[bm->pitch * (bm->rows - 1 - row) + col];
					pixel_addr = (2159LL + row - offsetY - ((LONGLONG)face2->glyph->metrics.height - (LONGLONG)face2->glyph->metrics.horiBearingY) / 64LL) * 3840LL\
						+ col + spacing + offsetX + ((fontSize_Width - face2->glyph->bitmap.width) / 2LL);

					if (pixel_addr >= (3840 * 2160)) {
						break;
					}

					pPixel = &p[pixel_addr];

					bgA = (*pPixel >> 24) & 0xFF;
					bgR = (*pPixel >> 16) & 0xFF;
					bgG = (*pPixel >> 8) & 0xFF;
					bgB = (*pPixel >> 0) & 0xFF;

					alpha_ratio = (DOUBLE)glyph_pixel_val / 255.0;

					tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
					tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
					tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
					tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

					*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);
				}
			}
			spacing += fontSize_Width + letter_spacing;
			FT_Done_Glyph(glyph);
			glyph = NULL;
		}
	}

	if ((pMadVROsdServices != NULL) && ui_screen) {
		pMadVROsdServices->OsdSetBitmap("ui", bitmap_ui, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
	}

	if (back_to_seek_play) {
		goto BACK_TO_SEEK_PLAY;
	}

	return ret_val;
}

static void display_clock(INT command)
{
	CAutoLock lock_it(&m_ClockLock);

	//	memset(ppv_clock, 0, 3840 * 2160 * sizeof(UINT32));

	ULONGLONG pixel_addr;
	UINT32* p = (UINT32*)ppv_clock;
	UINT32* pPixel;
	INT offsetX;
	INT offsetY;
	BYTE glyph_pixel_val;
	BYTE fgA, fgR, fgG, fgB;
	BYTE bgA, bgR, bgG, bgB;
	BYTE tA, tR, tG, tB;
	DOUBLE alpha_ratio;
	FT_Bitmap* bm;

	RECT rect = {};
	rect.left = 100;
	rect.right = rect.left + 500;
	rect.top = 50;
	rect.bottom = rect.top + 180;

	for (long y = rect.top; y < rect.bottom; y++) {
		for (long x = rect.left; x < rect.right; x++) {
			pixel_addr = (2159LL - y) * 3840LL + x;
			if (pixel_addr < (3840 * 2160)) {
				*(p + pixel_addr) = 0x00000000;
			}
		}
	}

	if (command == 0) {
		if (pMadVROsdServices != NULL) {
			pMadVROsdServices->OsdSetBitmap("clock", bitmap_clock, 0, 0, 0, 0, TRUE, 1, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
		}
		return;
	}

	FT_Glyph glyph = NULL;
	FT_BitmapGlyph bitmapGlyph;

	FT_Error ft_error;

	ULONGLONG fontSize_Width = 110;
	ULONGLONG textOutline_Width = 6;

	INT letter_spacing = -10;
	INT spacing = 0;

	offsetX = 88;
	offsetY = 174;

	FT_Set_Pixel_Sizes(face, fontSize_Width, fontSize_Width);

	//  2 * 64 result in 2px outline
	FT_Stroker_Set(stroker, textOutline_Width * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

	WCHAR clock_text[10];
	memset(clock_text, 0, sizeof(clock_text));

	wsprintf(clock_text, L"%2d：%02d", g_hour, g_minute);

	for (int i = 0; i < 5; i++) {
		ft_error = FT_Load_Glyph(face, FT_Get_Char_Index(face, clock_text[i]), FT_LOAD_DEFAULT | FT_LOAD_PEDANTIC);
		if (ft_error) {
			break;
		}
		ft_error = FT_Get_Glyph(face->glyph, &glyph);
		if (ft_error) {
			FT_Done_Glyph(glyph);
			glyph = NULL;
			break;
		}
		ft_error = FT_Glyph_StrokeBorder(&glyph, stroker, false, true);
		if (ft_error) {
			FT_Done_Glyph(glyph);
			glyph = NULL;
			break;
		}
		ft_error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
		if (ft_error) {
			FT_Done_Glyph(glyph);
			glyph = NULL;
			break;
		}
		bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
		bm = &bitmapGlyph->bitmap;

		fgR = 0;
		fgG = 0;
		fgB = 0;
		fgA = 255;

		ft_error = FT_Load_Glyph(face, FT_Get_Char_Index(face, clock_text[i]), FT_LOAD_RENDER);
		if (ft_error) {
			FT_Done_Glyph(glyph);
			glyph = NULL;
			break;
		}

		for (int row = 0; row < bm->rows; row++) {
			for (int col = 0; col < bm->pitch; col++) {
				glyph_pixel_val = bm->buffer[bm->pitch * (bm->rows - 1 - row) + col];
				pixel_addr = ((2159LL + row - offsetY - ((LONGLONG)face->glyph->metrics.height - (LONGLONG)face->glyph->metrics.horiBearingY) / 64LL) - textOutline_Width) * 3840LL \
					+ col + spacing + offsetX + ((fontSize_Width - face->glyph->bitmap.width) / 2LL) - textOutline_Width;

				if (pixel_addr >= (3840 * 2160)) {
					break;
				}

				pPixel = &p[pixel_addr];

				bgA = (*pPixel >> 24) & 0xFF;
				bgR = (*pPixel >> 16) & 0xFF;
				bgG = (*pPixel >> 8) & 0xFF;
				bgB = (*pPixel >> 0) & 0xFF;

				alpha_ratio = (DOUBLE)glyph_pixel_val / 255.0;

				tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
				tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
				tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
				tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

				*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);
			}
		}

		bm = &face->glyph->bitmap;

		fgR = 255;
		fgG = 255;
		fgB = 255;
		fgA = 255;

		for (int row = 0; row < bm->rows; row++) {
			for (int col = 0; col < bm->pitch; col++) {
				glyph_pixel_val = bm->buffer[bm->pitch * (bm->rows - 1 - row) + col];
				pixel_addr = (2159LL + row - offsetY - ((LONGLONG)face->glyph->metrics.height - (LONGLONG)face->glyph->metrics.horiBearingY) / 64LL) * 3840LL\
					+ col + spacing + offsetX + ((fontSize_Width - face->glyph->bitmap.width) / 2LL);

				if (pixel_addr >= (3840 * 2160)) {
					break;
				}

				pPixel = &p[pixel_addr];

				bgA = (*pPixel >> 24) & 0xFF;
				bgR = (*pPixel >> 16) & 0xFF;
				bgG = (*pPixel >> 8) & 0xFF;
				bgB = (*pPixel >> 0) & 0xFF;

				alpha_ratio = (DOUBLE)glyph_pixel_val / 255.0;

				tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
				tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
				tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
				tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

				*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);
			}
		}
		spacing += fontSize_Width + letter_spacing;
		FT_Done_Glyph(glyph);
		glyph = NULL;
	}

	if (pMadVROsdServices != NULL) {
		pMadVROsdServices->OsdSetBitmap("clock", bitmap_clock, 0, 0, 0, 0, TRUE, 1, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
	}
}

static void CALLBACK TimerThreadSubtitleCallBack(unsigned int timerID, unsigned int msg, unsigned int usrParam, unsigned int dw1, unsigned int dw2)
{
	CAutoLock lock_it(&m_SubtitleLock);

	memset(ppv_sub, 0, 3840 * 2160 * sizeof(UINT32)); // 字幕フレームバッファ消去

	UINT16 xml_id = *(UINT16*)tlv->lpXmlSubBuf[tlv->uiXmlSubBufReadPos];
	UINT16 xml_size = *(UINT16*)&tlv->lpXmlSubBuf[tlv->uiXmlSubBufReadPos][5];
	LPBYTE pXml_mem = &tlv->lpXmlSubBuf[tlv->uiXmlSubBufReadPos][7];

	if (++tlv->uiXmlSubBufReadPos >= XML_SUB_BUFFER_NUM) {
		tlv->uiXmlSubBufReadPos = 0;
	}
	//OutputDebug(L"xml_id:%04X\n", xml_id);
	//OutputDebug(L"XmlUpdate:%02X %d %d\n", xml_id, xml_size, tlv->ulXmlSubBufSize);

	tlv->XmlSubUpdate = FALSE;

	IStream* pStream = SHCreateMemStream(pXml_mem, xml_size);
	if (pStream == NULL) {
		timeKillEvent(timerID);
		return;
	}

	IXmlReader* pReader = NULL;
	if (FAILED(CreateXmlReader(__uuidof(IXmlReader), (void**)&pReader, NULL))) {
		SAFE_RELEASE(pStream);
		timeKillEvent(timerID);
		return;
	}

	if (FAILED(pReader->SetInput(pStream))) {
		SAFE_RELEASE(pStream);
		SAFE_RELEASE(pReader);
		timeKillEvent(timerID);
		return;
	}

	typedef struct {
		CString id;
		CString backgroundColor;
		CString color;
		CString fontFamily;
		CString fontSize;
		CString fontStyle;
		CString fontWeight;
		CString lineHeight;
		CString textDecoration;
		CString textOutline;
		CString writingMode;
		CString animation;
		CString border;
		CString border_top;
		CString border_bottom;
		CString border_left;
		CString border_right;
		CString letter_spacing;
		CString ruby;
		CString text_shadow;
	}sub_style_Typedef;

	typedef struct {
		UINT32 backgroundColor;
		UINT32 color;
		CString fontFamily;
		UINT fontSize_Width;
		UINT fontSize_Height;
		BOOL fontStyle;
		BOOL fontWeight;
		UINT lineHeight;
		BOOL textDecoration;
		UINT32 textOutline_Color;
		UINT textOutline_Width;
		UINT textOutline_Blur;
		BOOL writingMode;
		CString animation;
		CString border;
		CString border_top;
		CString border_bottom;
		CString border_left;
		CString border_right;
		UINT letter_spacing;
		CString ruby;
		CString text_shadow;
	}sub_style_val_Typedef;

	typedef struct {
		CString id;
		CString extent;
		CString origin;
	}sub_region_Typedef;

	typedef struct {
		UINT extent_Width;
		UINT extent_Height;
		UINT origin_X;
		UINT origin_Y;
	}sub_region_val_Typedef;

#define XM_SUB_ELEMENTS 50

	sub_style_Typedef sub_style[XM_SUB_ELEMENTS];
	sub_region_Typedef sub_region[XM_SUB_ELEMENTS];

	sub_style_val_Typedef  style_val = { 0, 0, L"", 0, 0, FALSE, FALSE, 0, FALSE, 0, 0, 0, FALSE, L"", L"", L"", L"", L"", L"", 0, L"", L"" };
	sub_region_val_Typedef region_val = { 0, 0, 0, 0 };

	LPCWSTR pwszLocalName;
	LPCWSTR pwszValue;
	XmlNodeType nodeType;
	HRESULT hr;
	INT num = 0;
	INT style_index = -1;
	INT region_index = -1;
	INT attribute_count_loop_limit;
	INT spacing = 0;

	ULONGLONG pixel_addr;
	UINT32* p = (UINT32*)ppv_sub;
	UINT32* pPixel;
	INT offsetX;
	INT offsetY;
	BYTE glyph_pixel_val;
	BYTE fgA, fgR, fgG, fgB;
	BYTE bgA, bgR, bgG, bgB;
	BYTE tA, tR, tG, tB;
	DOUBLE alpha_ratio;
	FT_Bitmap* bm;
	UINT stringCount;

	LPCWSTR pwszAttributeName;
	LPCWSTR pwszAttributeValue;

	FT_Glyph glyph = NULL;
	FT_BitmapGlyph bitmapGlyph;

	RECT rect = {};

	CString var1, var2, var3;
	TCHAR* e;
	INT sp;

	FT_Error ft_error;

	while (S_OK == pReader->Read(&nodeType)) {
		switch (nodeType) {
		case XmlNodeType_Element:
			if (FAILED(pReader->GetLocalName(&pwszLocalName, NULL))) {
				break;
			}

			hr = pReader->MoveToFirstAttribute();

			// 属性が無い
			if (S_FALSE == hr) {
				break;
			}

			if (S_OK != hr) {
				break;
			}

			if (lstrcmp(pwszLocalName, _T("style")) == 0) {
				if (++style_index >= XM_SUB_ELEMENTS) {
					style_index = XM_SUB_ELEMENTS - 1;
				}
			}
			else if (lstrcmp(pwszLocalName, _T("region")) == 0) {
				if (++region_index >= XM_SUB_ELEMENTS) {
					region_index = XM_SUB_ELEMENTS - 1;
				}
			}

			attribute_count_loop_limit = 50;
			do {
				if (FAILED(pReader->GetLocalName(&pwszAttributeName, NULL))) {
					break;
				}
				if (FAILED(pReader->GetValue(&pwszAttributeValue, NULL))) {
					break;
				}
				//				OutputDebug(L"%d %s %s %s\n", num, pwszLocalName, pwszAttributeName, pwszAttributeValue);

				if (lstrcmp(pwszLocalName, _T("style")) == 0) {
					if (style_index < 0) {
						style_index = 0;
					}
					if (lstrcmp(pwszAttributeName, _T("id")) == 0) {
						sub_style[style_index].id = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("backgroundColor")) == 0) {
						sub_style[style_index].backgroundColor = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("color")) == 0) {
						sub_style[style_index].color = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("fontFamily")) == 0) {
						sub_style[style_index].fontFamily = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("fontSize")) == 0) {
						sub_style[style_index].fontSize = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("fontStyle")) == 0) {
						sub_style[style_index].fontStyle = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("fontWeight")) == 0) {
						sub_style[style_index].fontWeight = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("lineHeight")) == 0) {
						sub_style[style_index].lineHeight = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("textDecoration")) == 0) {
						sub_style[style_index].textDecoration = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("textOutline")) == 0) {
						sub_style[style_index].textOutline = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("writingMode")) == 0) {
						sub_style[style_index].writingMode = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("animation")) == 0) {
						sub_style[style_index].animation = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("border")) == 0) {
						sub_style[style_index].border = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("border-top")) == 0) {
						sub_style[style_index].border_top = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("border-bottom")) == 0) {
						sub_style[style_index].border_bottom = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("border-left")) == 0) {
						sub_style[style_index].border_left = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("border-right")) == 0) {
						sub_style[style_index].border_right = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("letter-spacing")) == 0) {
						sub_style[style_index].letter_spacing = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("ruby")) == 0) {
						sub_style[style_index].ruby = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("text_shadow")) == 0) {
						sub_style[style_index].text_shadow = pwszAttributeValue;
					}
				}
				else if (lstrcmp(pwszLocalName, _T("region")) == 0) {
					if (region_index < 0) {
						region_index = 0;
					}
					if (lstrcmp(pwszAttributeName, _T("id")) == 0) {
						sub_region[region_index].id = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("extent")) == 0) {
						sub_region[region_index].extent = pwszAttributeValue;
					}
					else if (lstrcmp(pwszAttributeName, _T("origin")) == 0) {
						sub_region[region_index].origin = pwszAttributeValue;
					}
				}
				else if (lstrcmp(pwszLocalName, _T("p")) == 0) {
					spacing = 0;
					if (lstrcmp(pwszAttributeName, _T("region")) == 0) {
						for (int i = 0; i <= region_index; i++) {
							if (lstrcmp(pwszAttributeValue, sub_region[i].id) == 0) {
								if (!sub_region[i].extent.IsEmpty()) {
									sp = sub_region[i].extent.Find(' ', 0);
									var1 = sub_region[i].extent.Mid(0, sp);
									var2 = sub_region[i].extent.Mid(sp + 1);
									var1.Replace(_T("px"), _T(""));
									var2.Replace(_T("px"), _T(""));
									region_val.extent_Width = _ttoi(var1);
									region_val.extent_Height = _ttoi(var2);
								}
								else {
									region_val.extent_Width = 0;
									region_val.extent_Height = 0;
								}

								if (!sub_region[i].origin.IsEmpty()) {
									sp = sub_region[i].origin.Find(' ', 0);
									var1 = sub_region[i].origin.Mid(0, sp);
									var2 = sub_region[i].origin.Mid(sp + 1);
									var1.Replace(_T("px"), _T(""));
									var2.Replace(_T("px"), _T(""));
									region_val.origin_X = _ttoi(var1);
									region_val.origin_Y = _ttoi(var2);
								}
								else {
									region_val.origin_X = 0;
									region_val.origin_Y = 0;
								}

								rect.left = region_val.origin_X;
								rect.right = rect.left + region_val.extent_Width;
								rect.top = region_val.origin_Y;
								rect.bottom = rect.top + region_val.extent_Height;
								for (long y = rect.top; y < rect.bottom; y++) {
									for (long x = rect.left; x < rect.right; x++) {
										pixel_addr = (2159LL - y) * 3840LL + x;
										if (pixel_addr < (3840 * 2160)) {
											*(p + pixel_addr) = 0x80000000;// style_val.backgroundColor;
										}
									}
								}

								//								OutputDebug(L"p region_index:%d id:%s extWidth:%d extHeight:%d origin_X:%d origin_Y:%d\n", i, sub_region[i].id, region_val.extent_Width, region_val.extent_Height, region_val.origin_X, region_val.origin_Y);
								break;
							}
						}
					}
				}
				else if (lstrcmp(pwszLocalName, _T("span")) == 0) {
					if (lstrcmp(pwszAttributeName, _T("style")) == 0) {
						for (int i = 0; i <= style_index; i++) {
							if (lstrcmp(pwszAttributeValue, sub_style[i].id) == 0) {
								// backgroundColor
								if (!sub_style[i].backgroundColor.IsEmpty()) {
									var1 = sub_style[i].backgroundColor;
									var1.Replace(_T("#"), _T("0x"));
									style_val.backgroundColor = _tcstoul(var1, &e, 16);
								}
								else {
									style_val.backgroundColor = 0x00000080;
								}

								// color
								if (!sub_style[i].color.IsEmpty()) {
									var1 = sub_style[i].color;
									var1.Replace(_T("#"), _T("0x"));
									style_val.color = _tcstoul(var1, &e, 16);
								}
								else {
									style_val.color = 0xFFFFFFFF;
								}

								// fontSize
								if (!sub_style[i].fontSize.IsEmpty()) {
									sp = sub_style[i].fontSize.Find(' ', 0);
									var1 = sub_style[i].fontSize.Mid(0, sp);
									var2 = sub_style[i].fontSize.Mid(sp + 1);
									var1.Replace(_T("px"), _T(""));
									var2.Replace(_T("px"), _T(""));
									style_val.fontSize_Width = _ttoi(var1);
									style_val.fontSize_Height = _ttoi(var2);
								}
								else {
									style_val.fontSize_Width = 144;
									style_val.fontSize_Height = 144;
								}

								// fontStyle
								if (lstrcmp(sub_style[i].fontStyle, _T("italic")) == 0) {
									style_val.fontStyle = TRUE;
								}
								else {
									style_val.fontStyle = FALSE;
								}

								// fontWeight
								if (lstrcmp(sub_style[i].fontWeight, _T("bold")) == 0) {
									style_val.fontWeight = TRUE;
								}
								else {
									style_val.fontWeight = FALSE;
								}

								// lineHeight
								if (!sub_style[i].lineHeight.IsEmpty()) {
									var1 = sub_style[i].lineHeight;
									var1.Replace(_T("px"), _T(""));
									style_val.lineHeight = _ttoi(var1);
								}
								else {
									style_val.lineHeight = 240;
								}

								// textDecoration
								if (lstrcmp(sub_style[i].textDecoration, _T("underline")) == 0) {
									style_val.textDecoration = TRUE;
								}
								else {
									style_val.textDecoration = FALSE;
								}

								// textOutline
								if (!sub_style[i].textOutline.IsEmpty()) {
									sp = sub_style[i].textOutline.Find(' ', 0);
									var1 = sub_style[i].textOutline.Mid(0, sp);
									var2 = sub_style[i].textOutline.Mid(sp + 1);
									sp = var2.Find(' ', 0);
									var2 = var2.Mid(0, sp);
									var3 = var2.Mid(sp + 1);
									var1.Replace(_T("#"), _T(""));
									var2.Replace(_T("px"), _T(""));
									var3.Replace(_T("px"), _T(""));
									style_val.textOutline_Color = _tcstoul(var1, &e, 16);
									style_val.textOutline_Width = _ttoi(var2);
									style_val.textOutline_Blur = _ttoi(var3);
								}
								else {
									style_val.textOutline_Color = 0x000000FF;
									style_val.textOutline_Width = 4;
									style_val.textOutline_Blur = 0;
								}

								// writingMode
								if (lstrcmp(sub_style[i].writingMode, _T("tbrl")) == 0) {
									style_val.writingMode = TRUE;
								}
								else {
									style_val.writingMode = FALSE;
								}

								// letter_spacing
								if (!sub_style[i].letter_spacing.IsEmpty()) {
									var1 = sub_style[i].letter_spacing;
									var1.Replace(_T("px"), _T(""));
									style_val.letter_spacing = _ttoi(var1);
								}
								else {
									style_val.letter_spacing = 16;
								}

								// ruby
								style_val.ruby = sub_style[i].ruby;

								//OutputDebug(L"span style_index:%d id:%s color:%08X fontSize_Width:%d fontSize_Height:%d lineHeight:%d letter_spacing:%d\n", i, sub_style[i].id, style_val.color, style_val.fontSize_Width, style_val.fontSize_Height, style_val.lineHeight, style_val.letter_spacing);
								break;
							}
						}
					}
				}
			} while ((S_OK == pReader->MoveToNextAttribute()) && (--attribute_count_loop_limit >= 0));
			++num;
			break;
		case XmlNodeType_Text:
			if (FAILED(pReader->GetValue(&pwszValue, &stringCount))) {
				break;
			}

			offsetX = region_val.origin_X + style_val.letter_spacing / 2;
			offsetY = region_val.origin_Y + (style_val.lineHeight - style_val.fontSize_Height / 2);

			FT_Set_Pixel_Sizes(face, style_val.fontSize_Width, style_val.fontSize_Height);

			//  2 * 64 result in 2px outline
			FT_Stroker_Set(stroker, style_val.textOutline_Width * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

			for (int i = 0; i < stringCount; i++) {
				ft_error = FT_Load_Glyph(face, FT_Get_Char_Index(face, pwszValue[i]), FT_LOAD_DEFAULT | FT_LOAD_PEDANTIC);
				if (ft_error) {
					break;
				}
				ft_error = FT_Get_Glyph(face->glyph, &glyph);
				if (ft_error) {
					FT_Done_Glyph(glyph);
					glyph = NULL;
					break;
				}
				ft_error = FT_Glyph_StrokeBorder(&glyph, stroker, false, true);
				if (ft_error) {
					FT_Done_Glyph(glyph);
					glyph = NULL;
					break;
				}
				ft_error = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
				if (ft_error) {
					FT_Done_Glyph(glyph);
					glyph = NULL;
					break;
				}
				bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
				bm = &bitmapGlyph->bitmap;

				fgR = (style_val.textOutline_Color >> 24) & 0xFF;
				fgG = (style_val.textOutline_Color >> 16) & 0xFF;
				fgB = (style_val.textOutline_Color >> 8) & 0xFF;
				fgA = (style_val.textOutline_Color >> 0) & 0xFF;

				ft_error = FT_Load_Glyph(face, FT_Get_Char_Index(face, pwszValue[i]), FT_LOAD_RENDER);
				if (ft_error) {
					FT_Done_Glyph(glyph);
					glyph = NULL;
					break;
				}

				for (int row = 0; row < bm->rows; row++) {
					for (int col = 0; col < bm->pitch; col++) {
						glyph_pixel_val = bm->buffer[bm->pitch * (bm->rows - 1 - row) + col];
						if (style_val.writingMode != TRUE) { // 横書き
							pixel_addr = ((2159LL + row - offsetY - ((LONGLONG)face->glyph->metrics.height - (LONGLONG)face->glyph->metrics.horiBearingY) / 64LL) - style_val.textOutline_Width) * 3840LL \
								+ col + spacing + offsetX + ((style_val.fontSize_Width - face->glyph->bitmap.width) / 2LL) - style_val.textOutline_Width;
						}
						else { // 縦書き
							pixel_addr = ((2159LL + row - spacing - offsetY - ((LONGLONG)face->glyph->metrics.height - (LONGLONG)face->glyph->metrics.horiBearingY) / 64LL) - style_val.textOutline_Width) * 3840LL \
								+ col + offsetX + ((style_val.fontSize_Width - face->glyph->bitmap.width) / 2LL) - style_val.textOutline_Width;
						}

						if (pixel_addr >= (3840 * 2160)) {
							break;
						}

						pPixel = &p[pixel_addr];

						bgA = (*pPixel >> 24) & 0xFF;
						bgR = (*pPixel >> 16) & 0xFF;
						bgG = (*pPixel >> 8) & 0xFF;
						bgB = (*pPixel >> 0) & 0xFF;

						alpha_ratio = (DOUBLE)glyph_pixel_val / 255.0;

						tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
						tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
						tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
						tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

						*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);
					}
				}

				bm = &face->glyph->bitmap;

				fgR = (style_val.color >> 24) & 0xFF;
				fgG = (style_val.color >> 16) & 0xFF;
				fgB = (style_val.color >> 8) & 0xFF;
				fgA = (style_val.color >> 0) & 0xFF;

				for (int row = 0; row < bm->rows; row++) {
					for (int col = 0; col < bm->pitch; col++) {
						glyph_pixel_val = bm->buffer[bm->pitch * (bm->rows - 1 - row) + col];
						if (style_val.writingMode != TRUE) { // 横書き
							pixel_addr = (2159LL + row - offsetY - ((LONGLONG)face->glyph->metrics.height - (LONGLONG)face->glyph->metrics.horiBearingY) / 64LL) * 3840LL\
								+ col + spacing + offsetX + ((style_val.fontSize_Width - face->glyph->bitmap.width) / 2LL);
						}
						else { // 縦書き
							pixel_addr = (2159LL + row - spacing - offsetY - ((LONGLONG)face->glyph->metrics.height - (LONGLONG)face->glyph->metrics.horiBearingY) / 64LL) * 3840LL\
								+ col + offsetX + ((style_val.fontSize_Width - face->glyph->bitmap.width) / 2LL);
						}

						if (pixel_addr >= (3840 * 2160)) {
							break;
						}

						pPixel = &p[pixel_addr];

						bgA = (*pPixel >> 24) & 0xFF;
						bgR = (*pPixel >> 16) & 0xFF;
						bgG = (*pPixel >> 8) & 0xFF;
						bgB = (*pPixel >> 0) & 0xFF;

						alpha_ratio = (DOUBLE)glyph_pixel_val / 255.0;

						tA = fgA * alpha_ratio + bgA * (1.0 - alpha_ratio);
						tR = fgR * alpha_ratio + bgR * (1.0 - alpha_ratio);
						tG = fgG * alpha_ratio + bgG * (1.0 - alpha_ratio);
						tB = fgB * alpha_ratio + bgB * (1.0 - alpha_ratio);

						*pPixel = (tA << 24) | (tR << 16) | (tG << 8) | (tB << 0);
					}
				}
				if (style_val.writingMode != TRUE) { // 横書き
					spacing += style_val.fontSize_Width + style_val.letter_spacing;
				}
				else { // 縦書き
					spacing += style_val.fontSize_Height + style_val.letter_spacing;
				}
				FT_Done_Glyph(glyph);
				glyph = NULL;
			}
			break;
		}
	}

	if (pMadVROsdServices != NULL) {
		pMadVROsdServices->OsdSetBitmap("subtitle", bitmap_sub, 0, 0, 0, 0, TRUE, 0, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
	}

	SAFE_RELEASE(pStream);
	SAFE_RELEASE(pReader);

	timeKillEvent(timerID);
}

static void CALLBACK TimerThreadTimeoutCallBack(unsigned int timerID, unsigned int msg, unsigned int usrParam, unsigned int dw1, unsigned int dw2)
{
	CAutoLock lock_it(&m_ThreadTimeoutLock);

	puts("TimerThreadTimeoutCallBack");

	DWORD dwExitCode;
	GetExitCodeThread(hThread, &dwExitCode);
	if (dwExitCode == STILL_ACTIVE) {
		puts("KILL THREAD");
		TerminateThread(hThread, dwExitCode);
		channel_select_thread_running = FALSE;

		SendNotifyMessage(m_hWnd, WM_COMMAND, ID_POPUPMENU0_40024, 0);
	}
	timeKillEvent(timerID);
}

static void CALLBACK TimerMMTPCallBack(unsigned int timerID, unsigned int msg, unsigned int usrParam, unsigned int dw1, unsigned int dw2)
{
	PostMessage(m_hWnd, WM_NOTIFY, MMTP_PACKET_READ_CODE, 0);
}


static void CALLBACK Timer1HzCallBack(unsigned int timerID, unsigned int msg, unsigned int usrParam, unsigned int dw1, unsigned int dw2)
{
	CAutoLock lock_it(&m_Timer1HzLock);

	if (g_record) {
		elapsed++;
	}

	if (((elapsed - pre_elapsed) >= 5)) {
		if (full_screen) {
			if (pVideoWindow != NULL) {
				if (hideCursor == 0) {
					pVideoWindow->HideCursor(OATRUE);
					hideCursor = 1;

					RECT rect = {};
					GetClientRect(m_hWnd, &rect);
					pVideoWindow->SetWindowPosition(0, 0, rect.right - rect.left, rect.bottom - rect.top);
				}
			}
		}

		if (ui_screen) {
			if (!((g_point.x >= PANEL_X) && (g_point.x <= (PANEL_X + PANEL_WIDTH)) && (g_point.y >= PANEL_Y) && (g_point.y <= (PANEL_Y + PANEL_HEIGHT)))) {
				if ((pMadVROsdServices != NULL)) {
					ui_screen = 0;
					pMadVROsdServices->OsdSetBitmap("ui", NULL, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					//			OutputDebug(L"OSD OFF\n");
				}
			}
		}
	}

	static INT prev_minute = 0;

	if ((display_size.clock == TRUE) && (g_minute >= 0) && (g_minute != prev_minute)) {
		display_clock(1);
		prev_minute = g_minute;
	}

	if (hFileMMTP != NULL) {
		Manipulate_UI(0, 0, COMMAND_MANIPULATE_UI_CURRENT_TIME);
	}

	if ((IsWindowVisible(hdlg) == TRUE) && (tlv != NULL)) {
		float sig = tuner->GetSignalLevel();
		_swprintf(debug_info_message, \
			L"[APP]\n"
			L"JST   = %02d:%02d:%02d\n"
			L"Time  = %02d:%02d:%02d\n"
			L"V_MEM = %2lldMB UDF = %d\n"
			L"A_MEM = %2lldKB UDF = %d\n"
			L"\n"
			L"[TUNER]\n"
			L"SigLv = %.1f\n"
			L"DRate = %dMbps\n"
			L"Freq  = %.3fMHz\n"
			L"TSID  = 0x%04X\n"
			L"\n"
			L"[ASSET]\n"
			L"CID   = 0x%03X\n"
			L"V_PID = 0x%04X\n"
			L"A_PID = 0x%04X\n"
			L"\n"
			L"[TLV PACKET]\n"
			L"IPv4  = %I64u\n"
			L"IPv6  = %I64u\n"
			L"Comp  = %I64u\n"
			L"Ctrl  = %I64u\n"
			L"Null  = %I64u\n"
			L"ERROR = %I64u\n"
			L"TOTAL = %I64u\n",
			g_hour >= 0 ? g_hour : 0,
			g_minute >= 0 ? g_minute : 0,
			g_second >= 0 ? g_second : 0,
			(int)(elapsed / 3600),
			(int)((elapsed / 60) % 60),
			(int)(elapsed % 60),
			(llabs)((video_frame_buffer->llTotalReadBytes - video_frame_buffer->llTotalWriteBytes) >> 20), video_frame_buffer->countUnderflow,
			(llabs)((audio_frame_buffer->llTotalReadBytes - audio_frame_buffer->llTotalWriteBytes) >> 10), audio_frame_buffer->countUnderflow,
			sig,
			(int)(((g_count - pre_count) * 8) / 1000000),
			(float)Frequency / 1000.0f,
			(UINT16)tsid,
			tlv->uiCID,
			tlv->uiVideo_PID,
			tlv->uiAudio_PID,
			tlv->packetTypeIPV4,
			tlv->packetTypeIPV6,
			tlv->packetTypeCompressed,
			tlv->packetTypeControl,
			tlv->packetTypeNull,
			tlv->error,
			tlv->packetTypeIPV4 + tlv->packetTypeIPV6 + tlv->packetTypeCompressed + tlv->packetTypeControl + tlv->packetTypeNull + tlv->error
		);

		SendNotifyMessage(hdlg, WM_PAINT, 0, 0);
	}

	/*
	printf("\rSIG:%.1f DRATE:%.1fMBps REC:%d WERR:%02d ELP:%02d:%02d:%02d VD:%ldM AD:%ldK    ", \
		sig, \
		(float)((g_count - pre_count) * 8) / 1000000.0f, \
		g_record, \
		write_error, \
		elapsed / 3600, \
		(elapsed / 60) % 60, \
		elapsed % 60, \
		(video_frame_buffer->llTotalReadBytes - video_frame_buffer->llTotalWriteBytes) >> 20, \
		(audio_frame_buffer->llTotalReadBytes - audio_frame_buffer->llTotalWriteBytes) >> 10 \
	);
	*/

	pre_count = g_count;

	//if (GetAsyncKeyState('X')) {
	//	set_exit = 1;
	//}
	//if (GetAsyncKeyState('R')) {
	//	if (!g_record) {
	//		g_record = 1;
	//		elapsed = 0;
	//	}
	//}
}

DWORD WINAPI ThreadChannelSelectFunc(LPVOID lpVoid)
{
	int retry = 3;
	channel_select_thread_running = TRUE;

START_RETRY:
	if (tuner->SetChannel((BYTE)display_size.select_ch) == FALSE) {
		if (--retry >= 0) {
			tuner->CloseTuner();
			tuner->OpenTuner();
			OutputDebug(L"***RETRY SetChannel***");
			goto START_RETRY;
		}
		puts("Set channnel failed");
		channel_select_thread_running = FALSE;

		MessageBeep(MB_OK);
		channel_status = CHANNEL_STATUS_SELECT_FAILED; // 選局失敗
		InvalidateRect(m_hWnd, NULL, TRUE);
		SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);
		Sleep(2000);
		channel_status = CHANNEL_STATUS_HALT;
		SendNotifyMessage(m_hWnd, WM_COMMAND, ID_POPUPMENU0_40024, 0); // 再生停止
		SetActiveWindow(m_hWnd);

		timeKillEvent(timer_thread);

		ExitThread(TRUE);
	}
	else {
		puts("Set channel successfully");
	}

	channel_status = CHANNEL_STATUS_BUFFERING;
	InvalidateRect(m_hWnd, NULL, TRUE);
	SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

	tlv = new CTlvParse();
	video_frame_buffer = new CFrameBuffer(VIDEO_FRAME_BUFFER_SIZE);
	audio_frame_buffer = new CFrameBuffer(AUDIO_FRAME_BUFFER_SIZE);
	tlv->uiCID = (UINT16)cid;

	if (startCaptureGraph() == S_OK) {
		PostMessage(m_hWnd, WM_NOTIFY, THREAD_CHANNLE_SELECT_START_CODE, 0);
		PostMessage(m_hWnd, WM_ACTIVATE, 0, 0);
	}

	channel_select_thread_running = FALSE;

	timeKillEvent(timer_thread);
	ExitThread(TRUE);
}


DWORD WINAPI ThreadMMTPStarCaptureGraph(LPVOID lpVoid)
{
	if (startCaptureGraph() == S_OK) {
		PostMessage(m_hWnd, WM_NOTIFY, THREAD_CHANNLE_SELECT_START_CODE, 0);
		PostMessage(m_hWnd, WM_ACTIVATE, 0, 0);
	}
	else {
		timeKillEvent(timerMMTP);
		SendNotifyMessage(m_hWnd, WM_COMMAND, ID_POPUPMENU0_40024, 0); // 再生停止

		MessageBeep(MB_OK);
		channel_status = CHANNEL_STATUS_SELECT_FAILED; // 選局失敗
		InvalidateRect(m_hWnd, NULL, TRUE);
		SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

	}

	ExitThread(TRUE);
}

DWORD WINAPI ThreadMMTPStarCaptureGraphSeek(LPVOID lpVoid)
{
	ULONGLONG tick = GetTickCount64();

	while (audio_frame_buffer->writePos <= LAV_START_TO_PROCESS_READ_SIZE) {
		if ((GetTickCount64() - tick) > 10000) {
			if (pVideoWindow != NULL) {
				pVideoWindow->put_Visible(OAFALSE);
				pVideoWindow->put_MessageDrain(NULL);
				pVideoWindow->put_Owner(NULL);
				SAFE_RELEASE(pVideoWindow);
			}
			channel_status = CHANNEL_STATUS_HALT;
			InvalidateRect(m_hWnd, NULL, TRUE);
			SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

			manipulate_lock = 0;

			ExitThread(TRUE);

			return E_FAIL;
		}
	}

	INT retry = 3;
	while (FAILED(pCaptureGraphBuilder2->RenderStream(0, 0, pAudioSrc, pSplitterAudioSrc, pAudio)) && (--retry >= 0)) {
		Sleep(10);
	}
	if (retry < 0) {
		//g_record = 0;

		if (pVideoWindow != NULL) {
			pVideoWindow->put_Visible(OAFALSE);
			pVideoWindow->put_MessageDrain(NULL);
			pVideoWindow->put_Owner(NULL);
			SAFE_RELEASE(pVideoWindow);
		}
		channel_status = CHANNEL_STATUS_HALT;
		InvalidateRect(m_hWnd, NULL, TRUE);
		SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

		MessageBox(NULL, L"Audio RenderStream failed", L"Error", MB_OK | MB_TOPMOST);

		manipulate_lock = 0;

		ExitThread(TRUE);

		return E_FAIL;
	}

	time_to_start_graph = GetTickCount64() - tick;

	IMediaSeeking* m_pMS;
	if (FAILED(pAudio->QueryInterface(IID_IMediaSeeking, (void**)&m_pMS))) {
		puts("pAudio QI IMediaSeeking failed");
	}
	else {
		REFERENCE_TIME current = audio_delay[display_size.select_ch] * 1000 * 10;//500 * 1000 * 10; // 500ms audio shift
		if (FAILED(m_pMS->SetPositions(&current, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning))) {
			puts("SetPositions failed");
		}
		SAFE_RELEASE(m_pMS);
	}

	retry = 3;
	while (FAILED(pCaptureGraphBuilder2->RenderStream(0, 0, pVideoSrc, pSplitterVideoSrc, pVideo)) && (--retry >= 0)) {
		Sleep(10);
	}
	if (retry < 0) {
		//g_record = 0;

		if (pVideoWindow != NULL) {
			pVideoWindow->put_Visible(OAFALSE);
			pVideoWindow->put_MessageDrain(NULL);
			pVideoWindow->put_Owner(NULL);
			SAFE_RELEASE(pVideoWindow);
		}
		channel_status = CHANNEL_STATUS_HALT;
		InvalidateRect(m_hWnd, NULL, TRUE);
		SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

		MessageBox(NULL, L"Video RenderStream failed", L"Error", MB_OK | MB_TOPMOST);

		manipulate_lock = 0;

		ExitThread(TRUE);

		return E_FAIL;
	}

	memset(ppv_sub, 0, 3840 * 2160 * sizeof(UINT32));
	pMadVROsdServices->OsdSetBitmap("subtitle", bitmap_sub, 0, 0, 0, 0, TRUE, 0, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);


	for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
		timeKillEvent(timer_subtitle_thread[i]);
		timer_subtitle_thread[i] = NULL;
	}

	tlv->disbale_XmlSubUpdate_notify = FALSE;
	tlv->InitializeSubtitle();


	if (FAILED(pMediaControl->Run())) {
		OutputDebug(L"Restart Failed\n");
	}
	else {
		OutputDebug(L"Restart Success\n");
	}

	const GUID LAV_VideoDecoder = { 0xEE30215D, 0x164F, 0x4A92, { 0xA4, 0xEB, 0x9D, 0x4C, 0x13, 0x39, 0x0F, 0x9F} };
	const GUID LAV_AudioDecoder = { 0xE8E73B6B, 0x4CB3, 0x44A4, { 0xBE, 0x99, 0x4F, 0x7B, 0xCB, 0x96, 0xE4, 0x91} };
	CLSID clsid;
	LPWSTR idGet;

	IEnumFilters* pEnum = NULL;
	if (SUCCEEDED(pGraphBuilder->EnumFilters(&pEnum)))
	{
		IBaseFilter* pFilter = NULL;
		while (S_OK == pEnum->Next(1, &pFilter, NULL))
		{
			pFilter->GetClassID(&clsid);
			IEnumPins* pPinEnum = NULL;
			if (SUCCEEDED(pFilter->EnumPins(&pPinEnum))) {
				IPin* pPin = NULL;
				while (S_OK == pPinEnum->Next(1, &pPin, NULL)) {
					pPin->QueryId(&idGet);
					if (memcmp(&clsid, &LAV_VideoDecoder, sizeof(GUID)) == 0) {
						pLAVVideoDecoder = pFilter;
					}
					else if (memcmp(&clsid, &LAV_AudioDecoder, sizeof(GUID)) == 0) {
						pLAVAudioDecoder = pFilter;
					}

					SAFE_RELEASE(pPin);
				}
				SAFE_RELEASE(pPinEnum);
			}
			SAFE_RELEASE(pFilter);
		}
		SAFE_RELEASE(pEnum);
	}

	pre_count = 0;
	elapsed = 0;
	pre_elapsed = 0;

	if ((timer = timeSetEvent(1000, 0, (LPTIMECALLBACK)Timer1HzCallBack, (DWORD)NULL, TIME_PERIODIC)) == 0) {
		printf("time event fail\n");
	}

	if (pBasicAudio != NULL) {
		pBasicAudio->put_Volume(display_size.level);

	}

	manipulate_lock = 0;

	ExitThread(TRUE);
}



// [ファイルを開く]ダイアログ
HRESULT funcFileOpen(HWND hWnd, LPCWSTR fileName)
{
	BOOL ofn_ret = FALSE;

	if (fileName == NULL) {
		if (szOfnPath[0] == TEXT('\0')) {
			GetCurrentDirectory(MAX_PATH, szOfnPath);
		}

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = hWnd;
		ofn.lpstrInitialDir = szOfnPath;       // 初期フォルダ位置
		ofn.lpstrFile = szOfnFile;       // 選択ファイル格納
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = TEXT("MMTSファイル(*.mmts,*.mmt)\0*.mmts;*.mmt\0")
			TEXT("MMTPファイル(*.mmtp,*mtp)\0*.mmtp;*.mtp\0")
			TEXT("TLVファイル(*.tlv)\0*.tlv\0")
			TEXT("TSファイル(*.ts)\0*.ts\0")
			TEXT("すべてのファイル(*.*)\0*.*\0");
		ofn.lpstrTitle = TEXT("録画ファイルを選択してください");
		ofn.Flags = OFN_FILEMUSTEXIST;

		ofn_ret = GetOpenFileName(&ofn);
	}

	if ((ofn_ret == TRUE) || (fileName != NULL)) {

		SendNotifyMessage(m_hWnd, WM_COMMAND, ID_POPUPMENU0_40024, 0); // 再生停止

		LPCWSTR pFile = NULL;

		if (fileName != NULL) {
			pFile = fileName;
		}
		else {
			pFile = szOfnFile;
		}

		if ((hFileMMTP = CreateFile(pFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL)) == INVALID_HANDLE_VALUE) {
			MessageBox(NULL, L"Cannot open file", L"Error", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
			return E_FAIL;
		}
		else {
			if (tlv != NULL) {
				tlv->disable_scramble_notify = TRUE;
			}

			if (pMediaControl != NULL) {
				pMediaControl->Stop();
			}
			g_record = 0;

			timeKillEvent(timerMMTP);
			timeKillEvent(timer);

			for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
				timeKillEvent(timer_subtitle_thread[i]);
				timer_subtitle_thread[i] = NULL;
			}

			OutputDebug(L"録画動画読み込み 6\n");

			// カレントメニューアイテムの文字列をウィンドウテキストにセット
			std::wstring titleStr = L"Xview4K";
			std::wstring titleContentStr = L"番組情報";

			//			StrCpyW(channel_title_str, L"録画再生");
			StrCpyW(channel_title_str, pFile);
			titleStr.append(L" - ");
			titleStr.append(channel_title_str);
			SetWindowText(hWnd, titleStr.c_str());
			StrCpyW(titleBarText, titleStr.data());

			titleContentStr.append(L" - ");
			titleContentStr.append(channel_title_str);
			SetWindowText(hdlgContent, titleContentStr.c_str());
			titleStr.clear();
			titleContentStr.clear();
			SetWindowText(hEdit, L"");

			// 前回選択チャンネルのチェックを外す
			if (GetMenuState(hmenu, display_size.select_command_ch, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, display_size.select_command_ch, MF_BYCOMMAND | MFS_UNCHECKED);
			}

			ReleaseGraphs();

			tuner->CloseTuner();

			channel_status = CHANNEL_STATUS_RECORD_FILE_LOADING;
			InvalidateRect(m_hWnd, NULL, TRUE);
			SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

			HRESULT hr;
			if (FAILED(hr = CreateGraphs())) {
				return E_FAIL;
			}

			if (FAILED(hr = pGraphBuilder->QueryInterface(IID_IVideoWindow, (void**)&pVideoWindow))) {
				MessageBox(NULL, L"VideoWindow QI from GraphBuilder failed", L"Error", MB_OK | MB_TOPMOST);
				return E_FAIL;
			}
			else {
				pVideoWindow->put_Owner((OAHWND)m_hWnd);
				pVideoWindow->put_Visible(OAFALSE);
				pVideoWindow->put_WindowStyle(WS_OVERLAPPED);
				pVideoWindow->put_BackgroundPalette(0);
				pVideoWindow->put_Left(0);
				pVideoWindow->put_Top(0);
			}

			video_frame_buffer = new CFrameBuffer(VIDEO_FRAME_BUFFER_SIZE);
			audio_frame_buffer = new CFrameBuffer(AUDIO_FRAME_BUFFER_SIZE);

			file_seek.QuadPart = 0;
			file_seek_current.QuadPart = 0;
			file_seek_end.QuadPart = 0;
			file_seek_first_TOT.QuadPart = 0;
			file_seek_last_TOT.QuadPart = 0;

			file_seek.QuadPart = 0;
			SetFilePointerEx(hFileMMTP, file_seek, &file_seek_end, FILE_END);

			if (file_seek_end.QuadPart <= (BUFFER_SIZE * 2)) {
				MessageBox(NULL, L"ファイルサイズが小さすぎます", L"エラー", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
				SAFE_DELETE(video_frame_buffer);
				SAFE_DELETE(audio_frame_buffer);
				if (hFileMMTP != NULL) {
					CloseHandle(hFileMMTP);
					hFileMMTP = NULL;
				}
				return E_FAIL;
			}

			BOOL ca[3];

			std::wstring contentTitle1;
			std::wstring contentTitle2;

			tlv = new CTlvParse();

			for (int i = 1; i <= 2; i++) {
				bf = 0;
				tlv->Initialize();
				tlv->uiCID = i;
				tlv->disable_scramble_notify = TRUE;
				tlv->disbale_XmlSubUpdate_notify = TRUE;
				file_seek.QuadPart = 0;
				SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_BEGIN);

				DWORD reads;
				if (ReadFile(hFileMMTP, buffer0, BUFFER_SIZE, &reads, NULL) == 0) {
				}
				if (ReadFile(hFileMMTP, buffer1, BUFFER_SIZE, &reads, NULL) == 0) {
				}

				do {
					tlv->setPacketBufPtr(bf ? buffer1 : buffer0);
					tlv->setPacketBufSize(BUFFER_SIZE);
					tlv->parseScheduler();

					if (ReadFile(hFileMMTP, bf ? buffer1 : buffer0, BUFFER_SIZE, &reads, NULL) == 0) {
						break;
					}
					bf ^= 1;

					file_seek.QuadPart = 0;
					SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_CURRENT);
				} while ((tlv->content[1].contentTitleStr.empty() == TRUE) && (file_seek_current.QuadPart <= 50000000));

				if ((tlv->content[1].contentTitleStr.empty() == TRUE) || (file_seek_current.QuadPart >= 50000000)) {
					MessageBox(NULL, L"ファイルを開けませんでした", L"エラー", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
					SAFE_DELETE(video_frame_buffer);
					SAFE_DELETE(audio_frame_buffer);
					SAFE_DELETE(tlv);
					if (hFileMMTP != NULL) {
						CloseHandle(hFileMMTP);
						hFileMMTP = NULL;
					}
					return E_FAIL;
				}

				OutputDebug(L"cid:%d %s CA:%d\n", tlv->uiCID, tlv->content[0].contentTitleStr.c_str(), tlv->content[0].free_CA_mode);

				INT start_time_val0, start_time_val1;
				start_time_val0 = tlv->content[0].start_time_val;
				start_time_val1 = tlv->content[1].start_time_val;
				if (abs(start_time_val0 - start_time_val1) > (12 * 3600)) {
					if (start_time_val0 < start_time_val1) {
						start_time_val0 += 24 * 3600;
					}
					else {
						start_time_val1 += 24 * 3600;
					}
				}

				if (i == 1) {
					if (start_time_val0 < start_time_val1) {
						contentTitle1.append(tlv->content[0].contentTitleStr.c_str());
						contentTitle1.append(L"\r\n");
						contentTitle1.append(tlv->content[1].contentTitleStr.c_str());
						ca[i] = tlv->content[0].free_CA_mode;
					}
					else {
						contentTitle1.append(tlv->content[1].contentTitleStr.c_str());
						contentTitle1.append(L"\r\n");
						contentTitle1.append(tlv->content[0].contentTitleStr.c_str());
						ca[i] = tlv->content[1].free_CA_mode;
					}
					SetDlgItemText(hdlgSelectCID, IDC_RADIO1, contentTitle1.c_str());
				}
				else if (i == 2) {
					if (start_time_val0 < start_time_val1) {
						contentTitle2.append(tlv->content[0].contentTitleStr.c_str());
						contentTitle2.append(L"\r\n");
						contentTitle2.append(tlv->content[1].contentTitleStr.c_str());
						ca[i] = tlv->content[0].free_CA_mode;
					}
					else {
						contentTitle2.append(tlv->content[1].contentTitleStr.c_str());
						contentTitle2.append(L"\r\n");
						contentTitle2.append(tlv->content[0].contentTitleStr.c_str());
						ca[i] = tlv->content[1].free_CA_mode;
					}
					SetDlgItemText(hdlgSelectCID, IDC_RADIO2, contentTitle2.c_str());
				}
			}

			bf = 0;
			tlv->Initialize();
			tlv->disable_scramble_notify = TRUE;
			tlv->disbale_XmlSubUpdate_notify = TRUE;
			video_frame_buffer->Initialize();
			audio_frame_buffer->Initialize();
			if ((ca[1] == 0) && (ca[2] == 0)) {
				cid_MMTP = 1;
				tlv->uiCID = 1;
				if (wcscmp(contentTitle1.c_str(), contentTitle2.c_str()) != 0) {

					RECT rect = {};
					GetWindowRect(m_hWnd, &rect);
					RECT rect2 = {};
					GetWindowRect(hdlgSelectCID, &rect2);

					POINT point;
					point.x = ((rect.right - rect.left) - (rect2.right - rect2.left)) / 2 + rect.left;
					point.y = ((rect.bottom - rect.top) - (rect2.bottom - rect2.top)) / 2 + rect.top;

					MoveWindow(hdlgSelectCID, point.x, point.y, (rect2.right - rect2.left), (rect2.bottom - rect2.top), TRUE);
					CheckRadioButton(hdlgSelectCID, IDC_RADIO1, IDC_RADIO2, IDC_RADIO1);
					ShowWindow(hdlgSelectCID, SW_SHOW);
					SetActiveWindow(hdlgSelectCID);
					MSG msg;
					while (GetMessage(&msg, NULL, 0, 0)) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
					if (Button_GetState(GetDlgItem(hdlgSelectCID, IDC_RADIO2)) == BST_CHECKED) {
						cid_MMTP = 2;
						tlv->uiCID = 2;
					}
				}
			}
			else if ((ca[1] == 0) && (ca[2] == 1)) {
				cid_MMTP = 1;
				tlv->uiCID = 1;
			}
			else if ((ca[1] == 1) && (ca[2] == 0)) {
				cid_MMTP = 2;
				tlv->uiCID = 2;
			}
			else {
				cid_MMTP = 1;
				tlv->uiCID = 1;
			}

			DWORD reads;
			file_seek.QuadPart = 0;
			SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_BEGIN);

			if (ReadFile(hFileMMTP, buffer0, BUFFER_SIZE, &reads, NULL) == 0) {
			}
			if (ReadFile(hFileMMTP, buffer1, BUFFER_SIZE, &reads, NULL) == 0) {
			}

			g_second = -1;
			BOOL flag_first_TOT = FALSE;
			LONGLONG step_back = 50000000;

			do {
				tlv->setPacketBufPtr(bf ? buffer1 : buffer0);
				tlv->setPacketBufSize(BUFFER_SIZE);
				tlv->parseScheduler();

				if (ReadFile(hFileMMTP, bf ? buffer1 : buffer0, BUFFER_SIZE, &reads, NULL) == 0) {
				}
				if (reads < BUFFER_SIZE) {
					break;
				}
				bf ^= 1;

				if (flag_first_TOT == FALSE) {
					if (g_second > -1) {
						mmtp_start_time = g_hour * 3600 + g_minute * 60 + g_second;
						file_seek.QuadPart = 0;
						SetFilePointerEx(hFileMMTP, file_seek, &file_seek_first_TOT, FILE_CURRENT);
						flag_first_TOT = TRUE;

						g_second = -1;
						bf = 0;
						file_seek.QuadPart = file_seek_end.QuadPart - step_back;
						SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_BEGIN);

						if (ReadFile(hFileMMTP, buffer0, BUFFER_SIZE, &reads, NULL) == 0) {
						}
						if (reads < BUFFER_SIZE) {
							break;
						}
						if (ReadFile(hFileMMTP, buffer1, BUFFER_SIZE, &reads, NULL) == 0) {
						}
						if (reads < BUFFER_SIZE) {
							break;
						}
					}
				}
				else {
					if (g_second > -1) {
						mmtp_end_time = g_hour * 3600 + g_minute * 60 + g_second;
						file_seek.QuadPart = 0;
						SetFilePointerEx(hFileMMTP, file_seek, &file_seek_last_TOT, FILE_CURRENT);
						g_second = -1;
					}
				}

			} while (1);

			if ((file_seek_first_TOT.QuadPart == 0) || (file_seek_last_TOT.QuadPart == 0)) {
				MessageBox(NULL, L"ファイルフォーマットエラー", L"エラー", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
				SAFE_DELETE(video_frame_buffer);
				SAFE_DELETE(audio_frame_buffer);
				SAFE_DELETE(tlv);
				if (hFileMMTP != NULL) {
					CloseHandle(hFileMMTP);
					hFileMMTP = NULL;
				}
				return E_FAIL;
			}

			if (labs(mmtp_end_time - mmtp_start_time) >= (12 * 3600)) {
				if (mmtp_end_time < mmtp_start_time) {
					mmtp_end_time += 24 * 3600;
				}
			}

			mmtp_duration_time = mmtp_end_time - mmtp_start_time;

			//OutputDebug(L"file_seek_first_TOT:%I64d\n", file_seek_first_TOT.QuadPart);
			//OutputDebug(L"%02d:%02d:%02d\n", mmtp_start_time / 3600, (mmtp_start_time / 60) % 60, mmtp_start_time % 60);

			//OutputDebug(L"file_seek_last_TOT:%I64d\n", file_seek_last_TOT.QuadPart);
			//OutputDebug(L"%02d:%02d:%02d\n", mmtp_end_time / 3600, (mmtp_end_time / 60) % 60, mmtp_end_time % 60);

			//OutputDebug(L"duration\n");
			//OutputDebug(L"%02d:%02d:%02d\n", mmtp_duration_time / 3600, (mmtp_duration_time / 60) % 60, mmtp_duration_time % 60);

			//MessageBox(NULL, L"hit last", L"BINGO", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);			

			bf = 0;
			tlv->Initialize();
			tlv->uiCID = cid_MMTP;
			video_frame_buffer->Initialize();
			audio_frame_buffer->Initialize();

			file_seek.QuadPart = 0;// file_seek_first_TOT.QuadPart;
			SetFilePointerEx(hFileMMTP, file_seek, &file_seek_current, FILE_BEGIN);

			if (ReadFile(hFileMMTP, buffer0, BUFFER_SIZE, &reads, NULL) == 0) {
			}
			if (ReadFile(hFileMMTP, buffer1, BUFFER_SIZE, &reads, NULL) == 0) {
			}

			mmtp_current_time = 0;
			Frequency = 0;
			tsid = 0;
			manipulate_lock = 0;

			HANDLE thread = NULL;
			thread = CreateThread(NULL, 0, ThreadMMTPStarCaptureGraph, NULL, 0, &threadID);

			if ((timerMMTP = timeSetEvent(5, 1, (LPTIMECALLBACK)TimerMMTPCallBack, (DWORD)NULL, TIME_PERIODIC)) == 0) {
				OutputDebug(L"time event fail\n");
			}
		}
	}

	return S_OK;
}


LRESULT CALLBACK eMainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	static POINT po;
	HFONT hFont;

	static LONG IStyle = 0;
	static RECT bkRect = {};

	static POINT static_poc;
	static BOOL mouse_clicked = FALSE;

	static int extra_height = 30;

	static int key_count = 0;
	static ULONGLONG key_tick = 0;
	static BYTE key_val[256];


#define STOP_NOW TEXT("再生停止")
#define TUNER_OPEN TEXT("チューナー初期化中")
#define TUNER_OPEN_FAILED TEXT("チューナー初期化失敗")
#define SELECTING TEXT("選局中")
#define RECORD_FILE_LOADING TEXT("読み込み中")
#define BUFFERING TEXT("バッファリング中")
#define SELECT_FAILED TEXT("選局失敗")
#define SCRAMBLE_EVENT TEXT("スクランブル放送")

	//OutputDebug(L"msg:%04X\n", uMsg);
	//WCHAR sz[100];
	//wsprintf(sz, L"msg:%04X\n", uMsg);
	//OutputDebugString(sz);

	switch (uMsg) {
	case WM_NOTIFY:
	{
		switch (wParam) {
		case NOTIFY_CONTENT_UPDATE_CODE:
		{
			TabCtrl_SetCurSel(hTab, 0);

			INT start_time_val0, start_time_val1;
			start_time_val0 = tlv->content[0].start_time_val;
			start_time_val1 = tlv->content[1].start_time_val;
			if (abs(start_time_val0 - start_time_val1) > (12 * 3600)) {
				if (start_time_val0 < start_time_val1) {
					start_time_val0 += 24 * 3600;
				}
				else {
					start_time_val1 += 24 * 3600;
				}
			}

			WCHAR titleBarTextOut[300];
			memset(titleBarTextOut, 0, sizeof(titleBarTextOut));
			StrCatW(titleBarTextOut, titleBarText);
			if (start_time_val0 < start_time_val1) {
				StrCatW(titleBarTextOut, L" / ");
				StrCatW(titleBarTextOut, tlv->content[0].contentTitleStr.c_str());
				SetWindowText(m_hWnd, titleBarTextOut);
				SetWindowText(hEdit, tlv->content[0].contentInfoStr.data());
			}
			else {
				StrCatW(titleBarTextOut, L" / ");
				StrCatW(titleBarTextOut, tlv->content[1].contentTitleStr.c_str());
				SetWindowText(m_hWnd, titleBarTextOut);
				SetWindowText(hEdit, tlv->content[1].contentInfoStr.data());
			}
		}
		break;
		case THREAD_CHANNLE_SELECT_START_CODE:
		{
			RECT rect = {};
			GetClientRect(m_hWnd, &rect);
			pVideoWindow->SetWindowPosition(0, 0, rect.right - rect.left, rect.bottom - rect.top);
			pVideoWindow->put_MessageDrain((OAHWND)m_hWnd);
			pVideoWindow->put_Visible(OATRUE);
			//if (full_screen) {
			//	if (hideCursor == 0) {
			//		pVideoWindow->HideCursor(OATRUE);
			//		hideCursor = 1;
			//	}
			//}
		}
		break;
		case MMTP_PACKET_READ_CODE:
		{
			if (hFileMMTP == NULL) {
				break;
			}

			if ((llabs)(audio_frame_buffer->llTotalReadBytes - audio_frame_buffer->llTotalWriteBytes) > LAV_START_TO_PROCESS_READ_SIZE) {
				break;
			}

			tlv->setPacketBufPtr(bf ? buffer1 : buffer0);
			tlv->setPacketBufSize(BUFFER_SIZE);
			tlv->parseScheduler();

			DWORD reads;
			if (ReadFile(hFileMMTP, bf ? buffer1 : buffer0, BUFFER_SIZE, &reads, NULL) == 0) {
			}
			bf ^= 1;
			g_count += BUFFER_SIZE;
			tlv->mmtp_read_notified = FALSE;
		}
		break;
		case SCRAMBLE_DETECT_CODE:
		{
			SendNotifyMessage(m_hWnd, WM_COMMAND, ID_POPUPMENU0_40024, 0); // 再生停止

			MessageBeep(MB_OK);
			channel_status = CHANNEL_STATUS_SCRAMBLE; // 選局失敗
			InvalidateRect(m_hWnd, NULL, TRUE);
			SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

			SetActiveWindow(m_hWnd);

			//			MessageBox(NULL, L"この番組はスクランブル放送です", L"Info", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		break;
		case XML_SUB_UPDATE_CODE:
		{
			if (display_size.subtile == FALSE) {
				break;
			}

			INT sub_delay = -500;
			if (hFileMMTP != NULL) {
				sub_delay = 1500;
			}

			if ((timer_subtitle_thread[timer_subtitle_thread_index] = timeSetEvent(time_to_start_graph + sub_delay, 1, (LPTIMECALLBACK)TimerThreadSubtitleCallBack, (DWORD)NULL, TIME_ONESHOT)) == 0) {
				OutputDebug(L"timeSetEvent fail\n");
			}

			if (++timer_subtitle_thread_index >= XML_SUB_BUFFER_NUM) {
				timer_subtitle_thread_index = 0;
			}
		}
		break;
		default:
			break;
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		pre_elapsed = elapsed;

		RECT rect = {};
		POINT pos = {};
		POINT poc = {};
		GetCursorPos(&pos);
		memcpy(&poc, &pos, sizeof(POINT));
		ScreenToClient(hWnd, &poc);
		GetClientRect(hWnd, &rect);

		if ((pVideo != NULL)) {
			if (hFileMMTP != NULL) {
				DOUBLE fx, fy;
				RECT madRect = {};
				IMadVRInfo* pMadVRInfo;
				if (pVideo->QueryInterface(__uuidof(IMadVRInfo), (void**)&pMadVRInfo) == S_OK) {
					pMadVRInfo->GetRect("croppedVideoOutputRect", &madRect);

					SAFE_RELEASE(pMadVRInfo);
				}

				//OutputDebug(L"x:%d y:%d\n", poc.x, poc.y);
				//OutputDebug(L"x:%d y:%d w:%d h:%d\n", madRect.left, madRect.top, madRect.right - madRect.left, madRect.bottom - madRect.top);
				fx = (DOUBLE)((LONGLONG)poc.x - (LONGLONG)madRect.left) / (DOUBLE)((LONGLONG)madRect.right - (LONGLONG)madRect.left);
				fx = fx < 0 ? 0 : fx, fx = fx > 1 ? 1 : fx;

				fy = (DOUBLE)((LONGLONG)poc.y - (LONGLONG)madRect.top) / (DOUBLE)((LONGLONG)madRect.bottom - (LONGLONG)madRect.top);
				fy = fy < 0 ? 0 : fy, fy = fy > 1 ? 1 : fy;

				//			OutputDebug(L"fx:%.2f fy:%.2f\n", fx, fy);

				LONG posX, posY;
				posX = fx * 3840;
				posY = fy * 2160;

				if ((posX != g_point.x) && (posY != g_point.y) && (pMadVROsdServices != NULL)) {
					if (ui_screen == 0) {
						pMadVROsdServices->OsdSetBitmap("ui", bitmap_ui, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
						ui_screen = 1;
					}
					if (full_screen) {
						if (hideCursor) {
							pVideoWindow->HideCursor(OAFALSE);
							hideCursor = 0;
						}
					}
				}

				g_point.x = posX;
				g_point.y = posY;

				if (mouse_clicked == FALSE) {
					break;
				}

				if (Manipulate_UI(posX, posY, COMMAND_MANIPULATE_UI_CHANGE_SEEKBAR_POS)) {
					break;
				}

				if ((posX >= PANEL_X) && (posX <= (PANEL_X + PANEL_WIDTH)) && (posY >= PANEL_Y) && (posY <= (PANEL_Y + PANEL_HEIGHT))) {
					break;
				}
			}
			else {
				if (full_screen) {
					if (hideCursor) {
						pVideoWindow->HideCursor(OAFALSE);
						hideCursor = 0;
					}
				}
			}
		}

		if (mouse_clicked == FALSE) {
			break;
		}


		if (full_screen) {
			break;
		}

		if (PtInRect(&rect, poc) && (GetAsyncKeyState(VK_LBUTTON) < 0)) {
			RECT w = {};
			GetWindowRect(hWnd, &w);
			LONG width = w.right - w.left;
			LONG height = w.bottom - w.top;
			LONG offsetX = static_poc.x + (width - (rect.right - rect.left) - (display_size.title_bar ? 11 : 0));
			LONG offsetY = static_poc.y + (height - (rect.bottom - rect.top) - (display_size.title_bar ? 11 : 0));
			MoveWindow(hWnd, pos.x - offsetX, pos.y - offsetY, width, height, TRUE);
		}
	}
	break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		if (!(lParam & 0x40000000)) {
			ULONGLONG tick_now = GetTickCount64();
			if ((tick_now - key_tick) >= 500) {
				key_count = 0;
			}
			else {
				++key_count;
			}
			key_tick = tick_now;
			key_val[key_count] = wParam;

			BYTE keyPress[4];

			keyPress[0] = key_val[key_count];
			keyPress[1] = 0;
			keyPress[2] = 0;
			keyPress[3] = 0;

			if (keyPress[0] == VK_RIGHT) { // コマ送り
				if (hFileMMTP != NULL) {
					if (pMediaControl != NULL) {
						OAFilterState ofs;
						pMediaControl->GetState(100, &ofs);
						if (ofs == State_Paused) {
							IVideoFrameStep* pVideoFrameStep = NULL;
							if (SUCCEEDED(pGraphBuilder->QueryInterface(IID_IVideoFrameStep, (LPVOID*)&pVideoFrameStep))) {
								if (SUCCEEDED(pVideoFrameStep->CanStep(0, NULL))) {
									OutputDebug(L"Frame Step OK\n");

									pVideoFrameStep->Step(1, NULL);
								}
								else {
									OutputDebug(L"Frame Step NG\n");
								}
								SAFE_RELEASE(pVideoFrameStep);
							}
							else {
								OutputDebug(L"pVideoFrameStep QI failed\n");
							}
						}
					}
				}
			}

			if (keyPress[0] == VK_SPACE) {  // 一時停止・再生
				if (hFileMMTP != NULL) {
					Manipulate_UI(PLAY_ICON_X_POS, PLAY_ICON_Y_POS, COMMAND_MANIPULATE_UI_CHANGE_SEEKBAR_POS);
					Manipulate_UI(0, 0, COMMAND_MANIPULATE_UI_LBUTTON_RELEASE);
					if (ui_screen == 0) {
						pMadVROsdServices->OsdSetBitmap("ui", bitmap_ui, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
						ui_screen = 1;
					}
				}
			}

			if (key_count == 0) {
				if (keyPress[0] == VK_RIGHT) { // １つ次へ選局
					if (hFileMMTP == NULL) {
						if ((display_size.select_ch + 1) >= channel_number) {
							PostMessage(m_hWnd, WM_COMMAND, channel_to_id_table[0], 0);
						}
						else {
							PostMessage(m_hWnd, WM_COMMAND, channel_to_id_table[display_size.select_ch + 1], 0);
						}
					}
				}
				else if (keyPress[0] == VK_LEFT) { // １つ前へ選局
					if (hFileMMTP == NULL) {
						if ((display_size.select_ch - 1) < 0) {
							PostMessage(m_hWnd, WM_COMMAND, channel_to_id_table[channel_number - 1], 0);
						}
						else {
							PostMessage(m_hWnd, WM_COMMAND, channel_to_id_table[display_size.select_ch - 1], 0);
						}
					}
				}
			}
			else if (key_count == 1) {
				switch (key_val[key_count - 1]) {
				case 0x10: // shift
					keyPress[1] = 1;
					break;
				case 0x11: // control
					keyPress[2] = 1;
					break;
				case 0x12: // alt
					keyPress[3] = 1;
					break;
				default:
					break;
				}
			}
			else if (key_count == 2) {
				switch (key_val[key_count - 1]) {
				case 0x10:
					keyPress[1] = 1;
					break;
				case 0x11:
					keyPress[2] = 1;
					break;
				case 0x12:
					keyPress[3] = 1;
					break;
				default:
					break;
				}

				switch (key_val[key_count - 2]) {
				case 0x10:
					keyPress[1] = 1;
					break;
				case 0x11:
					keyPress[2] = 1;
					break;
				case 0x12:
					keyPress[3] = 1;
					break;
				default:
					break;
				}

			}
			else if (key_count >= 3) {
				switch (key_val[key_count - 1]) {
				case 0x10:
					keyPress[1] = 1;
					break;
				case 0x11:
					keyPress[2] = 1;
					break;
				case 0x12:
					keyPress[3] = 1;
					break;
				default:
					break;
				}

				switch (key_val[key_count - 2]) {
				case 0x10:
					keyPress[1] = 1;
					break;
				case 0x11:
					keyPress[2] = 1;
					break;
				case 0x12:
					keyPress[3] = 1;
					break;
				default:
					break;
				}

				switch (key_val[key_count - 3]) {
				case 0x10:
					keyPress[1] = 1;
					break;
				case 0x11:
					keyPress[2] = 1;
					break;
				case 0x12:
					keyPress[3] = 1;
					break;
				default:
					break;
				}
			}

			if (pMadVRCommand != NULL) {
				pMadVRCommand->SendCommandInt("keyPress", *(int*)&keyPress);
			}

			OutputDebug(L"KeyDown:%X count:%d\n", wParam, key_count);
		}
	}
	break;
	case WM_LBUTTONUP:
	{
		mouse_clicked = FALSE;

		if (hFileMMTP != NULL) {
			Manipulate_UI(0, 0, COMMAND_MANIPULATE_UI_LBUTTON_RELEASE);
		}
	}
	break;
	case WM_LBUTTONDOWN:
	{
		static_poc.x = GET_X_LPARAM(lParam);
		static_poc.y = GET_Y_LPARAM(lParam);
		SetActiveWindow(hWnd);
		mouse_clicked = TRUE;

		POINT pos = {};
		POINT poc = {};
		GetCursorPos(&pos);
		memcpy(&poc, &pos, sizeof(POINT));
		ScreenToClient(hWnd, &poc);

		if ((pVideo != NULL) && (hFileMMTP != NULL)) {
			DOUBLE fx, fy;
			RECT madRect = {};
			IMadVRInfo* pMadVRInfo;
			if (pVideo->QueryInterface(__uuidof(IMadVRInfo), (void**)&pMadVRInfo) == S_OK) {
				pMadVRInfo->GetRect("croppedVideoOutputRect", &madRect);

				SAFE_RELEASE(pMadVRInfo);
			}

			//OutputDebug(L"x:%d y:%d\n", poc.x, poc.y);
			//OutputDebug(L"x:%d y:%d w:%d h:%d\n", madRect.left, madRect.top, madRect.right - madRect.left, madRect.bottom - madRect.top);
			fx = (DOUBLE)((LONGLONG)poc.x - (LONGLONG)madRect.left) / (DOUBLE)((LONGLONG)madRect.right - (LONGLONG)madRect.left);
			fx = fx < 0 ? 0 : fx, fx = fx > 1 ? 1 : fx;

			fy = (DOUBLE)((LONGLONG)poc.y - (LONGLONG)madRect.top) / (DOUBLE)((LONGLONG)madRect.bottom - (LONGLONG)madRect.top);
			fy = fy < 0 ? 0 : fy, fy = fy > 1 ? 1 : fy;

			//OutputDebug(L"fx:%.2f fy:%.2f\n", fx, fy);

			LONG posX, posY;
			posX = fx * 3840;
			posY = fy * 2160;

			if (Manipulate_UI(posX, posY, COMMAND_MANIPULATE_UI_CHANGE_SEEKBAR_POS) == 1) {
				break;
			}

			if ((posX >= PANEL_X) && (posX <= (PANEL_X + PANEL_WIDTH)) && (posY >= PANEL_Y) && (posY <= (PANEL_Y + PANEL_HEIGHT))) {
				if ((posX >= PREV_ICON_X_POS) && (posX <= (PREV_ICON_X_POS + PREV_ICON_WIDTH)) && (posY >= PREV_ICON_Y_POS) && (posY <= (PREV_ICON_Y_POS + PREV_ICON_HEIGHT))) {
					break;
				}
				if ((posX >= NEXT_ICON_X_POS) && (posX <= (NEXT_ICON_X_POS + NEXT_ICON_WIDTH)) && (posY >= NEXT_ICON_Y_POS) && (posY <= (NEXT_ICON_Y_POS + NEXT_ICON_HEIGHT))) {
					break;
				}

				if (ui_screen == 1) {
					pMadVROsdServices->OsdSetBitmap("ui", NULL, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					ui_screen = 0;
				}
				else {
					pMadVROsdServices->OsdSetBitmap("ui", bitmap_ui, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					ui_screen = 1;
				}
				if (full_screen) {
					if (hideCursor) {
						pVideoWindow->HideCursor(OAFALSE);
						hideCursor = 0;
					}
					else {
						pVideoWindow->HideCursor(OATRUE);
						hideCursor = 1;
					}
				}

			}

		}

	}
	break;
	/*
	case WM_NCHITTEST:
	{
//		puts("WM_NCHITTEST");
		RECT rect = {};
		POINT poc = {};
		poc.x = GET_X_LPARAM(lParam);
		poc.y = GET_Y_LPARAM(lParam);
		GetClientRect(hWnd, &rect);

		ScreenToClient(hWnd, &poc);

		if (PtInRect(&rect, poc) && GetAsyncKeyState(VK_LBUTTON) < 0) {
			return HTCAPTION;
		}
	}
	break;
	*/
	case WM_ACTIVATE:
	{
	}
	break;
	case WM_CREATE:
	{
		//		CREATESTRUCT* tpCreateSt = (CREATESTRUCT*)lParam;
		RECT w = {}, c = {};
		GetWindowRect(hWnd, &w);
		GetClientRect(hWnd, &c);
		extra_height = (w.bottom - w.top) - (c.bottom - c.top) - 11;
		int width, height;
		width = w.right - w.left;
		height = (int)(width * (9.0 / 16.0));
		SetWindowPos(hWnd, NULL, 0, 0, width, height + (display_size.title_bar ? extra_height : 0), SWP_NOMOVE | SWP_NOZORDER);
		SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

		hmenu = GetSubMenu(LoadMenu(((LPCREATESTRUCT)(lParam))->hInstance, MAKEINTRESOURCE(IDR_MENU1)), 0);

		// 字幕表示
		if (display_size.subtile == TRUE) {
			SendMessage(hWnd, WM_COMMAND, ID_POPUPMENU0_40050, 0);
		}

		// 時計表示
		if (display_size.clock == TRUE) {
			SendMessage(hWnd, WM_COMMAND, ID_POPUPMENU0_40052, 0);
		}

		// 常に前面に表示
		if (display_size.top_most == TRUE) {
			SendMessage(hWnd, WM_COMMAND, ID_POPUPMENU0_40023, 0);
		}

		// タイトルバー非表示
		if (display_size.title_bar == FALSE) {
			PostMessage(hWnd, WM_COMMAND, ID_POPUPMENU0_40003, 0);
		}

		// 表示サイズチェック
		if (display_size.command != -1) {
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_CHECKED);
		}

		// 選局チェック
		CheckMenuItem(hmenu, display_size.select_command_ch, MF_BYCOMMAND | MFS_CHECKED);

		// 自動再生
		//if (tuner->IsTunerOpening()) {
		//	PostMessage(hWnd, WM_COMMAND, display_size.select_command_ch, 0);
		//}

		// 音量チェック
		CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_CHECKED);

		HICON hIcon;
		hIcon = (HICON)LoadImage(ghInst, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 16, 16, 0);

		hdlg = CreateDialog(NULL, MAKEINTRESOURCE(IDD_FORMVIEW), hWnd, DlgProc);
		SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		hdlgSelectCID = CreateDialog(NULL, MAKEINTRESOURCE(IDD_FORMVIEW1), hWnd, DlgProcSelectCID);
		SendMessage(hdlgSelectCID, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		hdlgContent = CreateDialog(NULL, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgContentProc);

		hTab = GetDlgItem(hdlgContent, IDC_TAB1);
		hEdit = GetDlgItem(hdlgContent, IDC_EDIT1);

		TC_ITEM TabItem;
		TabItem.mask = TCIF_TEXT;
		TabItem.pszText = L"ただいまの番組";
		TabCtrl_InsertItem(hTab, 0, &TabItem);

		TabItem.mask = TCIF_TEXT;
		TabItem.pszText = L"このあとの番組";
		TabCtrl_InsertItem(hTab, 1, &TabItem);

		// ウインドウを表示する
		::ShowWindow(hWnd, SW_SHOW);
	}
	break;
	case WM_DPICHANGED:
	{
	}
	break;
	case WM_COMMAND:
	{
		BYTE ch = 0;
		UINT uState;
		DWORD param = LOWORD(wParam);
		RECT rect = {};

		switch (param) {
		case ID_POPUPMENU0_40053: // ファイルを開く
		{
			funcFileOpen(hWnd, NULL);
		}
		break;
		case ID_POPUPMENU0_40050: // 字幕表示
		{
			// 前回選択のチェックを外す
			if (GetMenuState(hmenu, param, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_UNCHECKED);
				display_size.subtile = FALSE;

				for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
					timeKillEvent(timer_subtitle_thread[i]);
					timer_subtitle_thread[i] = NULL;
					Sleep(1);
				}

				memset(ppv_sub, 0, 3840 * 2160 * sizeof(UINT32));
				if (pMadVROsdServices != NULL) {
					pMadVROsdServices->OsdSetBitmap("subtitle", bitmap_sub, 0, 0, 0, 0, TRUE, 0, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
				}
			}
			else {
				if (tlv != NULL) {
					tlv->uiXmlSubBufWritePos = 0;
					tlv->uiXmlSubBufReadPos = 0;
				}

				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_CHECKED);
				display_size.subtile = TRUE;
			}
			SetActiveWindow(hWnd);
		}
		break;
		case ID_POPUPMENU0_40052: // 時計表示
		{
			if (GetMenuState(hmenu, param, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_UNCHECKED);
				display_size.clock = FALSE;
				if (pMadVROsdServices != NULL) {
					//memset(ppv_clock, 0, 3840 * 2160 * sizeof(UINT32));
					//pMadVROsdServices->OsdSetBitmap("clock", bitmap_clock, 0, 0, 0, 0, TRUE, 0, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					display_clock(0);
				}
			}
			else {
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_CHECKED);
				if (pMadVROsdServices != NULL) {
					if (g_second > 0) {
						display_clock(1);
					}
				}
				display_size.clock = TRUE;
			}
			SetActiveWindow(hWnd);
		}
		break;
		case ID_POPUPMENU0_40051: // フルスクリーン表示
		{
			if (GetMenuState(hmenu, param, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			else {
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_CHECKED);
			}
			PostMessage(hWnd, WM_LBUTTONDBLCLK, 0, 0);
		}
		break;
		case ID_POPUPMENU0_40023: // 常に前面に表示
		{
			RECT w = {};
			GetWindowRect(hWnd, &w);

			// 前回選択のチェックを外す
			if (GetMenuState(hmenu, param, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_UNCHECKED);
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, w.right - w.left, w.bottom - w.top, SWP_NOMOVE | SWP_NOSIZE);
				display_size.top_most = FALSE;
			}
			else {
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_CHECKED);
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, w.right - w.left, w.bottom - w.top, SWP_NOMOVE | SWP_NOSIZE);
				display_size.top_most = TRUE;
			}
			SetActiveWindow(hWnd);
		}
		break;
		case ID_POPUPMENU0_40003: // タイトルバー非表示
		{
			if (full_screen) {
				break;
			}

			// 前回選択のチェックを外す
			if (GetMenuState(hmenu, param, MF_BYCOMMAND) & MFS_CHECKED) { // 非表示から表示へ
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_UNCHECKED);
				display_size.title_bar = TRUE;

				RECT c = {};
				GetClientRect(hWnd, &c);
				int width, height;
				height = c.bottom - c.top;
				width = (int)(height * (16.0 / 9.0));

				SetWindowPos(hWnd, NULL, 0, 0, width, height + extra_height, SWP_NOMOVE | SWP_NOZORDER);
				SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			}
			else { // 表示から非表示へ
				CheckMenuItem(hmenu, param, MF_BYCOMMAND | MFS_CHECKED);
				display_size.title_bar = FALSE;

				RECT w = {};
				GetWindowRect(hWnd, &w);
				int width, height;
				width = w.right - w.left;
				height = (int)(width * (9.0 / 16.0));

				SetWindowPos(hWnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
				SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPED);
			}

			// ウインドウを表示する
			::ShowWindow(hWnd, SW_SHOW);
			SetActiveWindow(hWnd);
		}
		break;
		case ID_40040: // 1/6 表示サイズ
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 640;
			display_size.height = 360;
			display_size.command = ID_40040;
			break;
		case ID_40041: // 1/5
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 768;
			display_size.height = 432;
			display_size.command = ID_40041;
			break;
		case ID_40042: // 1/4
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 960;
			display_size.height = 540;
			display_size.command = ID_40042;
			break;
		case ID_40043: // 1/3
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 1280;
			display_size.height = 720;
			display_size.command = ID_40043;
			break;
		case ID_40044: // 1/2
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 1920;
			display_size.height = 1080;
			display_size.command = ID_40044;
			break;
		case ID_40045: // 3/4 表示サイズ
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 2880;
			display_size.height = 1620;
			display_size.command = ID_40045;
			break;
		case ID_40046: // 1/1
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 3840;
			display_size.height = 2160;
			display_size.command = ID_40046;
			break;
		case ID_40047: // 2/1
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.width = 7680;
			display_size.height = 4320;
			display_size.command = ID_40047;
			break;
		case ID_POPUPMENU0_40024: // 再生停止
		{
			OutputDebug(L"再生停止\n");
			ULONGLONG timeout = GetTickCount64();
			while (channel_status == CHANNEL_STATUS_SELECTING) { // チューナー選局中は待機
				Sleep(10);
				if ((GetTickCount64() - timeout) > 3000) {
					break;
				}
			}
			Sleep(100);

			DWORD dwExitCode;
			GetExitCodeThread(hThread, &dwExitCode);
			if (dwExitCode == STILL_ACTIVE) {
				puts("KILL THREAD");
				TerminateThread(hThread, dwExitCode);
				channel_select_thread_running = FALSE;
			}


			timeKillEvent(timerMMTP);
			timeKillEvent(timer);
			timeKillEvent(timer_thread);

			if (pMediaControl != NULL) {
				pMediaControl->Stop();
			}
			g_record = 0;

			if (pVideoWindow != NULL) {
				pVideoWindow->put_Visible(OAFALSE);
				pVideoWindow->put_Owner(NULL);
				pVideoWindow->SetWindowPosition(0, 0, 0, 0);
				pVideoWindow->put_MessageDrain(NULL);
				SAFE_RELEASE(pVideoWindow);
			}

			channel_status = CHANNEL_STATUS_HALT;
			InvalidateRect(m_hWnd, NULL, TRUE);
			SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

			SetActiveWindow(hWnd);
		}
		break;
		case ID_50001: // ボリューム最大
			// 前回選択ボリュームのチェックを外す
			if (GetMenuState(hmenu, display_size.volume_command, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			display_size.level = 0;
			display_size.volume_command = param;
			if (pBasicAudio != NULL) {
				pBasicAudio->put_Volume(display_size.level);

			}
			// 今回選択ボリュームのチェックを入れる
			CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_CHECKED);
			SetActiveWindow(hWnd);
			break;
		case ID_50002: // ボリューム大
			// 前回選択ボリュームのチェックを外す
			if (GetMenuState(hmenu, display_size.volume_command, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			display_size.level = -500;
			display_size.volume_command = param;
			if (pBasicAudio != NULL) {
				pBasicAudio->put_Volume(display_size.level);

			}
			// 今回選択ボリュームのチェックを入れる
			CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_CHECKED);
			SetActiveWindow(hWnd);
			break;
		case ID_50003: // ボリューム中
			// 前回選択ボリュームのチェックを外す
			if (GetMenuState(hmenu, display_size.volume_command, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			display_size.level = -900;
			display_size.volume_command = param;
			if (pBasicAudio != NULL) {
				pBasicAudio->put_Volume(display_size.level);

			}
			// 今回選択ボリュームのチェックを入れる
			CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_CHECKED);
			SetActiveWindow(hWnd);
			break;
		case ID_50004: // ボリューム小
			// 前回選択ボリュームのチェックを外す
			if (GetMenuState(hmenu, display_size.volume_command, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			display_size.level = -1500;
			display_size.volume_command = param;
			if (pBasicAudio != NULL) {
				pBasicAudio->put_Volume(display_size.level);

			}
			// 今回選択ボリュームのチェックを入れる
			CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_CHECKED);
			SetActiveWindow(hWnd);
			break;
		case ID_50005: // 消音（ミュート）
			// 前回選択ボリュームのチェックを外す
			if (GetMenuState(hmenu, display_size.volume_command, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			display_size.level = -10000;
			display_size.volume_command = param;
			if (pBasicAudio != NULL) {
				pBasicAudio->put_Volume(display_size.level);

			}
			// 今回選択ボリュームのチェックを入れる
			CheckMenuItem(hmenu, display_size.volume_command, MF_BYCOMMAND | MFS_CHECKED);
			SetActiveWindow(hWnd);
			break;
		case ID_40002: // ID_40002からID_40012までチャンネル選択
			ch = 0;
			break;
		case ID_40003:
			ch = 1;
			break;
		case ID_40004:
			ch = 2;
			break;
		case ID_40005:
			ch = 3;
			break;
		case ID_40006:
			ch = 4;
			break;
		case ID_40007:
			ch = 5;
			break;
		case ID_40008:
			ch = 6;
			break;
		case ID_40009:
			ch = 7;
			break;
		case ID_40010:
			ch = 8;
			break;
		case ID_40011:
			ch = 9;
			break;
		case ID_40012:
			ch = 10;
			break;
		case ID_POPUPMENU0_40011: // 番組情報
			if (!IsWindowVisible(hdlgContent)) {
				GetWindowRect(hdlgContent, &rect);
				MoveWindow(hdlgContent, po.x + 50, po.y, rect.right - rect.left, rect.bottom - rect.top, TRUE);

				ShowWindow(hdlgContent, SW_SHOW);
				HideCaret(hEdit);

				TabCtrl_SetCurSel(hTab, 0);
				nmhdr.idFrom = IDC_TAB1;
				nmhdr.code = TCN_SELCHANGE;
				SendNotifyMessage(hdlgContent, WM_NOTIFY, 0, (LPARAM)&nmhdr);
			}
			else {
				ShowWindow(hdlgContent, SW_HIDE);
			}

			//　チェック
			uState = GetMenuState(hmenu, ID_POPUPMENU0_40011, MF_BYCOMMAND);
			if (uState & MFS_CHECKED) {
				CheckMenuItem(hmenu, ID_POPUPMENU0_40011, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			else {
				CheckMenuItem(hmenu, ID_POPUPMENU0_40011, MF_BYCOMMAND | MFS_CHECKED);
			}
			break;
		case ID_POPUPMENU0_50000: // デバッグ情報
			if (!IsWindowVisible(hdlg)) {
				GetWindowRect(hdlg, &rect);
				MoveWindow(hdlg, po.x + 50, po.y, rect.right - rect.left, rect.bottom - rect.top, TRUE);
				ShowWindow(hdlg, SW_SHOW);
			}
			else {
				ShowWindow(hdlg, SW_HIDE);
			}

			//　チェック
			uState = GetMenuState(hmenu, ID_POPUPMENU0_50000, MF_BYCOMMAND);
			if (uState & MFS_CHECKED) {
				CheckMenuItem(hmenu, ID_POPUPMENU0_50000, MF_BYCOMMAND | MFS_UNCHECKED);
			}
			else {
				CheckMenuItem(hmenu, ID_POPUPMENU0_50000, MF_BYCOMMAND | MFS_CHECKED);
			}
			break;
		case ID_POPUPMENU0_40002: // 終了
			SendMessage(hWnd, WM_DESTROY, 0, 0);
			break;
		default:
			break;
		}

		if ((param == ID_40040) || (param == ID_40041) || (param == ID_40042) || (param == ID_40043) || (param == ID_40044) || (param == ID_40045) || (param == ID_40046) || (param == ID_40047)) { // 表示サイズ変更
			if (full_screen) {
				SetWindowPos(hWnd, NULL, bkRect.left, bkRect.top, bkRect.right - bkRect.left, bkRect.bottom - bkRect.top, SWP_SHOWWINDOW | SWP_NOZORDER);
				SetWindowLong(hWnd, GWL_STYLE, IStyle);
				if (pVideoWindow != NULL) {
					RECT recta = {};
					GetClientRect(hWnd, &recta);
					pVideoWindow->SetWindowPosition(0, 0, recta.right - recta.left, recta.bottom - recta.top);
					if (hideCursor) {
						pVideoWindow->HideCursor(OAFALSE);
						hideCursor = 0;
					}
				}
				full_screen = 0;
			}

			if (display_size.title_bar) {
				SetWindowPos(hWnd, NULL, 0, 0, display_size.width, display_size.height + extra_height, SWP_NOMOVE | SWP_NOZORDER);
			}
			else {
				SetWindowPos(hWnd, NULL, 0, 0, display_size.width, display_size.height, SWP_NOMOVE | SWP_NOZORDER);
			}

			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_CHECKED); // 表示サイズ選択チェック


			// ウインドウを表示する
			::ShowWindow(hWnd, SW_SHOW);
			SetActiveWindow(hWnd);
		}


		if ((param == ID_40002) || (param == ID_40003) || (param == ID_40004) || (param == ID_40005) || (param == ID_40006) || (param == ID_40007) || (param == ID_40008) || (param == ID_40009) || (param == ID_40010) || (param == ID_40011) || (param == ID_40012)) { // チャンネル選択

			if (channel_select_thread_running == TRUE) { // チャンネル切り替えスレッド動作中ならキャンセル
				break;
			}

			if (pMediaControl != NULL) {
				pMediaControl->Stop();
			}
			g_record = 0;

			timeKillEvent(timerMMTP);
			timeKillEvent(timer);

			if (hFileMMTP != NULL) {
				CloseHandle(hFileMMTP);
				hFileMMTP = NULL;
			}

			g_second = -1;
			if (pMadVROsdServices != NULL) {
				memset(ppv_clock, 0, 3840 * 2160 * sizeof(UINT32));
				pMadVROsdServices->OsdSetBitmap("clock", bitmap_clock, 0, 0, 0, 0, TRUE, 1, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);

				memset(ppv_ui, 0, 3840 * 2160 * sizeof(UINT32));
				pMadVROsdServices->OsdSetBitmap("ui", bitmap_ui, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
			}


			for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
				timeKillEvent(timer_subtitle_thread[i]);
				timer_subtitle_thread[i] = NULL;
				Sleep(1);
			}

			// カレントメニューアイテムの文字列をウィンドウテキストにセット
			std::wstring titleStr = L"Xview 4K";
			std::wstring titleContentStr = L"番組情報";
			if (LONG len = GetMenuString(hmenu, param, NULL, 0, MF_BYCOMMAND)) {
				GetMenuString(hmenu, param, channel_title_str, len + 1, MF_BYCOMMAND);
				titleStr.append(L" - ");
				titleStr.append(channel_title_str);
				SetWindowText(hWnd, titleStr.c_str());
				StrCpyW(titleBarText, titleStr.data());

				titleContentStr.append(L" - ");
				titleContentStr.append(channel_title_str);
				SetWindowText(hdlgContent, titleContentStr.c_str());
			}
			titleStr.clear();
			titleContentStr.clear();
			SetWindowText(hEdit, L"");

			// 前回選択チャンネルのチェックを外す
			if (GetMenuState(hmenu, display_size.select_command_ch, MF_BYCOMMAND) & MFS_CHECKED) {
				CheckMenuItem(hmenu, display_size.select_command_ch, MF_BYCOMMAND | MFS_UNCHECKED);
			}

			display_size.select_ch = ch;
			display_size.select_command_ch = param;

			// 今回選択チャンネルのチェックを入れる
			CheckMenuItem(hmenu, display_size.select_command_ch, MF_BYCOMMAND | MFS_CHECKED);

			ReleaseGraphs();

			channel_status = CHANNEL_STATUS_SELECTING;
			InvalidateRect(m_hWnd, NULL, TRUE);
			SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

			HRESULT hr;
			if (FAILED(hr = CreateGraphs())) {
				return E_FAIL;
			}

			if (FAILED(hr = pGraphBuilder->QueryInterface(IID_IVideoWindow, (void**)&pVideoWindow))) {
				MessageBox(NULL, L"VideoWindow QI from GraphBuilder failed", L"Error", MB_OK | MB_TOPMOST);
				return E_FAIL;
			}
			else {
				pVideoWindow->put_Owner((OAHWND)m_hWnd);
				pVideoWindow->put_Visible(OAFALSE);
				pVideoWindow->put_WindowStyle(WS_OVERLAPPED);
				pVideoWindow->put_BackgroundPalette(0);
				pVideoWindow->put_Left(0);
				pVideoWindow->put_Top(0);
			}

			CloseHandle(hThread);
			hThread = CreateThread(NULL, 0, ThreadChannelSelectFunc, NULL, 0, &threadID);
			if ((timer_thread = timeSetEvent(15000, 1, (LPTIMECALLBACK)TimerThreadTimeoutCallBack, (DWORD)NULL, TIME_ONESHOT)) == 0) {
				printf("time event fail\n");
			}

			//SetActiveWindow(hWnd);
		}
	}
	break;
	case WM_NCLBUTTONDBLCLK:
	{
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		POINT pos = {};
		POINT poc = {};
		GetCursorPos(&pos);
		memcpy(&poc, &pos, sizeof(POINT));
		ScreenToClient(hWnd, &poc);

		if ((pVideo != NULL) && (hFileMMTP != NULL)) {
			DOUBLE fx, fy;
			RECT madRect = {};
			IMadVRInfo* pMadVRInfo;
			if (pVideo->QueryInterface(__uuidof(IMadVRInfo), (void**)&pMadVRInfo) == S_OK) {
				pMadVRInfo->GetRect("croppedVideoOutputRect", &madRect);

				SAFE_RELEASE(pMadVRInfo);
			}

			fx = (DOUBLE)((LONGLONG)poc.x - (LONGLONG)madRect.left) / (DOUBLE)((LONGLONG)madRect.right - (LONGLONG)madRect.left);
			fx = fx < 0 ? 0 : fx, fx = fx > 1 ? 1 : fx;

			fy = (DOUBLE)((LONGLONG)poc.y - (LONGLONG)madRect.top) / (DOUBLE)((LONGLONG)madRect.bottom - (LONGLONG)madRect.top);
			fy = fy < 0 ? 0 : fy, fy = fy > 1 ? 1 : fy;

			LONG posX, posY;
			posX = fx * 3840;
			posY = fy * 2160;

			if ((posX >= PANEL_X) && (posX <= (PANEL_X + PANEL_WIDTH)) && (posY >= PANEL_Y) && (posY <= (PANEL_Y + PANEL_HEIGHT))) {
				return 0;
			}
		}

		if (!full_screen) { // フルスクリーンモードへ移行

			RECT w = {};
			GetWindowRect(hWnd, &w);
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, w.right - w.left, w.bottom - w.top, SWP_NOMOVE | SWP_NOSIZE);


			// 現在のウインドウレクトを保存
			GetWindowRect(hWnd, &display_size.WindowRect);

			GetWindowRect(hWnd, &bkRect);
			IStyle = GetWindowLong(hWnd, GWL_STYLE);
			SetWindowLong(hWnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPED);

			RECT deskRect = {};
			GetWindowRect(GetDesktopWindow(), &deskRect);
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, deskRect.right - deskRect.left, deskRect.bottom - deskRect.top, SWP_SHOWWINDOW | SWP_NOZORDER);
			if (pVideoWindow != NULL) {
				RECT rect = {};
				GetClientRect(hWnd, &rect);
				pVideoWindow->SetWindowPosition(0, 0, rect.right - rect.left, rect.bottom - rect.top);
				if (hideCursor == 0) {
					pVideoWindow->HideCursor(OATRUE);
					hideCursor = 1;
				}
			}
			full_screen = 1;
			CheckMenuItem(hmenu, ID_POPUPMENU0_40051, MF_BYCOMMAND | MFS_CHECKED);

		}
		else { // 通常モードに移行
			SetWindowPos(hWnd, NULL, bkRect.left, bkRect.top, bkRect.right - bkRect.left, bkRect.bottom - bkRect.top, SWP_SHOWWINDOW | SWP_NOZORDER);
			SetWindowLong(hWnd, GWL_STYLE, IStyle);
			if (pVideoWindow != NULL) {
				RECT rect = {};
				GetClientRect(hWnd, &rect);
				pVideoWindow->SetWindowPosition(0, 0, rect.right - rect.left, rect.bottom - rect.top);
				if (hideCursor) {
					pVideoWindow->HideCursor(OAFALSE);
					hideCursor = 0;
				}
			}
			full_screen = 0;
			CheckMenuItem(hmenu, ID_POPUPMENU0_40051, MF_BYCOMMAND | MFS_UNCHECKED);

			SetWindowPos(hWnd, display_size.top_most == TRUE ? HWND_TOPMOST : NULL, bkRect.left, bkRect.top, bkRect.right - bkRect.left, bkRect.bottom - bkRect.top, SWP_NOMOVE | SWP_NOSIZE);
		}
	}
	break;
	case WM_KILLFOCUS:
	{
	}
	break;
	case WM_RBUTTONDOWN:
	{
		po.x = LOWORD(lParam);
		po.y = HIWORD(lParam);

		if (!full_screen) {
			ClientToScreen(hWnd, &po);
		}
		TrackPopupMenu(hmenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, po.x, po.y, 0, hWnd, NULL);
	}
	break;
	case WM_SIZING:
	{
		if (!full_screen && (display_size.command != -1)) {
			CheckMenuItem(hmenu, display_size.command, MF_BYCOMMAND | MFS_UNCHECKED); // 前回選択項目チェックはずす
			display_size.command = -1;
		}
	}
	break;
	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
	{
		if ((pVideoWindow != NULL) && !full_screen) {
			RECT rect = {};
			GetClientRect(hWnd, &rect);
			pVideoWindow->SetWindowPosition(0, 0, rect.right - rect.left, rect.bottom - rect.top);
			UpdateWindow(hWnd);
		}
	}
	break;
	case WM_PAINT:
	{
		hdc = BeginPaint(hWnd, &ps);
		hFont = CreateFont(50, 0, 0, 0, FW_SEMIBOLD, TRUE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, _T("メイリオ"));
		SelectObject(hdc, hFont);

		HDC hMdc;
		HBITMAP hbmp;
		//BMP画像をファイルから読み込む
		hbmp = (HBITMAP)LoadBitmap(ghInst, MAKEINTRESOURCE(IDB_BITMAP1));
		hMdc = CreateCompatibleDC(hdc);
		SelectObject(hMdc, hbmp);

		RECT rect = {};
		GetClientRect(hWnd, &rect);

		BitBlt(hdc, ((rect.right - rect.left) - 723) / 2, ((rect.bottom - rect.top) - 588) / 2, 723, 588, hMdc, 0, 0, SRCCOPY);
		//BitBlt(hdc, ((rect.right - rect.left) - 297) / 2, ((rect.bottom - rect.top) - 299) / 2, 297, 299, hMdc, 0, 0, SRCCOPY);

		DeleteDC(hMdc);
		DeleteObject(hbmp);
		//		SetBkColor(hdc, RGB(0, 0, 0));

		switch (channel_status)
		{
		case CHANNEL_STATUS_HALT: // 停止メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(0, 255, 0));
			TextOut(hdc, 10, 10, channel_title_str, lstrlen(channel_title_str));
			TextOut(hdc, 10, 60, STOP_NOW, lstrlen(STOP_NOW));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		case CHANNEL_STATUS_TUNER_OPENING: // チューナー初期化中メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(0, 255, 0));
			TextOut(hdc, 10, 10, TUNER_OPEN, lstrlen(TUNER_OPEN));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		case CHANNEL_STATUS_TUNER_OPEN_FAILED: // チューナー初期化失敗メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(255, 0, 0));
			TextOut(hdc, 10, 10, TUNER_OPEN_FAILED, lstrlen(TUNER_OPEN_FAILED));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		case CHANNEL_STATUS_SELECTING: // 選局中メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(0, 255, 0));
			TextOut(hdc, 10, 10, channel_title_str, lstrlen(channel_title_str));
			TextOut(hdc, 10, 60, SELECTING, lstrlen(SELECTING));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		case CHANNEL_STATUS_RECORD_FILE_LOADING: // 録画ファイル読み込み中メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(0, 255, 0));
			TextOut(hdc, 10, 10, channel_title_str, lstrlen(channel_title_str));
			TextOut(hdc, 10, 60, RECORD_FILE_LOADING, lstrlen(RECORD_FILE_LOADING));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		case CHANNEL_STATUS_BUFFERING: // バッファリング中メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(0, 255, 0));
			TextOut(hdc, 10, 10, channel_title_str, lstrlen(channel_title_str));
			TextOut(hdc, 10, 60, BUFFERING, lstrlen(BUFFERING));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		case CHANNEL_STATUS_SELECT_FAILED: // 選局失敗メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(255, 0, 0));
			TextOut(hdc, 10, 10, channel_title_str, lstrlen(channel_title_str));
			TextOut(hdc, 10, 60, SELECT_FAILED, lstrlen(SELECT_FAILED));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		case CHANNEL_STATUS_SCRAMBLE: // スクランブル放送メッセージ
		{
			SetBkColor(hdc, RGB(0, 0, 0));
			SetTextColor(hdc, RGB(255, 255, 0));
			TextOut(hdc, 10, 10, channel_title_str, lstrlen(channel_title_str));
			TextOut(hdc, 10, 60, SCRAMBLE_EVENT, lstrlen(SCRAMBLE_EVENT));
			SelectObject(hdc, GetStockObject(SYSTEM_FONT));
			DeleteObject(hFont);
		}
		break;
		default:
		{

		}
		break;
		}

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_CLOSE:
	case WM_DESTROY:
	{
		if (!full_screen) {
			// 現在のウインドウレクトを保存
			GetWindowRect(hWnd, &display_size.WindowRect);
		}

		// 終了する（ 引数はそのまま終了コードとなります ）
		DestroyWindow(hdlg);
		DestroyWindow(hdlgContent);
		::PostQuitMessage(0);
		return 0;
	}
	break;
	}

	// デフォルト処理呼び出し
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//int _tmain(int argc, char* argv[])
int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	//	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	INT nArgc;
	TCHAR** lppArgv = CommandLineToArgvW(GetCommandLine(), &nArgc);

	ghInst = hInstance;

	HMODULE hModule = NULL;
	extern BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID /* lpReserved */);
	DllMain(hModule, DLL_PROCESS_ATTACH, 0);

	HRESULT hr;
	if (FAILED(hr = CoInitialize(NULL))) {
		MessageBox(NULL, L"CoInitialize failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	////acas
	//int code, r;
	//res.return_code = 0;

	//acas = create_a_cas_card();
	//if (acas == NULL) {
	//	printf("error - failed on create_a_cas_card()");
	//}
	//if (acas != NULL) {
	//	code = acas->init(acas);//connect_card command
	//	if (code < 0) {
	//		printf("error - failed on A_CAS_CARD::init() : code=%d\n", code);
	//	}
	//}
	//r = acas->get_init_status(acas, &is);
	//if (r < 0) {
	//	printf("error - INVALID_A_CAS_STATUS");;
	//}
	////may need get_id_a_cas_card
	//acas->get_id(acas, &casid);
	////don't call show_acas_power_on_control_info
	//acas->scramble_key(acas, &casid, &caskey);

	tuner = new CBonTuner;

	if (tuner->OpenTuner() == FALSE) {
		OutputDebug(L"Tuner open failed\n");
		puts("Tuner open failed");
	}
	else {
		OutputDebug(L"Tuner opend successfully\n");
		puts("Tuner opend successfully");
	}

	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

	TCHAR szPath[_MAX_PATH];
	TCHAR szDir[_MAX_DIR];
	TCHAR szDrive[_MAX_PATH];
	TCHAR szFilePath[_MAX_PATH];

	GetModuleFileNameW(NULL, szPath, MAX_PATH);

	_wsplitpath(szPath, szDrive, szDir, NULL, NULL);

	wsprintfW(szFilePath, _T("%s%sconfig"), szDrive, szDir);

	BOOL config_ok = FALSE;
	HANDLE hConfigFile;
	if ((hConfigFile = CreateFile(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL)) == INVALID_HANDLE_VALUE) {
		wprintf(L"Cannot open %s\n", szFilePath);
	}
	else {
		DWORD reads;
		if (ReadFile(hConfigFile, &display_size, sizeof(display_size), &reads, NULL) == 0) {
			wprintf(L"Read Error: %s\n", szFilePath);
		}
		else {
			config_ok = TRUE;
		}
		CloseHandle(hConfigFile);
	}

	WNDCLASSEX tWndClass;
	// ウインドウクラスパラメータセット
	tWndClass.cbSize = sizeof(WNDCLASSEX);
	tWndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	tWndClass.lpfnWndProc = eMainWindowProc;
	tWndClass.cbClsExtra = 0;    // ::GetClassLong で取得可能なメモリ
	tWndClass.cbWndExtra = 0;    // ::GetWindowLong で取得可能なメモリ
	tWndClass.hInstance = hInstance;
	tWndClass.hIcon = ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	tWndClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	tWndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_PEN);
	tWndClass.lpszMenuName = L"none";
	tWndClass.lpszClassName = L"none";
	tWndClass.hIconSm = NULL;


	// ウインドウクラス生成
	if (0 == ::RegisterClassEx(&tWndClass)) {

		/* 失敗 */
		return(-1);
	}

	INT xPos = 0, yPos = 0, width = 1920, height = 1080;
	if (config_ok) {

		xPos = display_size.WindowRect.left;
		yPos = display_size.WindowRect.top;

		if (display_size.command == -1) {
			width = display_size.WindowRect.right - display_size.WindowRect.left;
			height = display_size.WindowRect.bottom - display_size.WindowRect.top;
		}
		else {
			width = display_size.width;
			height = display_size.height;
		}
	}
	else {
		RECT deskRect;
		GetWindowRect(GetDesktopWindow(), &deskRect);

		xPos = ((deskRect.right - deskRect.left) - display_size.width) / 2;
		yPos = ((deskRect.bottom - deskRect.top) - display_size.height) / 2;
		width = display_size.width;
		height = display_size.height;
	}

	m_hWnd = ::CreateWindowEx(
		0                       // extended window style
		, tWndClass.lpszClassName // pointer to registered class name
		, L"Xview4K"            // pointer to window name
		, WS_OVERLAPPEDWINDOW // window style
		, xPos           // horizontal position of window
		, yPos           // vertical position of window
		, width                     // window width
		, height                    // window height
		, NULL                    // handle to parent or owner window
		, NULL                    // handle to menu, or child-window identifier
		, hInstance               // handle to application instance
		, (VOID*)0x12345678       // pointer to window-creation data
	);


	channel_status = CHANNEL_STATUS_TUNER_OPENING;
	InvalidateRect(m_hWnd, NULL, TRUE);
	SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

	Sleep(500);

	if (tuner->IsTunerOpening()) {
		channel_status = CHANNEL_STATUS_HALT;
		InvalidateRect(m_hWnd, NULL, TRUE);
		SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);
	}
	else {
		if (nArgc <= 1) {
			MessageBeep(MB_OK);
			channel_status = CHANNEL_STATUS_TUNER_OPEN_FAILED;
			InvalidateRect(m_hWnd, NULL, TRUE);
			SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);
		}
	}

	BITMAPV5HEADER bmh;
	memset(&bmh, 0, sizeof(BITMAPV5HEADER));
	bmh.bV5Size = sizeof(BITMAPV5HEADER);
	bmh.bV5Width = 3840;
	bmh.bV5Height = 2160;
	bmh.bV5Planes = 1;
	bmh.bV5BitCount = 32;
	bmh.bV5Compression = BI_RGB;
	bmh.bV5AlphaMask = 0xFF000000;
	bmh.bV5RedMask = 0x00FF0000;
	bmh.bV5GreenMask = 0x0000FF00;
	bmh.bV5BlueMask = 0x000000FF;

	bitmap_sub = CreateDIBSection(NULL, (BITMAPINFO*)&bmh, DIB_RGB_COLORS, (void**)&ppv_sub, NULL, 0);
	bitmap_clock = CreateDIBSection(NULL, (BITMAPINFO*)&bmh, DIB_RGB_COLORS, (void**)&ppv_clock, NULL, 0);
	bitmap_ui = CreateDIBSection(NULL, (BITMAPINFO*)&bmh, DIB_RGB_COLORS, (void**)&ppv_ui, NULL, 0);

	for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
		timer_subtitle_thread[i] = NULL;
	}


	//char system_path[_MAX_PATH];
	//DWORD dwSize = sizeof(system_path);
	//GetEnvironmentVariableA("windir", system_path, dwSize);

	//std::string font_path(system_path);
	//	font_path.append("\\Fonts\\YuGothB.ttc");
	//	font_path.append("\\Fonts\\UDDigiKyokashoN-B.ttc");
	//	font_path.append("HiraginoMaruProNW4.ttc");

	//	FT_Error ft_error;
	//ft_error = FT_New_Face(library, "HiraginoMaruProNW4.ttc", 0, &face);
	//if (ft_error) {
	//	OutputDebug(L"FT_Error\n");
	//}

	HGLOBAL hLoadedResourceFont = NULL;
	LPVOID pLockedResourceFont = NULL;
	DWORD dwResourceFontSize = 0;

	HRSRC hResourceFont = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT1), RT_FONT);

	if (hResourceFont)
	{
		hLoadedResourceFont = LoadResource(hInstance, hResourceFont);
		if (hLoadedResourceFont)
		{
			pLockedResourceFont = LockResource(hLoadedResourceFont);
			if (pLockedResourceFont)
			{
				dwResourceFontSize = SizeofResource(hInstance, hResourceFont);
			}
		}
	}

	FT_Init_FreeType(&library);
	FT_Init_FreeType(&library2);

	FT_Error ft_error;
	ft_error = FT_New_Memory_Face(library, (FT_Byte*)pLockedResourceFont, (FT_Long)dwResourceFontSize, 0, &face);
	if (ft_error) {
		OutputDebug(L"FT_Error\n");
	}

	ft_error = FT_New_Memory_Face(library2, (FT_Byte*)pLockedResourceFont, (FT_Long)dwResourceFontSize, 0, &face2);
	if (ft_error) {
		OutputDebug(L"FT_Error\n");
	}


	FT_Stroker_New(library, &stroker);
	FT_Stroker_New(library2, &stroker2);


	buffer0 = new BYTE[BUFFER_SIZE];
	buffer1 = new BYTE[BUFFER_SIZE];

	if (nArgc > 1) {
		// 録画ファイル再生
		funcFileOpen(m_hWnd, lppArgv[1]);
		//		MessageBox(NULL, lppArgv[1], L"Message", NULL);
	}
	else {
		// 自動再生
		if (tuner->IsTunerOpening()) {
			PostMessage(m_hWnd, WM_COMMAND, display_size.select_command_ch, 0);
		}
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (pMediaControl != NULL) {
		pMediaControl->Stop();
	}
	g_record = 0;

	timeKillEvent(timerMMTP);
	timeKillEvent(timer);

	if (hFileMMTP != NULL) {
		CloseHandle(hFileMMTP);
	}

	for (int i = 0; i < XML_SUB_BUFFER_NUM; i++) {
		timeKillEvent(timer_subtitle_thread[i]);
		timer_subtitle_thread[i] = NULL;
		Sleep(1);
	}

	FT_Stroker_Done(stroker);
	FT_Stroker_Done(stroker2);
	FT_Done_Face(face);
	FT_Done_Face(face2);
	FT_Done_FreeType(library);
	FT_Done_FreeType(library2);

	DeleteObject(bitmap_clock);
	DeleteObject(bitmap_sub);
	DeleteObject(bitmap_ui);

	ReleaseGraphs();

	tuner->CloseTuner();
	tuner->Release();

	SAFE_DELETE_ARRAY(buffer0);
	SAFE_DELETE_ARRAY(buffer1);

	//Close ACAS IC card
	//acas->release;

	CoUninitialize();

	if ((hConfigFile = CreateFile(szFilePath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL)) == INVALID_HANDLE_VALUE) {
		wprintf(L"Cannot open %s\n", szFilePath);
	}
	else {
		DWORD written;
		if (WriteFile(hConfigFile, &display_size, sizeof(display_size), &written, NULL) == 0) {
			wprintf(L"Write Error: %s\n", szFilePath);
		}
		CloseHandle(hConfigFile);
	}

	return S_OK;
}


HRESULT startCaptureGraph()
{
	HRESULT hr;

	InvalidateRect(m_hWnd, NULL, TRUE);
	SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

	g_record = 1;
	ULONGLONG tick = GetTickCount64();

	do {
		if (g_record) {
			if ((GetTickCount64() - tick) >= 4000) {
				return E_FAIL;
			}
			if (((audio_frame_buffer->writePos > LAV_START_TO_PROCESS_READ_SIZE) && tlv->setup)) {
				if (FAILED(hr = pCaptureGraphBuilder2->RenderStream(0, 0, pAudioSrc, pSplitterAudioSrc, pAudio))) {
					//g_record = 0;

					if (pVideoWindow != NULL) {
						pVideoWindow->put_Visible(OAFALSE);
						pVideoWindow->put_MessageDrain(NULL);
						pVideoWindow->put_Owner(NULL);
						SAFE_RELEASE(pVideoWindow);
					}
					channel_status = CHANNEL_STATUS_HALT;
					InvalidateRect(m_hWnd, NULL, TRUE);
					SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

					MessageBox(NULL, L"Audio RenderStream failed", L"Error", MB_OK | MB_TOPMOST);

					return E_FAIL;
				}

				time_to_start_graph = GetTickCount64() - tick;

				IMediaSeeking* m_pMS;
				if (FAILED(hr = pAudio->QueryInterface(IID_IMediaSeeking, (void**)&m_pMS))) {
					puts("pAudio QI IMediaSeeking failed");
				}
				else {
					REFERENCE_TIME current = audio_delay[display_size.select_ch] * 1000 * 10;//500 * 1000 * 10; // 500ms audio shift
					if (FAILED(hr = m_pMS->SetPositions(&current, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning))) {
						puts("SetPositions failed");
					}
					SAFE_RELEASE(m_pMS);
				}

				int i,retry_count = 5;
				for (i = 0; i < retry_count+1; i++) {
					hr = pCaptureGraphBuilder2->RenderStream(0, 0, pVideoSrc, pSplitterVideoSrc, pVideo);
					if (FAILED(hr) && i== retry_count) {
					// if (FAILED(hr = pCaptureGraphBuilder2->RenderStream(0, 0, pVideoSrc, pSplitterVideoSrc, pVideo))) {
						//g_record = 0;

						if (pVideoWindow != NULL) {
							pVideoWindow->put_Visible(OAFALSE);
							pVideoWindow->put_MessageDrain(NULL);
							pVideoWindow->put_Owner(NULL);
							SAFE_RELEASE(pVideoWindow);
						}
						channel_status = CHANNEL_STATUS_HALT;
						InvalidateRect(m_hWnd, NULL, TRUE);
						SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

						MessageBox(NULL, L"Video RenderStream failed, count=5", L"Error", MB_OK | MB_TOPMOST);

						return E_FAIL;
					}
					else if (FAILED(hr) ){
						Sleep(10);
					}
					else { 
						//MessageBox(NULL, (LPCWSTR)i, L"Message", NULL);
						break;
					}
				}

				//CComPtr<IMFVideoDisplayControl> m_Vdc;
				//CComQIPtr<IMFGetService> service(pVideo);
				//hr = service->GetService(MR_VIDEO_RENDER_SERVICE, IID_IMFVideoDisplayControl, (void**)&m_Vdc);
				//hr = m_Vdc->SetVideoWindow(m_hWnd);
				//MFVideoNormalizedRect mvnr = { 0, 0, 1, 1 };
				//RECT rect;
				//GetClientRect(m_hWnd, &rect);
				//hr = m_Vdc->SetVideoPosition(&mvnr, &rect);

				if (FAILED(hr = pAudio->QueryInterface(IID_IBasicAudio, (void**)&pBasicAudio))) {
					puts("pBasicAudio QI failed");
				}
				else {
					pBasicAudio->put_Volume(display_size.level);
				}

				//IMadVRTextOsd* pMadVRTextOsd;
				//if (pVideo->QueryInterface(__uuidof(IMadVRTextOsd), (void**)&pMadVRTextOsd) == S_OK) {
				//	pMadVRTextOsd->OsdDisplayMessage(L"あいうえおかきくけこ！", 1000000);
				//}

				memset(ppv_sub, 0, 3840 * 2160 * sizeof(UINT32));
				if (pVideo->QueryInterface(__uuidof(IMadVROsdServices), (void**)&pMadVROsdServices) == S_OK) {
					pMadVROsdServices->OsdSetBitmap("subtitle", bitmap_sub, 0, 0, 0, 0, TRUE, 0, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					pMadVROsdServices->OsdSetBitmap("clock", bitmap_clock, 0, 0, 0, 0, TRUE, 1, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					pMadVROsdServices->OsdSetBitmap("ui", bitmap_ui, 0, 0, 0, 0, TRUE, 2, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					ui_screen = 1;

					if (display_size.clock == TRUE) {
						if (g_second > 0) {
							display_clock(1);
						}
					}
					else {
						memset(ppv_clock, 0, 3840 * 2160 * sizeof(UINT32));
						pMadVROsdServices->OsdSetBitmap("clock", bitmap_clock, 0, 0, 0, 0, TRUE, 1, 0, BITMAP_STRETCH_TO_OUTPUT, NULL, NULL, NULL);
					}
					if (hFileMMTP != NULL) {
						Manipulate_UI(0, 0, COMMAND_MANIPULATE_UI_INIT);
					}

				}

				if (pVideo->QueryInterface(__uuidof(IMadVRCommand), (void**)&pMadVRCommand) == S_OK) {
				}

				//IMadVRExternalPixelShaders* pMadVRExternalPixelShaders;
				//if (pVideo->QueryInterface(__uuidof(IMadVRExternalPixelShaders), (void**)&pMadVRExternalPixelShaders) == S_OK) {
				//}

				/*
				LPCSTR shader_green = "sampler s0 : register(s0); \
					\
					float4 main(float2 tex : TEXCOORD0) : COLOR \
				{ \
					return float4(0, dot(tex2D(s0, tex), float4(.375, .5, .125, 0)), 0, 0); \
				}";

				LPCSTR shader_wave = \
				"\
				sampler s0 : register(s0);\
				float4 p0 : register(c0); \
				#define CLOCK (p0[3]) \
				float4 main(float2 tex : TEXCOORD0) : COLOR\
				{\
					float4 c0 = float4(0, 0, 0, 0); \
					tex.x += sin(tex.x + CLOCK / 0.3) / 20; \
					tex.y += sin(tex.x + CLOCK / 0.3) / 20; \
					if (tex.x >= 0 && tex.x <= 1 && tex.y >= 0 && tex.y <= 1) \
					{ \
						c0 = tex2D(s0, tex); \
					} \
					return c0; \
				}\
				";

				pMadVRExternalPixelShaders->AddPixelShader(shader_wave, "ps_2_0", ShaderStage_PreScale, NULL);
				*/


				const GUID LAV_Splitter = { 0x171252A0, 0x8820, 0x4AFE, { 0x9D, 0xF8, 0x5C, 0x92, 0xB2, 0xD6, 0x6B, 0x04} };
				const GUID LAV_VideoDecoder = { 0xEE30215D, 0x164F, 0x4A92, { 0xA4, 0xEB, 0x9D, 0x4C, 0x13, 0x39, 0x0F, 0x9F} };
				const GUID LAV_AudioDecoder = { 0xE8E73B6B, 0x4CB3, 0x44A4, { 0xBE, 0x99, 0x4F, 0x7B, 0xCB, 0x96, 0xE4, 0x91} };
				CLSID clsid;
				LPWSTR idGet;

				IEnumFilters* pEnum = NULL;
				if (SUCCEEDED(pGraphBuilder->EnumFilters(&pEnum)))
				{
					IBaseFilter* pFilter = NULL;
					while (S_OK == pEnum->Next(1, &pFilter, NULL))
					{
						pFilter->GetClassID(&clsid);
						IEnumPins* pPinEnum = NULL;
						if (SUCCEEDED(pFilter->EnumPins(&pPinEnum))) {
							IPin* pPin = NULL;
							while (S_OK == pPinEnum->Next(1, &pPin, NULL)) {
								pPin->QueryId(&idGet);
								if (memcmp(&clsid, &LAV_VideoDecoder, sizeof(GUID)) == 0) {
									pLAVVideoDecoder = pFilter;
								}
								else if (memcmp(&clsid, &LAV_AudioDecoder, sizeof(GUID)) == 0) {
									pLAVAudioDecoder = pFilter;
								}

								SAFE_RELEASE(pPin);
							}
							SAFE_RELEASE(pPinEnum);
						}
						SAFE_RELEASE(pFilter);
					}
					SAFE_RELEASE(pEnum);
				}

				pMediaControl->Run();

				pre_count = 0;
				elapsed = 0;
				if ((timer = timeSetEvent(1000, 0, (LPTIMECALLBACK)Timer1HzCallBack, (DWORD)NULL, TIME_PERIODIC)) == 0) {
					printf("time event fail\n");
				}

				break;
			}
		}
	} while (1);

	//	channel.status = CHANNEL_STATUS_HALT;
	channel_status = 999; // メッセージ消去
	InvalidateRect(m_hWnd, NULL, TRUE);
	SendNotifyMessage(m_hWnd, WM_PAINT, 0, 0);

	return S_OK;
}

HRESULT CreateGraphs()
{
	HRESULT hr;

	if (FAILED(hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, __uuidof(IGraphBuilder), (LPVOID*)&pGraphBuilder))) {
		MessageBox(NULL, L"GraphBuilder CoCI failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (AddToRot(pGraphBuilder, &dwRegister) == E_FAIL) {
		puts("AddToRot fail");
	}

	if (FAILED(hr = pGraphBuilder->QueryInterface(__uuidof(IMediaControl), (LPVOID*)&pMediaControl))) {
		MessageBox(NULL, L"MediaControl QI failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (FAILED(hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC, __uuidof(ICaptureGraphBuilder2), (LPVOID*)&pCaptureGraphBuilder2))) {
		MessageBox(NULL, L"CaptureGraphBuilder CoCI failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (FAILED(hr = pCaptureGraphBuilder2->SetFiltergraph(pGraphBuilder))) {
		MessageBox(NULL, L"setFilterGraph CaptureGraphBuilder failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	//	if (FAILED(hr = CoCreateInstance(CLSID_EnhancedVideoRenderer, NULL, CLSCTX_INPROC, __uuidof(IBaseFilter), (LPVOID*)&pVideo))) {
	if (FAILED(hr = CoCreateInstance(CLSID_madVR, NULL, CLSCTX_INPROC, __uuidof(IBaseFilter), (LPVOID*)&pVideo))) {
		MessageBox(NULL, L"Video CoCI failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (FAILED(hr = pGraphBuilder->AddFilter(pVideo, L"video"))) {
		MessageBox(NULL, L"GraphBuilder AddFilter Video failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}


	pSplitterVideo = new CLAVSplitter(NULL, &hr);
	pSplitterVideo->read_buffer_size = LAV_VIDEO_READ_SIZE;
	if (FAILED(hr = pGraphBuilder->AddFilter(pSplitterVideo, L"Video Splitter"))) {
		MessageBox(NULL, L"GraphBuilder AddFilter pSplitterVideo failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (FAILED(hr = pSplitterVideo->QueryInterface(__uuidof(IBaseFilter), (LPVOID*)&pSplitterVideoSrc))) {
		MessageBox(NULL, L"pSplitterVideoSrc QI from pSplitterVideo failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}


	pSplitterAudio = new CLAVSplitter(NULL, &hr);
	pSplitterAudio->read_buffer_size = LAV_AUDIO_READ_SIZE;
	if (FAILED(hr = pGraphBuilder->AddFilter(pSplitterAudio, L"Audio Splitter"))) {
		MessageBox(NULL, L"GraphBuilder AddFilter pSplitterAudio failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (FAILED(hr = pSplitterAudio->QueryInterface(__uuidof(IBaseFilter), (LPVOID*)&pSplitterAudioSrc))) {
		MessageBox(NULL, L"pSplitterAudioSrc QI from pSplitterAudio failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}


	if (FAILED(hr = CoCreateInstance(CLSID_DSoundRender, NULL, CLSCTX_INPROC, __uuidof(IBaseFilter), (LPVOID*)&pAudio))) {
		MessageBox(NULL, L"Audio CoCI failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}
	if (FAILED(hr = pGraphBuilder->AddFilter(pAudio, L"audio"))) {
		MessageBox(NULL, L"GraphBuilder AddFilter Audio failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	myAsyncVideoSrc = new CMyAsyncSrc(NULL, &hr, 0);
	if (FAILED(hr = pGraphBuilder->AddFilter(myAsyncVideoSrc, L"myAsyncVideoSrc"))) {
		MessageBox(NULL, L"GraphBuilder AddFilter myAsyncVideoSrc failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (FAILED(hr = myAsyncVideoSrc->QueryInterface(__uuidof(IBaseFilter), (LPVOID*)&pVideoSrc))) {
		MessageBox(NULL, L"VideoSrc QI from myAsyncVideoSrc failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	myAsyncAudioSrc = new CMyAsyncSrc(NULL, &hr, 1);
	if (FAILED(hr = pGraphBuilder->AddFilter(myAsyncAudioSrc, L"myAsyncAudioSrc"))) {
		MessageBox(NULL, L"GraphBuilder AddFilter myAsyncAudioSrc failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	if (FAILED(hr = myAsyncAudioSrc->QueryInterface(__uuidof(IBaseFilter), (LPVOID*)&pAudioSrc))) {
		MessageBox(NULL, L"AudioSrc QI from myAsyncAudioSrc failed", L"Error", MB_OK | MB_TOPMOST);
		return E_FAIL;
	}

	return S_OK;
}


HRESULT ReleaseGraphs()
{
	if (dwRegister != NULL) {
		RemoveFromRot(dwRegister);
	}

	if (pVideoWindow != NULL) {
		pVideoWindow->put_Visible(OAFALSE);
		pVideoWindow->put_MessageDrain(NULL);
		pVideoWindow->put_Owner(NULL);
	}

	if (pGraphBuilder != NULL) {
		IEnumFilters* pEnum = NULL;
		if (SUCCEEDED(pGraphBuilder->EnumFilters(&pEnum))) {
			IBaseFilter* pFilter = NULL;
			while (S_OK == pEnum->Next(1, &pFilter, NULL)) {
				IEnumPins* pPinEnum = NULL;
				if (SUCCEEDED(pFilter->EnumPins(&pPinEnum))) {
					IPin* pPin = NULL;
					while (S_OK == pPinEnum->Next(1, &pPin, NULL)) {
						pPin->BeginFlush();
						pPin->EndFlush();
						pPin->EndOfStream();
						SAFE_RELEASE(pPin);
					}
					SAFE_RELEASE(pPinEnum);
				}
				pGraphBuilder->RemoveFilter(pFilter);
				SAFE_RELEASE(pFilter);
				pEnum->Reset();
			}
			SAFE_RELEASE(pEnum);
		}

		SAFE_RELEASE(pMadVRCommand);
		SAFE_RELEASE(pMadVROsdServices);
		SAFE_DELETE(myAsyncAudioSrc);
		SAFE_DELETE(myAsyncVideoSrc);
		SAFE_DELETE(pSplitterAudio);
		SAFE_DELETE(pSplitterVideo);

		//SAFE_RELEASE(pVideoWindow);
		//SAFE_RELEASE(pSplitterAudioSrc);
		//SAFE_RELEASE(pSplitterVideoSrc);
		//SAFE_RELEASE(pAudioSrc);
		//SAFE_RELEASE(pVideoSrc);
		pVideoSrc = NULL;
		pAudioSrc = NULL;
		pSplitterVideoSrc = NULL;
		pSplitterAudioSrc = NULL;
		pVideoWindow = NULL;


		SAFE_RELEASE(pBasicAudio);
		SAFE_RELEASE(pAudio);
		SAFE_RELEASE(pVideo);
		SAFE_RELEASE(pCaptureGraphBuilder2);
		SAFE_RELEASE(pMediaControl);
		SAFE_RELEASE(pGraphBuilder);



		/*
		IEnumFilters* pEnum = NULL;
		if (SUCCEEDED(pGraphBuilder->EnumFilters(&pEnum)))
		{
			IBaseFilter* pFilter = NULL;
			while (S_OK == pEnum->Next(1, &pFilter, NULL))
			{
				IEnumPins* pPinEnum = NULL;
				if (SUCCEEDED(pFilter->EnumPins(&pPinEnum))) {
					IPin* pPin = NULL;
					while (S_OK == pPinEnum->Next(1, &pPin, NULL)) {
						SAFE_RELEASE(pPin);
					}
					SAFE_RELEASE(pPinEnum);
				}
				pGraphBuilder->RemoveFilter(pFilter);
				SAFE_RELEASE(pFilter);
				pEnum->Reset();
			}
			SAFE_RELEASE(pEnum);
		}

		SAFE_RELEASE(pGraphBuilder);
		SAFE_RELEASE(pMediaControl);
		SAFE_RELEASE(pCaptureGraphBuilder2);
		SAFE_RELEASE(pVideo);
		SAFE_RELEASE(pAudio);
		SAFE_RELEASE(pBasicAudio);
		SAFE_DELETE(pSplitterVideo);
		SAFE_DELETE(pSplitterAudio);
		SAFE_DELETE(myAsyncVideoSrc);
		SAFE_DELETE(myAsyncAudioSrc);

		pVideoSrc = NULL;
		pAudioSrc = NULL;
		pSplitterVideoSrc = NULL;
		pSplitterAudioSrc = NULL;
		pVideoWindow = NULL;
		*/
	}

	SAFE_DELETE(video_frame_buffer);
	SAFE_DELETE(audio_frame_buffer);
	SAFE_DELETE(tlv);

	return S_OK;
}

//time_t time;
//tm t;
//		_time64(&time);
//		localtime_s(&t, &time);
////		printf("%02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
//		if ((t.tm_hour == 16) && (t.tm_min == 14) && (t.tm_sec >= 30)) {
//			if (!record) {
//				record = 1;
//				elapsed = 0;
//			}
//		}
//		if (record && (elapsed >= 930)) {
//			break;
//		}

	//	Sleep(1000);
