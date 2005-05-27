/*
 * Copyright (C) 2004 Sun Microsystems, Inc. All rights reserved. Use is
 * subject to license terms.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */ 
#include <Ole2.h>
#include "stdafx.h"
#include "MsgServer.h"
#include "BrowserWindow.h"
#include "resource.h"
#include "Message.h"
//#include "prthread.h"
#include "VariantWrapper.h"
#include "Util.h"
#define WM_SOCKET_MSG   WM_USER + 1

CComModule _Module;

HWND gMainWnd;

//array of browserwindow
WBArray ABrowserWnd;

void SocketMsgHandler(const char* pMsg)
{
    char* msg = new char[strlen(pMsg) + 1];
    strcpy(msg, pMsg);
    ::PostMessage(gMainWnd, WM_SOCKET_MSG, 0, (long)msg);
}

HRESULT LoadWebBrowserFromStream(BrowserWindow* pBrowserWnd, IStream* pStream)
{
    HRESULT hr;
    CComPtr<IDispatch> pHTMLDoc;
    CComPtr<IPersistStreamInit> pPersistStreamInit;

    hr = pBrowserWnd->m_pWB->get_Document(&pHTMLDoc);
    if (SUCCEEDED(hr))
    {
        hr = pHTMLDoc->QueryInterface(IID_IPersistStreamInit, (void**)&pPersistStreamInit);
        if (SUCCEEDED(hr))
        {
            hr = pPersistStreamInit->Load(pStream);
        }
    }
    return hr;
}

void setContent(BrowserWindow* pBrowserWnd, char* pContent)
{
    if (pContent == NULL)
    {
        return;
    }
    CComPtr<IStream> pStream;
    size_t contentLen = strlen(pContent);
    HGLOBAL hHTMLText;
    hHTMLText = GlobalAlloc(GPTR, contentLen);
    if (hHTMLText)
    {
        strcpy((char *) hHTMLText, pContent);
        HRESULT hr = CreateStreamOnHGlobal(hHTMLText, TRUE, &pStream);
        if (SUCCEEDED(hr))
        {
            LoadWebBrowserFromStream(pBrowserWnd, pStream);
        }
    }
    GlobalFree(hHTMLText);
}

LPSTR executeScript(BrowserWindow* pBrowserWnd, char* scriptCode)
{
    //Get the IHTMLDocument Interface
    CComPtr<IDispatch> pIDDispatch;
    HRESULT hRes;
    hRes = pBrowserWnd->m_pWB->get_Document((struct IDispatch **)&pIDDispatch);
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IWebBrowser::get_Document()...");
    }
    else 
    {
        return NULL;
    }
    hRes = pIDDispatch->QueryInterface(IID_IHTMLDocument2, (void **)&(pBrowserWnd->m_pHD2));
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IDispatch::QueryInterface(IID_IHTMLDocument2, void**)...");
    }
    else 
    {
        return NULL;
    }
    hRes = pIDDispatch->QueryInterface(IID_IHTMLDocument3, (void **)&(pBrowserWnd->m_pHD3));
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IDispatch::QueryInterface(IID_IHTMLDocument3, void**)...");
    }
    else 
    {
        return NULL;
    }
    //Get the IHTMLWindow2
    hRes = pBrowserWnd->m_pHD2->get_parentWindow((struct IHTMLWindow2 **)&(pBrowserWnd->m_pHW));
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IHTMLDocument2::get_ParentWindow()...");
    }
    else 
    {
        return NULL;
    }
    //Execute the script with IHTMLWindow2::execScript()
    CComVariant varResult;
    CComBSTR vtCode;
    CComBSTR vtLanguage;
    varResult.Clear();
    
    // Tune the given jscript to assign the returned value to a predefine 
    // DOM property of the currently loaded webapge:
    //     JDIC_BROWSER_INTERMEDIATE_PROP
    vtCode.Append(TuneJavaScript(scriptCode));
    vtLanguage.Append("javascript");
    hRes = pBrowserWnd->m_pHW->execScript((BSTR)vtCode, (BSTR)vtLanguage, &varResult);
    if (FAILED(hRes))
    {
        void* pMsgBuf;
        ::FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            hRes, 
            MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
            (LPTSTR) &pMsgBuf,
            0,
            NULL);
        WBTRACE((LPSTR)(pMsgBuf));
        LocalFree(pMsgBuf);
    }
    //Try to get the return value of the script
    CComBSTR elementTag;
    //1. Get the head elements collection
    elementTag.Append("head");
    CComPtr<IHTMLElementCollection> pElementCollection;
    hRes = pBrowserWnd->m_pHD3->getElementsByTagName(elementTag, (IHTMLElementCollection **)&pElementCollection);
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IHTMLDocument3::getElementsByTagName()...");
    }
    else 
    {
        return NULL;
    }
    //2. Get the # 0 element
    CComPtr<IDispatch> pElementDispatch;
    CComPtr<IHTMLElement> pHTMLElement;
    VARIANT itemIndex;
    itemIndex.vt = VT_I4;
    itemIndex.lVal = 0;
    hRes = pElementCollection->item(itemIndex, itemIndex, &pElementDispatch);
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IHTMLElementCollection::item()...");
    }
    else 
    {
        return NULL;
    }
    hRes = pElementDispatch->QueryInterface(IID_IHTMLElement, (void **)&pHTMLElement);
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IDispatch::QueryInterface(IID_IHTMLElement, void**)...");
    }
    else 
    {
        return NULL;
    }
    //3. Get the pre-defined attribute value
    CComBSTR attribName;
    CComVariant varValue;
    attribName.Append(JDIC_BROWSER_INTERMEDIATE_PROP);
    hRes = pHTMLElement->getAttribute(attribName, 0, &varValue);
    // Remove the intermedial attribute created by JDIC Browser.
    VARIANT_BOOL success;
    pHTMLElement->removeAttribute(attribName, 0, &success);
    if (SUCCEEDED(hRes))
    {
        WBTRACE("IHTMLElement::getAttribute()");
    }
    else 
    {
        return NULL;
    }
    VariantWrapper varWrapper(&varValue);
    return varWrapper.ToString();
}

void CommandProc(char* pInputChar)
{
    BrowserWindow * pBrowserWnd;
    HRESULT hRes;
    int instanceNum;
    int eventID;
    char eventMessage[1024];

    int i = sscanf(pInputChar, "%d,%d,%s", &instanceNum, &eventID, &eventMessage);
    if (i < 2) 
    {
        delete pInputChar;
        return;
    }

    // In case that the last message string argument contains spaces, sscanf 
    // returns before the first space. Below line returns the complete message
    // string.
    char* mMsgString = (char *)strchr(pInputChar, ',');
    mMsgString++;
    mMsgString = (char*)strchr(mMsgString, ',');
    mMsgString++;

    switch (eventID)
    {
    case JEVENT_INIT:
        break;

    case JEVENT_CREATEWINDOW:
        {
        // only create new browser window when the instance does not exist
        if (instanceNum < ABrowserWnd.GetSize() && 
            (BrowserWindow *) ABrowserWnd[instanceNum] != NULL)
            break;

        RECT rect;
        if (i != 3) 
            break;
        HWND hWnd = (HWND) atoi(mMsgString);
        pBrowserWnd = new BrowserWindow();
        if (!pBrowserWnd) 
            break;

        SetRect(&rect, 0, 0, 0, 0);
        HWND hWndClient = pBrowserWnd->Create(hWnd,rect,
                _T("about:blank"),
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                WS_EX_CLIENTEDGE);

        hRes = pBrowserWnd->QueryControl(&(pBrowserWnd->m_pWB));
        if (pBrowserWnd->m_pWB == NULL) {
            WBTRACE("Failed to query pBrowserWnd->m_pWB!");
            break;
        }

        hRes = pBrowserWnd->DispEventAdvise(pBrowserWnd->m_pWB);

        pBrowserWnd->SetReady(instanceNum);
        SendSocketMessage(instanceNum, CEVENT_INIT_WINDOW_SUCC);
        
        //save the pointer of BrowserWnd to array
        ABrowserWnd.SetAtGrow(instanceNum, pBrowserWnd);
        //show window
        ShowWindow(hWndClient, SW_SHOW);
        UpdateWindow(hWndClient);
        SetFocus(hWndClient);
        }
        break;

    case JEVENT_DESTROYWINDOW:
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        if(pBrowserWnd != NULL){
            hRes = pBrowserWnd->DispEventUnadvise(pBrowserWnd->m_pWB);
            pBrowserWnd->DestroyWindow();
            delete pBrowserWnd;
            ABrowserWnd.SetAt(instanceNum, NULL);
        }
        SendSocketMessage(instanceNum, CEVENT_DISTORYWINDOW_SUCC);
        break;

    case JEVENT_SHUTDOWN:
        ::PostMessage(gMainWnd, WM_QUIT, 0, 0);
        break;

    case JEVENT_SET_BOUNDS:
        {
        if (i != 3) break;
        int x, y, w, h;
        i = sscanf(mMsgString, "%d,%d,%d,%d", &x, &y, &w, &h);
        if (i == 4) {
            pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
            ATLASSERT(pBrowserWnd != NULL);
            pBrowserWnd->SetWindowPos(NULL, x, y, w, h, SWP_NOZORDER);
        }
        }
        break;

    case JEVENT_NAVIGATE:
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        ATLASSERT(pBrowserWnd != NULL);
        pBrowserWnd->m_pWB->Navigate(CComBSTR(mMsgString), NULL, NULL, NULL, NULL);
        break;

    case JEVENT_GOBACK:
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        ATLASSERT(pBrowserWnd != NULL);
        pBrowserWnd->m_pWB->GoBack();
        break;

    case JEVENT_GOFORWARD:
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        ATLASSERT(pBrowserWnd != NULL);
        pBrowserWnd->m_pWB->GoForward();
        break;

    case JEVENT_REFRESH:
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        ATLASSERT(pBrowserWnd != NULL);
        pBrowserWnd->m_pWB->Refresh();
        break;

    case JEVENT_STOP:
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        ATLASSERT(pBrowserWnd != NULL);
        pBrowserWnd->m_pWB->Stop();
        break;

    case JEVENT_GETCONTENT:
        {
            pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];

            // JavaScript to return the content of the currently loaded URL 
            // in *IE*, which is different from the JavaScript for Mozilla.
            char* IE_GETCONTENT_SCRIPT 
                = "(document.documentElement||document.body).outerHTML;";
            LPSTR exeResult = executeScript(pBrowserWnd, IE_GETCONTENT_SCRIPT);
            SendSocketMessage(instanceNum, CEVENT_GETCONTENT, (LPSTR)(exeResult));
            delete [] exeResult;
            break;
        }

    case JEVENT_EXECUTESCRIPT:
        {
            pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
            LPSTR exeResult = executeScript(pBrowserWnd, mMsgString);
            SendSocketMessage(instanceNum, CEVENT_EXECUTESCRIPT, (LPSTR)(exeResult));
            delete [] exeResult;
            break;
        }

    case JEVENT_SETCONTENT:
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        setContent(pBrowserWnd, mMsgString);
        break;

    case JEVENT_GETURL:
        USES_CONVERSION;
        BSTR bsUrl;
        pBrowserWnd = (BrowserWindow *) ABrowserWnd[instanceNum];
        ATLASSERT(pBrowserWnd != NULL);
        pBrowserWnd->m_pWB->get_LocationURL(&bsUrl);
        SendSocketMessage(instanceNum, CEVENT_RETURN_URL, W2A(bsUrl));
        SysFreeString(bsUrl);
        break;
    }
    delete pInputChar;
    return;
}


//this function is passed as parameter to struct WNDCLASSEX 
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SOCKET_MSG:
        CommandProc ((char*)lParam);
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


//this function is called in CreateHiddenWnd()
void RegisterWindowClass()
{
    WNDCLASSEX  wcx;

    ZeroMemory(&wcx, sizeof(WNDCLASSEX));
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.lpfnWndProc = MainWindowProc;
    wcx.hInstance = GetModuleHandle(NULL);
    wcx.hIcon = NULL; // LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDR_MAIN));
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcx.lpszClassName = "SampleWindowClass";
    wcx.lpszMenuName = NULL; //MAKEINTRESOURCE(IDR_MAIN);
    RegisterClassEx(&wcx);
}

void CreateHiddenWnd() 
{
    RegisterWindowClass();
    gMainWnd = CreateWindowEx(WS_EX_APPWINDOW,
        "SampleWindowClass", _T("Browser App"), WS_DISABLED,
        -10010, -10010, -10000, -10000, NULL, NULL, 0, NULL);
}

/////////////////////////////////////////////////////////////////////////////
//
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/, LPTSTR lpCmdLine, int /*nShowCmd*/)
{
    if (strstr(lpCmdLine, "-port=")) {
        int port = atoi(&lpCmdLine[6]);
        gMessenger.SetPort(port);
        gMessenger.CreateServerSocket();
    }
    if (gMessenger.IsFailed()) {
        fprintf(stderr, "Failed to create server socket\n");
        return -1;
    }

    lpCmdLine = GetCommandLine(); //this line necessary for _ATL_MIN_CRT
    //Initializes the COM library for use
#if _WIN32_WINNT >= 0x0400 & defined(_ATL_FREE_THREADED)
    HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
    HRESULT hRes = CoInitialize(NULL);
#endif

    ATLASSERT(SUCCEEDED(hRes));
    _Module.Init(NULL, hInstance, &LIBID_SHDocVw);

    //create hidden window for purpose of handling messages
    CreateHiddenWnd();

    //init new thread for socket communication listening.
    HANDLE hThread = CreateThread(NULL,
                                  0, 
                                  PortListening, 
                                  SocketMsgHandler,
                                  0, 
                                  NULL);

    if (hThread == NULL) {
        SendSocketMessage(-1, CEVENT_INIT_FAILED);
        goto exit;
    }

    //init atl
    AtlAxWinInit();

    //process message
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    { 
        if (bRet == -1)
        {
            // handle the error and possibly exit
        }
        else
        {
            int size = ABrowserWnd.GetSize();
            for (int i = 0; i < size; i++) {
                BrowserWindow *pBrowserWnd = (BrowserWindow *)ABrowserWnd[i];
                if (pBrowserWnd && pBrowserWnd->PreTranslateMessage(&msg)) {
                    break;
                }
            }
            if (i == size) {
                TranslateMessage(&msg); 
                DispatchMessage(&msg); 
            }
        }
    }
    
exit:
    _Module.Term();
    CoUninitialize();
    return 0;
}
