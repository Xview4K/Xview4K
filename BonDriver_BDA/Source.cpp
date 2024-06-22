#include <stdio.h>

#include "BonTuner.h"



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

unsigned long long count = 0;
int record = 0;
int write_error = 0;
FILE* fp_dst = NULL;

int main(int argc, char* argv[])
{
	HMODULE hModule = NULL;

	extern BOOL APIENTRY DllMain(HMODULE hModule,
		DWORD  ul_reason_for_call,
		LPVOID /* lpReserved */
	);
	DllMain(hModule, DLL_PROCESS_ATTACH, 0);

	CBonTuner* tuner = new CBonTuner;

	if ((fp_dst = fopen("g:/4Kmmt.mmts", "wb")) == NULL) {
		perror("outputfile:");
		exit(EXIT_FAILURE);
	}

	if (tuner->OpenTuner() == FALSE) {
		puts("Tuner open failed");
		return -1;
	}
	else {
		puts("Tuner opend successfully");
//		wprintf(L"%s\n", tuner->GetReadyCount());
	}


	if (tuner->SetChannel(0) == FALSE) {
		puts("Set channnel failed");
	}
	else {
		puts("Set channel successfully");
	}

	unsigned long long pre_count = 0;
	long elapsed = 0;

	float sig;
	time_t time;
	tm t;
	while (1) {
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

		sig = tuner->GetSignalLevel();
		printf("\rSIG:%.1f DRATE:%.1fMBps REC:%d WERR:%02d ELP:%02d:%02d:%02d ", sig, (float)((count - pre_count) * 8) / 1000000.0f, record, write_error, elapsed / 3600, (elapsed / 60) % 60, elapsed % 60);
		pre_count = count;
		Sleep(1000);
		if (record) {
			elapsed++;
		}
		if (GetAsyncKeyState('X')) {
			break;
		}
		if (GetAsyncKeyState('R')) {
			if (!record) {
				record = 1;
				elapsed = 0;
			}
		}
	}

//	MessageBoxW(NULL, L"Block Execution", L"Block", MB_OK);

	tuner->CloseTuner();

	if (fp_dst != NULL) {
		fclose(fp_dst);
	}

	return 0;
}
