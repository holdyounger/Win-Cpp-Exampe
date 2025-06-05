#include <windows.h>
#include <iostream>
#include <comutil.h>
#include <exdisp.h>

#pragma comment(lib, "comsuppw.lib")  // 需要链接 comsuppw.lib 以支持 _bstr_t

int main1() {
    // 初始化 COM
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        std::cerr << "COM 初始化失败!" << std::endl;
        return 1;
    }

    // 创建 Shell.Application COM 对象
    IShellWindows* pShellWindows = nullptr;
    hr = CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_ALL, IID_IShellWindows, (void**)&pShellWindows);

    if (FAILED(hr)) {
        std::cerr << "无法创建 Shell.Application 对象!" << std::endl;
        CoUninitialize();
        return 1;
    }

    // 遍历所有打开的 Windows 资源管理器窗口
    long count = 0;
    pShellWindows->get_Count(&count);

    for (long i = 0; i < count; i++) {
        VARIANT vIndex;
        vIndex.vt = VT_I4;
        vIndex.lVal = i;

        IDispatch* pDisp = nullptr;
        pShellWindows->Item(vIndex, &pDisp);
        if (pDisp) {
            IWebBrowser2* pWebBrowser = nullptr;
            hr = pDisp->QueryInterface(IID_IWebBrowser2, (void**)&pWebBrowser);
            if (SUCCEEDED(hr)) {
                BSTR vURL;
                // 获取 Explorer 的位置 URL
                hr = pWebBrowser->get_LocationURL(&vURL);
                if (SUCCEEDED(hr)) {
                    char* lpszText2 = _com_util::ConvertBSTRToString(vURL);
                    std::cout << "Explorer 路径: " << lpszText2 << std::endl;
                    delete[] lpszText2;
                    SysFreeString(vURL);
                }
                pWebBrowser->Release();
            }
            pDisp->Release();
        }
    }

    // 释放 ShellWindows 对象
    pShellWindows->Release();

    // 关闭 COM
    CoUninitialize();
    return 0;
}
