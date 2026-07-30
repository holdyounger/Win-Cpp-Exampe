// GoodDll.cpp
// 修复版本：移除 ATL CString 依赖，改用 std::wstring
// 
// 原始问题：EPSVHRule.dll 使用 CString (ATL) 但未初始化 ATL 模块
// 在 XP SP3 + VS2017 上，atlstr.h 的静态初始化器在 DllMain 之前
// 调用 CAtlStringMgr::GetInstance()，返回 NULL → 0xC0000005
// 
// 修复思路：彻底移除 #include <atlstr.h>，用 std::wstring 替代 CString
// 这是不依赖任何 ATL 初始化的根本解决方案

#include <windows.h>
#include <string>
#include <stdio.h>

HMODULE g_hInstance = NULL;

// 模拟修复后的 CShimDB 类 — 使用 std::wstring 代替 CString
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

        printf("[GoodDll] UninstallDB: m_strInsExePath = %S\n", m_strInsExePath.c_str());
    }

private:
    std::wstring m_strInstalledDB;
    std::wstring m_strInstalledDB64;
    std::wstring m_strNewDB;
    std::wstring m_strNewDB64;
    std::wstring m_strInsExePath;
    HMODULE m_hAppHelp;
};

// DllMain — 不需要任何 ATL 初始化
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport)
void __stdcall Uninstall(void)
{
    printf("[GoodDll] Uninstall() called\n");
    CShimDB db;
    db.UninstallDB();
    printf("[GoodDll] Uninstall completed successfully\n");
}
