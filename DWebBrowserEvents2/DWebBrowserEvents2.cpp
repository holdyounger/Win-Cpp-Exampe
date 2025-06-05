#include <windows.h>
#include <shlobj.h>
#include <exdispid.h>
#include <atlbase.h>
#include <atlcom.h>
#include <iostream>
#include <comutil.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comsuppw.lib")  // 需要链接 comsuppw.lib 以支持 _bstr_t

// 事件监听器类
class ExplorerEventSink : public IDispatch {
private:
    LONG m_refCount;

public:
    ExplorerEventSink() : m_refCount(1) {}

    // IUnknown 实现
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDispatch) {
            *ppv = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    // IDispatch 实现
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) override {
        return E_NOTIMPL;
    }

    // 事件回调函数
    HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams,
        VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) override {
        if (dispIdMember == DISPID_NAVIGATECOMPLETE2) {  // 监听 Explorer 路径变化
            if (pDispParams->cArgs >= 2 && pDispParams->rgvarg[1].vt == VT_DISPATCH) {
                CComPtr<IWebBrowser2> pWebBrowser;
                pDispParams->rgvarg[1].pdispVal->QueryInterface(IID_IWebBrowser2, (void**)&pWebBrowser);
                if (pWebBrowser) {
                    BSTR vURL;
                    pWebBrowser->get_LocationURL(&vURL);
                    char* lpszText2 = _com_util::ConvertBSTRToString(vURL);
                    std::wcout << L"[Explorer Path Changed] " << lpszText2 << std::endl;
                    delete[] lpszText2;
                    SysFreeString(vURL);
                }
            }
        }
        return S_OK;
    }
};

// 全局钩子函数
void MonitorExplorerTabs()
{
    CoInitialize(NULL);

    CComPtr<IShellWindows> spShellWindows;
    HRESULT hr = spShellWindows.CoCreateInstance(CLSID_ShellWindows);
    if (FAILED(hr)) {
        std::cout << "Failed to get IShellWindows" << std::endl;
        return;
    }

    long count = 0;
    spShellWindows->get_Count(&count);
    std::cout << "Monitoring " << count << " Explorer Tabs..." << std::endl;

    for (long i = 0; i < count; i++) {
        CComVariant vIndex(i);
        CComPtr<IDispatch> spDisp;
        spShellWindows->Item(vIndex, &spDisp);
        if (!spDisp)
            continue;

        CComPtr<IWebBrowser2> spWebBrowser;
        spDisp->QueryInterface(IID_IWebBrowser2, (void**)&spWebBrowser);
        if (!spWebBrowser)
            continue;

        // 绑定事件监听器
        ExplorerEventSink* pSink = new ExplorerEventSink();
        CComPtr<IConnectionPointContainer> spCPC;
        CComPtr<IConnectionPoint> spCP;
        DWORD dwCookie = 0;

        if (SUCCEEDED(spWebBrowser->QueryInterface(IID_IConnectionPointContainer, (void**)&spCPC)) &&
            SUCCEEDED(spCPC->FindConnectionPoint(DIID_DWebBrowserEvents2, &spCP))) {
            spCP->Advise(pSink, &dwCookie);
        }
    }

    // 进入消息循环，监听 Explorer 变化
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
}

int main()
{
    MonitorExplorerTabs();
    return 0;
}
