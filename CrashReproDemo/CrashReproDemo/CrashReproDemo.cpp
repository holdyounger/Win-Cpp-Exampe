// CrashReproDemo.cpp
// CrashReproDemo — 复现 EPSVHRule.dll 5.dmp 崩溃
//
// 崩溃根因: ATL CString 使用前未初始化 ATL 模块
// - BadDll: 包含 #include <atlstr.h>，触发 CAtlStringMgr 静态初始化
//   → CAtlStringMgr::GetInstance() 返回 NULL → 0xC0000005
// - GoodDll: 移除 ATL 依赖，使用 std::wstring，不会崩溃
//
// 用法: CrashReproDemo.exe [bad|good|both]

#include <windows.h>
#include <stdio.h>
#include <conio.h>

typedef void (__stdcall *PFN_Uninstall)(void);

void TestDll(const wchar_t* dllName, const char* label)
{
    wprintf(L"\n========== Testing %hs ==========\n", label);
    wprintf(L"[+] Loading %s ...\n", dllName);

    HMODULE hMod = LoadLibraryW(dllName);
    if (!hMod)
    {
        DWORD err = GetLastError();
        printf("[!] LoadLibrary(%S) failed, error=%lu\n", dllName, err);

        if (err == 0xC0000005)
        {
            printf("[!] Error 0xC0000005 = ACCESS_VIOLATION\n");
            printf("[!] DLL crashed during initialization (ATL static init)\n");
            printf("[!] This reproduces the 5.dmp crash!\n");
        }
        else
        {
            printf("[!] Error %lu (0x%lX)\n", err, err);
        }
        return;
    }

    printf("[+] %S loaded at 0x%p\n", dllName, hMod);

    PFN_Uninstall pfn = (PFN_Uninstall)GetProcAddress(hMod, "Uninstall");
    if (!pfn)
    {
        DWORD err = GetLastError();
        printf("[!] GetProcAddress(Uninstall) failed, error=%lu\n", err);
        FreeLibrary(hMod);
        return;
    }

    printf("[+] Uninstall found at 0x%p\n", pfn);
    printf("[+] Calling Uninstall()...\n");

    __try
    {
        pfn();
        printf("[+] Uninstall() returned successfully\n");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DWORD code = GetExceptionCode();
        printf("[!] Exception caught: 0x%08lX\n", code);
        if (code == 0xC0000005)
        {
            printf("[!] ACCESS_VIOLATION — This reproduces the 5.dmp crash!\n");
        }
    }

    FreeLibrary(hMod);
    printf("[+] %S unloaded\n", dllName);
}

int main(int argc, char* argv[])
{
    printf("=== CrashReproDemo ===\n");
    printf("Reproduces EPSVHRule.dll 5.dmp crash (ATL CString not initialized)\n\n");

    const char* mode = (argc > 1) ? argv[1] : "both";

    if (_stricmp(mode, "bad") == 0)
    {
        TestDll(L"BadDll.dll", "BadDll (ATL CString, no init)");
    }
    else if (_stricmp(mode, "good") == 0)
    {
        TestDll(L"GoodDll.dll", "GoodDll (std::wstring, no ATL)");
    }
    else
    {
        TestDll(L"BadDll.dll", "BadDll (ATL CString, no init)");
        printf("\n");
        TestDll(L"GoodDll.dll", "GoodDll (std::wstring, no ATL)");
    }

    printf("\n=== Done ===\n");
    printf("Press any key to exit...\n");
    _getch();
    return 0;
}
