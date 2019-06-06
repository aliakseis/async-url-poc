// async-url-poc.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#include <iostream>
#include <string>

class CComUsageScope
{
    bool m_bInitialized;
public:
    explicit CComUsageScope(DWORD dwCoInit = COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY)
    {
        m_bInitialized = SUCCEEDED(CoInitializeEx(NULL, dwCoInit));
    }
    ~CComUsageScope()
    {
        if (m_bInitialized)
            CoUninitialize();
    }
};

VOID CALLBACK SendAsyncProc(
    HWND hwnd,
    UINT uMsg,
    ULONG_PTR dwData,
    LRESULT lResult)
{
    std::cout << "In " << __FUNCTION__ << " thread " << GetCurrentThreadId() << '\n';

    CComUsageScope scope;

    CComPtr<IAccessible> pacc;
    if (FAILED(ObjectFromLresult(lResult, __uuidof(IAccessible), 0, (void**)&pacc)))
        return;

    POINT ptScreen{ LOWORD(dwData), HIWORD(dwData) };
    {
        CComVariant vtChild;
        CComQIPtr<IAccessible> paccChild;
        for (; SUCCEEDED(pacc->accHitTest(ptScreen.x, ptScreen.y, &vtChild))
            && VT_DISPATCH == vtChild.vt && (paccChild = vtChild.pdispVal) != NULL;
            vtChild.Clear())
        {
            pacc.Attach(paccChild.Detach());
        }
    }

    VARIANT v;
    v.vt = VT_I4;
    v.lVal = CHILDID_SELF;

    while (pacc)
    {
        CComVariant vRole;

        if (SUCCEEDED(pacc->get_accRole(v, &vRole)) && vRole.vt == VT_I4 && vRole.lVal == ROLE_SYSTEM_LINK)
        {
            CComBSTR url;
            if (FAILED(pacc->get_accValue(v, &url)))
                return;
            LPCWSTR buf = url;
            std::string result(buf, buf + url.Length());
            std::cout << result << '\n';
            return;
        }

        CComPtr<IDispatch> spDisp;
        if (FAILED(pacc->get_accParent(&spDisp)))
            return;
        CComQIPtr<IAccessible> spParent(spDisp);
        pacc.Attach(spParent.Detach());
    }
}

int main()
{
    std::cout << "In " << __FUNCTION__ << " thread " << GetCurrentThreadId() << '\n';

    POINT pt;
    if (!GetCursorPos(&pt))
        return 1;

    HWND hWnd = WindowFromPoint(pt);
    if (NULL == hWnd)
        return 1;

    TCHAR szBuffer[64];

    const int classNameLength
        = ::GetClassName(hWnd, szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]));

    szBuffer[sizeof(szBuffer) / sizeof(szBuffer[0]) - 1] = _T('\0');

    if (_tcscmp(szBuffer, _T("MozillaWindowClass")) != 0 && _tcscmp(szBuffer, _T("Chrome_RenderWidgetHostHWND")) != 0)
        return 1;

    MSG msg;
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    if (!SendMessageCallback(hWnd, WM_GETOBJECT, 0L, OBJID_CLIENT, SendAsyncProc, MAKELONG(pt.x, pt.y)))
    {
        std::cerr << "SendMessageCallback() failed.\n";
        return 1;
    }

    //Sleep(10000);
    while (WAIT_TIMEOUT != MsgWaitForMultipleObjects(0, NULL, FALSE, 10000, QS_ALLINPUT))
    {
        MSG msg;
        // we have a message - peek and dispatch it
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}
