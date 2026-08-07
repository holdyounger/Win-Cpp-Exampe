// MitigationPolicyDemo.cpp
// 演示 GetProcessMitigationPolicy 查询进程缓解策略
// v141_xp (Win7.1A SDK) 兼容版本

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

// ===================================================================
// 自包含结构体定义（不依赖 SDK headfile）
// ===================================================================
typedef struct _SIDE_CHANNEL_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD SideChannelIsolation : 1;
            DWORD DisablePageCombine : 1;
            DWORD DisableSpeculativeStoreBypass : 1;
            DWORD Reserved : 29;
        };
    };
} SIDE_CHANNEL_POLICY;

// 函数指针
typedef BOOL (WINAPI *GET_POLICY_FN)(HANDLE, DWORD, PVOID, SIZE_T);
typedef BOOL (WINAPI *SET_POLICY_FN)(DWORD, PVOID, SIZE_T);

// ===================================================================
// 辅助函数
// ===================================================================
static GET_POLICY_FN GetGetPolicyFn()
{
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    return h ? (GET_POLICY_FN)GetProcAddress(h, "GetProcessMitigationPolicy") : NULL;
}

static SET_POLICY_FN GetSetPolicyFn()
{
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    return h ? (SET_POLICY_FN)GetProcAddress(h, "SetProcessMitigationPolicy") : NULL;
}

// ===================================================================
// 方式 A: 硬编码掩码
// ===================================================================
BOOL HasMitigationFlags3()
{
    GET_POLICY_FN fn = GetGetPolicyFn();
    if (!fn) return FALSE;

    SIDE_CHANNEL_POLICY pol = {0};
    if (!fn(GetCurrentProcess(), 39, &pol, sizeof(pol)))
        return FALSE;

    return (pol.Flags & 0x07) != 0;  // bit0=SideChannelIsolation, bit1=DisablePageCombine, bit2=SSB
}

// ===================================================================
// 方式 B: 位域字段
// ===================================================================
BOOL HasMitigationFlags_FieldAccess()
{
    GET_POLICY_FN fn = GetGetPolicyFn();
    if (!fn) return FALSE;

    SIDE_CHANNEL_POLICY pol = {0};
    if (!fn(GetCurrentProcess(), 39, &pol, sizeof(pol)))
        return FALSE;

    return pol.SideChannelIsolation || pol.DisablePageCombine || pol.DisableSpeculativeStoreBypass;
}

// ===================================================================
// 详细打印
// ===================================================================
void PrintMitigationPolicy()
{
    GET_POLICY_FN fn = GetGetPolicyFn();
    if (!fn)
    {
        _tprintf(_T("[FAIL] GetProcessMitigationPolicy not available\n"));
        return;
    }

    SIDE_CHANNEL_POLICY pol = {0};
    if (!fn(GetCurrentProcess(), 39, &pol, sizeof(pol)))
    {
        _tprintf(_T("[FAIL] error=0x%x\n"), GetLastError());
        return;
    }

    _tprintf(_T("\n=== Side-Channel Isolation Policy ===\n"));
    _tprintf(_T("  Flags                        : 0x%x\n"), pol.Flags);
    _tprintf(_T("  SideChannelIsolation         : %s\n"),
        pol.SideChannelIsolation ? _T("ENABLED") : _T("disabled"));
    _tprintf(_T("  DisablePageCombine           : %s\n"),
        pol.DisablePageCombine ? _T("ENABLED") : _T("disabled"));
    _tprintf(_T("  DisableSpeculativeStoreBypass: %s\n\n"),
        pol.DisableSpeculativeStoreBypass ? _T("ENABLED") : _T("disabled"));
}

// ===================================================================
// 尝试启用策略
// ===================================================================
void TryEnablePolicies()
{
    SET_POLICY_FN fnSet = GetSetPolicyFn();
    if (!fnSet)
    {
        _tprintf(_T("[SKIP] SetProcessMitigationPolicy not available\n"));
        return;
    }

    SIDE_CHANNEL_POLICY pol = {0};
    pol.SideChannelIsolation = 1;
    pol.DisablePageCombine = 1;
    pol.DisableSpeculativeStoreBypass = 1;

    if (fnSet(39, &pol, sizeof(pol)))
        _tprintf(_T("[OK]   SetProcessMitigationPolicy succeeded\n"));
    else
    {
        DWORD err = GetLastError();
        _tprintf(_T("[FAIL] SetProcessMitigationPolicy error=0x%x\n"), err);
        if (err == ERROR_ACCESS_DENIED)
            _tprintf(_T("       (需要管理员权限)\n"));
    }
}

// ===================================================================
// 主函数
// ===================================================================
int _tmain()
{
    _tprintf(_T("=== MitigationPolicyDemo ===\n"));
#ifdef _WIN64
    _tprintf(_T("Platform: x64\n\n"));
#else
    _tprintf(_T("Platform: x86\n\n"));
#endif

    TryEnablePolicies();
    PrintMitigationPolicy();

    BOOL r1 = HasMitigationFlags3();
    BOOL r2 = HasMitigationFlags_FieldAccess();

    _tprintf(_T("HasMitigationFlags3()           : %s\n"), r1 ? _T("YES") : _T("NO"));
    _tprintf(_T("HasMitigationFlags_FieldAccess(): %s\n"), r2 ? _T("YES") : _T("NO"));

    _tprintf(_T("%s\n\n"), (r1 == r2) ? _T("[OK] 一致") : _T("[WARN] 不一致！"));

    _tprintf(_T("按 Enter 退出..."));
    getchar();
    return 0;
}
