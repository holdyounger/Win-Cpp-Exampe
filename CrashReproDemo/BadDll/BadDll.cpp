// BadDll.cpp
// 模拟 EPSVHRule.dll 的缺陷版本：使用 ATL CString 但不初始化 ATL 模块
//
// 在 XP SP3 + VS2017 (v141_xp) 上：
// - atlstr.h 注册了静态初始化器 InitializeCAtlStringMgr
// - 在 _initterm 阶段 (DllMain 之前) 调用 CAtlStringMgr::GetInstance()
// - 该函数内部访问 NULL 指针 → 0xC0000005 Access Violation
// - 崩溃在 LoadLibrary 阶段，DLL 都没机会执行 DllMain
//
// 这正是 5.dmp 崩溃的复现：
//   GoodDll!ATL::CAtlStringMgr::GetInstance+0x34:
//   10063864 8b1481  mov edx,dword ptr [ecx+eax*4] ds:0023:00000000=????????

#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
#include <stdio.h>

HMODULE g_hInstance = NULL;

// 模拟 EPSVHRule.dll 的 CShimDB 类
// 关键：包含 CString 成员变量，触发 CAtlStringMgr 静态初始化
class CShimDB
{
public:
    CShimDB()
    {
        m_hAppHelp = NULL;
    }
    ~CShimDB()
    {
        if (m_hAppHelp) FreeLibrary(m_hAppHelp);
    }

    void UninstallDB()
    {
        TCHAR szWinDir[MAX_PATH] = { 0 };
        GetWindowsDirectory(szWinDir, MAX_PATH);
        m_strInsExePath = szWinDir;

        printf("[BadDll] UninstallDB: m_strInsExePath = %S\n", m_strInsExePath.GetString());
    }

private:
    CStringW m_strInstalledDB;
    CStringW m_strInstalledDB64;
    CStringW m_strNewDB;
    CStringW m_strNewDB64;
    CStringW m_strInsExePath;
    HMODULE m_hAppHelp;
};

// 模拟 EPSVHRule.dll 的 DllMain — 故意不初始化 ATL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hModule;
        // 故意缺失：没有 CComModule::Init() 或 _Module.Init()
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// 模拟 ProtectRule.cpp 的 Uninstall() 函数
extern "C" __declspec(dllexport)
void __stdcall Uninstall(void)
{
    printf("[BadDll] Uninstall() called\n");
    CShimDB db;
    db.UninstallDB();
    printf("[BadDll] Uninstall completed\n");
}
