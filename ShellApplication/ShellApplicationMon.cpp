#include <windows.h>
#include <iostream>
#include <comutil.h>
#include <exdisp.h>
#include <exdispid.h>
#include <atlbase.h>  // 用于CComPtr

#pragma comment(lib, "comsuppw.lib") 

#include <thread>
#include <mutex>
#include <queue>
using namespace std;

// 事件接收器类 - 使用多线程模型 
class ExplorerEventSink : public IDispatch
{
public:
    ExplorerEventSink(IWebBrowser2* pWebBrowser) :
        m_pWebBrowser(pWebBrowser),
        m_refCount(1),
        m_dwCookie(0),
        m_hEventQueueReady(CreateEvent(NULL, FALSE, FALSE, NULL))
    {
        // 创建工作线程处理事件
        m_hWorkerThread = CreateThread(
            NULL,
            0,
            WorkerThreadProc,
            this,
            0,
            NULL);

        // 注册事件接收器 
        IConnectionPointContainer* pCPC = nullptr;
        if (SUCCEEDED(m_pWebBrowser->QueryInterface(IID_IConnectionPointContainer, (void**)&pCPC)))
        {
            IConnectionPoint* pCP = nullptr;
            if (SUCCEEDED(pCPC->FindConnectionPoint(DIID_DWebBrowserEvents2, &pCP)))
            {
                pCP->Advise(this, &m_dwCookie);
                pCP->Release();
            }
            pCPC->Release();
        }
    }

    ~ExplorerEventSink()
    {
        // 通知工作线程退出
        m_bExit = true;
        SetEvent(m_hEventQueueReady);

        // 等待工作线程结束 
        WaitForSingleObject(m_hWorkerThread, INFINITE);

        // 关闭句柄
        CloseHandle(m_hWorkerThread);
        CloseHandle(m_hEventQueueReady);

        // 注销事件接收器
        if (m_dwCookie != 0)
        {
            IConnectionPointContainer* pCPC = nullptr;
            if (SUCCEEDED(m_pWebBrowser->QueryInterface(IID_IConnectionPointContainer, (void**)&pCPC)))
            {
                IConnectionPoint* pCP = nullptr;
                if (SUCCEEDED(pCPC->FindConnectionPoint(DIID_DWebBrowserEvents2, &pCP)))
                {
                    pCP->Unadvise(m_dwCookie);
                    pCP->Release();
                }
                pCPC->Release();
            }
        }
    }

    // IUnknown 方法 
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (riid == IID_IDispatch || riid == IID_IUnknown)
        {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG refCount = InterlockedDecrement(&m_refCount);
        if (refCount == 0)
        {
            delete this;
        }
        return refCount;
    }

    // IDispatch 方法 
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override { return E_NOTIMPL; }
    STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) override { return E_NOTIMPL; }

    STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
        DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) override
    {
        if (dispIdMember == DISPID_NAVIGATECOMPLETE2 || dispIdMember == DISPID_DOCUMENTCOMPLETE)
        {
            // 将事件放入队列，由工作线程处理 
            EventData data;
            data.dispId = dispIdMember;

            CComBSTR vURL;
            if (SUCCEEDED(m_pWebBrowser->get_LocationURL(&vURL)))
            {
                data.url = _com_util::ConvertBSTRToString(vURL);
                // std::cout << "Explorer 地址栏已更改: " << data.url << std::endl;
                SysFreeString(vURL);
            }

            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_eventQueue.push(std::move(data));
            }

            SetEvent(m_hEventQueueReady);
        }
        return S_OK;
    }

private:
    struct EventData
    {
        DISPID dispId;
        std::string url;
        ~EventData() 
        {
            // if (!url.empty())  
                // delete[] url.data(); 
        }
    };

    static DWORD WINAPI WorkerThreadProc(LPVOID lpParam)
    {
        ExplorerEventSink* pThis = static_cast<ExplorerEventSink*>(lpParam);
        pThis->ProcessEvents();
        return 0;
    }

    void ProcessEvents()
    {
        while (!m_bExit)
        {
            WaitForSingleObject(m_hEventQueueReady, INFINITE);

            if (m_bExit) break;

            std::queue<EventData> localQueue;
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                std::swap(localQueue, m_eventQueue);
            }

            while (!localQueue.empty())
            {
                auto& data = localQueue.front();
                std::cout << "Explorer 地址栏已更改: " << data.url << std::endl;
                localQueue.pop();
            }
        }
    }

    CComPtr<IWebBrowser2> m_pWebBrowser;
    ULONG m_refCount;
    DWORD m_dwCookie;
    HANDLE m_hWorkerThread;
    HANDLE m_hEventQueueReady;
    std::queue<EventData> m_eventQueue;
    std::mutex m_queueMutex;
    bool m_bExit = false;
};

int main()
{
    // 初始化 COM 为多线程模型 
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        std::cerr << "COM 初始化失败!" << std::endl;
        return 1;
    }

    // 创建 Shell.Application COM 对象
    CComPtr<IShellWindows> pShellWindows;
    hr = pShellWindows.CoCreateInstance(CLSID_ShellWindows);

    if (FAILED(hr))
    {
        std::cerr << "无法创建 Shell.Application 对象!" << std::endl;
        CoUninitialize();
        return 1;
    }

    // 遍历所有打开的 Windows 资源管理器窗口并注册事件接收器 
    long count = 0;
    pShellWindows->get_Count(&count);

    std::vector<ExplorerEventSink*> sinks;

    for (long i = 0; i < count; i++)
    {
        VARIANT vIndex;
        vIndex.vt = VT_I4;
        vIndex.lVal = i;

        CComPtr<IDispatch> pDisp;
        pShellWindows->Item(vIndex, &pDisp);
        if (pDisp)
        {
            CComPtr<IWebBrowser2> pWebBrowser;
            hr = pDisp->QueryInterface(IID_IWebBrowser2, (void**)&pWebBrowser);
            if (SUCCEEDED(hr))
            {
                // 为每个Explorer窗口创建事件接收器
                sinks.push_back(new  ExplorerEventSink(pWebBrowser));

                // 打印初始URL 
                CComBSTR vURL;
                hr = pWebBrowser->get_LocationURL(&vURL);
                if (SUCCEEDED(hr))
                {
                    char* lpszText2 = _com_util::ConvertBSTRToString(vURL);
                    std::cout << i << " 初始 Explorer 路径: " << lpszText2 << std::endl;
                    delete[] lpszText2;
                }
            }
        }
    }

    // 消息循环，保持程序运行以接收事件
    std::cout << "监控Explorer地址栏更改中...按任意键退出" << std::endl;
    std::cin.get();

    // 清理事件接收器 
    for (auto sink : sinks)
    {
        delete sink;
    }

    // 关闭 COM 
    CoUninitialize();
    return 0;
}