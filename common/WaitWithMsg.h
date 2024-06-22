#pragma once

static DWORD WaitForMultipleObjectsWithMessageLoop(DWORD nCount, LPHANDLE pHandles, BOOL fWaitAll, DWORD dwMilliseconds)
{
	DWORD dwRet;
	MSG msg;

	while (1)
	{
		dwRet = ::MsgWaitForMultipleObjects(nCount, pHandles, fWaitAll, dwMilliseconds, QS_ALLINPUT);

		if (dwRet == WAIT_OBJECT_0 + nCount) {
			// ���b�Z�[�W�̃f�B�X�p�b�`
			while (::PeekMessage(&msg, NULL, WM_NULL, WM_NULL, PM_REMOVE)) {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
		}
		else {
			// WaitForMultipleObjects()�̉���
			return dwRet;
		}
	}
}

static DWORD WaitForSingleObjectWithMessageLoop(HANDLE hHandle, DWORD dwMilliseconds)
{
	return WaitForMultipleObjectsWithMessageLoop(1, &hHandle, FALSE, dwMilliseconds);
}

static void SleepWithMessageLoop(DWORD dwMilliseconds)
{
	WaitForMultipleObjectsWithMessageLoop(0, NULL, FALSE, dwMilliseconds);
	return;
}