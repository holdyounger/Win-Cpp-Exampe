// ProcessChainDemo.cpp : 进程链查找 Demo
// 基于 360EDR FileAction 进程链查找机制的简化实现
// 演示：进程启动时间获取、父进程PID获取、进程链向上遍历、根进程定位

#include <iostream>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <ctime>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ntdll.lib")

// ============================================================================
// ntdll 未文档化声明
// ============================================================================

typedef LONG(NTAPI* pfnNtQueryInformationProcess)(
    IN  HANDLE ProcessHandle,
    IN  ULONG  ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN  ULONG  ProcessInformationLength,
    OUT PULONG ReturnLength
    );

typedef struct _PROCESS_BASIC_INFORMATION_DYN {
    PVOID Reserved1;
    PVOID PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;  // InheritedFromUniqueProcessId
} PROCESS_BASIC_INFORMATION_DYN;

// ============================================================================
// 数据结构
// ============================================================================

// 进程信息记录（简化版 EVENT_INFO）
struct ProcRecord {
    DWORD       dwPID = 0;
    DWORD       dwParentID = 0;
    DWORD       dwRootPID = 0;
    __int64     ProcessStartTime = 0;
    __int64     ParentStartTime = 0;
    __int64     RootStartTime = 0;
    short       nLinkLevel = 0;
    int         nChildCount = 0;
    std::wstring ProcessName;
    std::wstring ProcessPath;
    std::wstring RootProcessName;
    bool        bFiltOver = false;
};

// MAKE_KEY: PID * 10^10 + StartTime % 10^10
#define MAKE_KEY(dwPid, StartTime) ((__int64)((unsigned __int64)(dwPid) * (unsigned __int64)10000000000ULL + (unsigned __int64)(StartTime) % (unsigned __int64)10000000000ULL))

// 全局进程树
std::map<__int64, ProcRecord> g_ProcessTree;

// 停止进程列表（遇到这些进程时确定为根进程）
const std::vector<std::wstring> g_StopProcesses = {
    L"explorer.exe", L"svchost.exe", L"lsass.exe", L"services.exe",
    L"winlogon.exe", L"wininit.exe", L"csrss.exe", L"smss.exe",
    L"userinit.exe", L"taskmgr.exe",
    L"360desktoplite.exe", L"360desktoplite64.exe"
};

// 浏览器进程列表
const std::vector<std::wstring> g_BrowserProcesses = {
    L"chrome.exe", L"msedge.exe", L"iexplore.exe",
    L"360se.exe", L"360chrome.exe", L"qqbrowser.exe"
};

// ============================================================================
// 工具函数
// ============================================================================

// 判断是否为停止进程
bool IsStopProcess(const std::wstring& processName) {
    std::wstring lower = processName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& name : g_StopProcesses) {
        if (lower == name) return true;
    }
    for (const auto& name : g_BrowserProcesses) {
        if (lower == name) return true;
    }
    return false;
}

// 获取进程名（通过 PID）
std::wstring GetProcessNameById(DWORD dwPid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPid);
    if (!hProcess) return L"";

    WCHAR szPath[MAX_PATH] = { 0 };
    std::wstring name;
    if (GetModuleFileNameExW(hProcess, NULL, szPath, MAX_PATH)) {
        std::wstring fullPath(szPath);
        size_t pos = fullPath.find_last_of(L"\\/");
        name = (pos != std::wstring::npos) ? fullPath.substr(pos + 1) : fullPath;
    }

    // OpenProcess 失败时尝试用 Toolhelp32Snapshot
    if (name.empty()) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (pe.th32ProcessID == dwPid) {
                        name = pe.szExeFile;
                        break;
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
    }

    CloseHandle(hProcess);
    return name;
}

// 获取进程路径
std::wstring GetProcessPathById(DWORD dwPid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPid);
    if (!hProcess) return L"";

    WCHAR szPath[MAX_PATH] = { 0 };
    std::wstring path;
    if (GetModuleFileNameExW(hProcess, NULL, szPath, MAX_PATH)) {
        path = szPath;
    }
    CloseHandle(hProcess);
    return path;
}

// ============================================================================
// 辅助函数：Unix 时间戳 → 可读日期时间字符串
// ============================================================================

std::wstring FormatUnixTime(__int64 unixTime) {
    if (unixTime == 0) return L"N/A";

    time_t tt = (time_t)unixTime;
    struct tm lt = { 0 };
    if (localtime_s(&lt, &tt) != 0) return L"invalid";

    WCHAR buf[64] = { 0 };
    wcsftime(buf, 64, L"%Y-%m-%d %H:%M:%S", &lt);
    return std::wstring(buf);
}

// ============================================================================
// 核心函数：获取进程启动时间（FILETIME → Unix 时间戳）
// ============================================================================

bool GetProcStartTime(DWORD dwPid, __int64& llCreateTime) {
    llCreateTime = 0;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPid);
    if (!hProcess) return false;

    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    BOOL bRet = GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser);
    CloseHandle(hProcess);

    if (!bRet) return false;

    // FILETIME (1601纪元, 100ns单位) → Unix时间戳 (1970纪元, 秒)
    ULARGE_INTEGER uli;
    uli.LowPart = ftCreate.dwLowDateTime;
    uli.HighPart = ftCreate.dwHighDateTime;
    llCreateTime = (__int64)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);

    return true;
}

// ============================================================================
// 核心函数：获取父进程 PID（NtQueryInformationProcess）
// ============================================================================

DWORD GetParentProcessId(HANDLE hProcess) {
    static pfnNtQueryInformationProcess pNtQueryInfo = nullptr;
    if (!pNtQueryInfo) {
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (!hNtdll) return 0;
        pNtQueryInfo = (pfnNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");
        if (!pNtQueryInfo) return 0;
    }

    PROCESS_BASIC_INFORMATION_DYN pbi = { 0 };
    ULONG returnLength = 0;
    LONG status = pNtQueryInfo(hProcess, 0 /* ProcessBasicInformation */, &pbi, sizeof(pbi), &returnLength);
    if (status != 0) return 0;

    return (DWORD)(ULONG_PTR)pbi.Reserved3;  // InheritedFromUniqueProcessId
}

DWORD GetParentProcessIdByPid(DWORD dwPid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPid);
    if (!hProcess) {
        // 尝试有限权限
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPid);
        if (!hProcess) return 0;
    }
    DWORD dwParent = GetParentProcessId(hProcess);
    CloseHandle(hProcess);
    return dwParent;
}

// ============================================================================
// 核心函数：进程树记录查找
// ============================================================================

ProcRecord* GetProcessTreeRecord(DWORD dwPid, __int64 startTime) {
    auto it = g_ProcessTree.find(MAKE_KEY(dwPid, startTime));
    if (it != g_ProcessTree.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// 核心函数：构建进程链
// 从指定 PID 开始，向上遍历父进程链，填充 RootProcess 信息
// ============================================================================

bool SetProcessRootInfo(ProcRecord& record) {
    DWORD currentPid = record.dwParentID;
    __int64 currentStartTime = record.ParentStartTime;

    // 如果父进程启动时间未知，尝试获取
    if (currentStartTime == 0 && currentPid != 0) {
        GetProcStartTime(currentPid, currentStartTime);
        record.ParentStartTime = currentStartTime;
    }

    ProcRecord* pParent = nullptr;
    ProcRecord* pLastValid = nullptr;

    int loopGuard = 0;
    while (currentPid != 0 && loopGuard++ < 100) {
        pParent = GetProcessTreeRecord(currentPid, currentStartTime);

        if (!pParent) {
            // 父进程记录不存在，可能已退出或不在树中
            // 尝试实时获取信息
            std::wstring parentName = GetProcessNameById(currentPid);
            if (IsStopProcess(parentName) || parentName.empty()) {
                // 父进程是停止进程或无法访问 → 设为根
                record.dwRootPID = currentPid;
                record.RootStartTime = currentStartTime;
                record.RootProcessName = parentName;
                return true;
            }
            break;
        }

        pLastValid = pParent;

        // 检查父进程是否为停止进程
        if (IsStopProcess(pParent->ProcessName)) {
            record.dwRootPID = pParent->dwPID;
            record.RootStartTime = pParent->ProcessStartTime;
            record.RootProcessName = pParent->ProcessName;
            return true;
        }

        // 继承根信息（如果父进程已有）
        if (pParent->dwRootPID != 0) {
            record.dwRootPID = pParent->dwRootPID;
            record.RootStartTime = pParent->RootStartTime;
            record.RootProcessName = pParent->RootProcessName;
            return true;
        }

        // 向上遍历
        currentPid = pParent->dwParentID;
        currentStartTime = pParent->ParentStartTime;

        if (currentPid == 0 || currentStartTime == 0) {
            // 无法继续向上 → 以最近的父进程为根
            record.dwRootPID = pParent->dwPID;
            record.RootStartTime = pParent->ProcessStartTime;
            record.RootProcessName = pParent->ProcessName;
            return true;
        }
    }

    // 如果有最近的有效父进程，以它为根
    if (pLastValid) {
        record.dwRootPID = pLastValid->dwPID;
        record.RootStartTime = pLastValid->ProcessStartTime;
        record.RootProcessName = pLastValid->ProcessName;
        return true;
    }

    // 没有任何父进程信息 → 自己就是根
    record.dwRootPID = record.dwPID;
    record.RootStartTime = record.ProcessStartTime;
    record.RootProcessName = record.ProcessName;
    return false;
}

// ============================================================================
// 核心函数：打印进程链
// 从指定进程开始，向上遍历到根进程，打印完整链路
// ============================================================================

void PrintProcessChain(DWORD dwPid, __int64 startTime) {
    std::wcout << L"\n========== 进程链 (PID=" << dwPid << L") ==========\n";

    ProcRecord* pCurrent = GetProcessTreeRecord(dwPid, startTime);
    if (!pCurrent) {
        std::wcout << L"  [!] 进程记录不存在 (PID=" << dwPid << L")\n";
        return;
    }

    int level = 0;
    DWORD currentPid = pCurrent->dwPID;
    __int64 currentStartTime = pCurrent->ProcessStartTime;

    std::wcout << L"  [" << level << L"] PID=" << std::setw(6) << currentPid
              << L"  " << pCurrent->ProcessName
              << L"  (Start=" << FormatUnixTime(pCurrent->ProcessStartTime)
              << L" [" << pCurrent->ProcessStartTime << L"])\n";

    // 向上遍历
    while (true) {
        ProcRecord* pRec = GetProcessTreeRecord(currentPid, currentStartTime);
        if (!pRec || pRec->dwParentID == 0) break;

        __int64 parentStartTime = pRec->ParentStartTime;
        if (parentStartTime == 0) {
            GetProcStartTime(pRec->dwParentID, parentStartTime);
        }

        ProcRecord* pParent = GetProcessTreeRecord(pRec->dwParentID, parentStartTime);
        if (!pParent) {
            // 父进程不在树中，尝试实时获取名称
            std::wstring parentName = GetProcessNameById(pRec->dwParentID);
            level++;
            std::wcout << L"  [" << level << L"] PID=" << std::setw(6) << pRec->dwParentID
                      << L"  " << (parentName.empty() ? L"<unknown>" : parentName)
                      << L"  (Start=" << FormatUnixTime(parentStartTime)
                      << L" [" << parentStartTime << L"])"
                      << (IsStopProcess(parentName) ? L"  [STOP]" : L"")
                      << L"  <not in tree>\n";
            break;
        }

        level++;
        std::wcout << L"  [" << level << L"] PID=" << std::setw(6) << pParent->dwPID
                  << L"  " << pParent->ProcessName
                  << L"  (Start=" << FormatUnixTime(pParent->ProcessStartTime)
                  << L" [" << pParent->ProcessStartTime << L"])"
                  << (IsStopProcess(pParent->ProcessName) ? L"  [STOP]" : L"")
                  << L"\n";

        // 到达根进程
        if (pParent->dwPID == pCurrent->dwRootPID) {
            break;
        }

        currentPid = pParent->dwPID;
        currentStartTime = pParent->ProcessStartTime;
    }

    // 打印根进程信息
    std::wcout << L"\n  Root Process: PID=" << pCurrent->dwRootPID
              << L"  Name=" << pCurrent->RootProcessName
              << L"  Start=" << FormatUnixTime(pCurrent->RootStartTime)
              << L" [" << pCurrent->RootStartTime << L"]\n";
    std::wcout << L"  Link Level: " << pCurrent->nLinkLevel << L"\n";
    std::wcout << L"==========================================\n";
}

// ============================================================================
// 构建进程树快照
// 使用 Toolhelp32Snapshot 枚举所有进程，构建进程链
// ============================================================================

void BuildProcessTreeSnapshot() {
    std::wcout << L"\n>>> 构建进程树快照...\n";

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"CreateToolhelp32Snapshot failed: " << GetLastError() << std::endl;
        return;
    }

    PROCESSENTRY32W pe = { sizeof(pe) };
    int count = 0;

    // 第一遍：枚举所有进程，创建记录
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            ProcRecord rec;
            rec.dwPID = pe.th32ProcessID;
            rec.dwParentID = pe.th32ParentProcessID;
            rec.ProcessName = pe.szExeFile;
            rec.ProcessPath = GetProcessPathById(pe.th32ProcessID);

            // 获取进程启动时间
            __int64 startTime = 0;
            if (GetProcStartTime(pe.th32ProcessID, startTime)) {
                rec.ProcessStartTime = startTime;
            } else {
                // 可能是 System Idle Process (PID=0) 或受保护进程
                rec.ProcessStartTime = 0;
            }

            // 获取父进程启动时间
            if (rec.dwParentID != 0) {
                __int64 parentStart = 0;
                GetProcStartTime(rec.dwParentID, parentStart);
                rec.ParentStartTime = parentStart;
            }

            __int64 key = MAKE_KEY(rec.dwPID, rec.ProcessStartTime);
            g_ProcessTree[key] = rec;
            count++;

        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);

    std::wcout << L"  已添加 " << count << L" 个进程记录\n";

    // 第二遍：为每个记录设置根进程信息
    std::wcout << L"  正在定位根进程...\n";

    for (auto& pair : g_ProcessTree) {
        ProcRecord& rec = pair.second;

        if (IsStopProcess(rec.ProcessName)) {
            // 停止进程自身就是根
            rec.dwRootPID = rec.dwPID;
            rec.RootStartTime = rec.ProcessStartTime;
            rec.RootProcessName = rec.ProcessName;
            continue;
        }

        SetProcessRootInfo(rec);
    }

    // 第三遍：计算链等级
    for (auto& pair : g_ProcessTree) {
        ProcRecord& rec = pair.second;
        if (rec.dwRootPID == rec.dwPID) {
            rec.nLinkLevel = 0;  // 根进程
        } else {
            // 计算到根的层级
            short level = 0;
            DWORD curPid = rec.dwPID;
            __int64 curStart = rec.ProcessStartTime;
            int guard = 0;
            while (guard++ < 100) {
                ProcRecord* pRec = GetProcessTreeRecord(curPid, curStart);
                if (!pRec) break;
                if (pRec->dwPID == pRec->dwRootPID) break;
                level++;
                curPid = pRec->dwParentID;
                curStart = pRec->ParentStartTime;
                if (curPid == 0) break;
            }
            rec.nLinkLevel = level;
        }
    }

    std::wcout << L"  进程树构建完成！\n";
}

// ============================================================================
// 打印进程树统计
// ============================================================================

void PrintTreeStats() {
    std::wcout << L"\n>>> 进程树统计\n";
    std::wcout << L"  总进程数: " << g_ProcessTree.size() << L"\n";

    int rootCount = 0, leafCount = 0, maxLevel = 0;
    std::map<std::wstring, int> rootCounts;

    for (const auto& pair : g_ProcessTree) {
        const auto& rec = pair.second;
        if (rec.dwPID == rec.dwRootPID) rootCount++;
        if (rec.nChildCount == 0) leafCount++;
        if (rec.nLinkLevel > maxLevel) maxLevel = rec.nLinkLevel;
        rootCounts[rec.RootProcessName]++;
    }

    std::wcout << L"  根进程数: " << rootCount << L"\n";
    std::wcout << L"  叶子进程数: " << leafCount << L"\n";
    std::wcout << L"  最大链深度: " << maxLevel << L"\n";

    std::wcout << L"\n  根进程分布 (Top 10):\n";
    std::vector<std::pair<int, std::wstring>> sortedRoots;
    for (const auto& p : rootCounts) {
        sortedRoots.push_back({ p.second, p.first });
    }
    std::sort(sortedRoots.begin(), sortedRoots.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    int shown = 0;
    for (const auto& p : sortedRoots) {
        if (shown++ >= 10) break;
        std::wcout << L"    " << std::setw(6) << p.first << L"  " << p.second << L"\n";
    }
}

// ============================================================================
// 查找指定进程的链
// ============================================================================

void FindProcessChain(const std::wstring& targetName) {
    std::wcout << L"\n>>> 查找进程: " << targetName << L"\n";

    int found = 0;
    for (const auto& pair : g_ProcessTree) {
        const auto& rec = pair.second;
        std::wstring lowerName = rec.ProcessName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

        if (lowerName.find(targetName) != std::wstring::npos) {
            found++;
            PrintProcessChain(rec.dwPID, rec.ProcessStartTime);
            if (found >= 5) {
                std::wcout << L"  ... (more results, showing first 5)\n";
                break;
            }
        }
    }

    if (found == 0) {
        std::wcout << L"  未找到匹配进程\n";
    }
}

// ============================================================================
// 打印指定 PID 的进程链
// ============================================================================

void FindProcessChainByPid(DWORD dwPid) {
    __int64 startTime = 0;
    if (!GetProcStartTime(dwPid, startTime)) {
        std::wcout << L"\n[!] 无法获取 PID=" << dwPid << L" 的启动时间\n";
        return;
    }

    // 如果不在树中，临时添加
    ProcRecord* pRec = GetProcessTreeRecord(dwPid, startTime);
    if (!pRec) {
        std::wcout << L"\n[!] PID=" << dwPid << L" 不在进程树快照中，尝试实时构建...\n";

        ProcRecord rec;
        rec.dwPID = dwPid;
        rec.ProcessName = GetProcessNameById(dwPid);
        rec.ProcessPath = GetProcessPathById(dwPid);
        rec.ProcessStartTime = startTime;
        rec.dwParentID = GetParentProcessIdByPid(dwPid);
        if (rec.dwParentID != 0) {
            GetProcStartTime(rec.dwParentID, rec.ParentStartTime);
        }

        __int64 key = MAKE_KEY(rec.dwPID, rec.ProcessStartTime);
        g_ProcessTree[key] = rec;
        SetProcessRootInfo(g_ProcessTree[key]);
    }

    PrintProcessChain(dwPid, startTime);
}

// ============================================================================
// 交互式菜单
// ============================================================================

void PrintMenu() {
    std::wcout << L"\n";
    std::wcout << L"========================================\n";
    std::wcout << L"  Process Chain Demo\n";
    std::wcout << L"========================================\n";
    std::wcout << L"  1. 重建进程树快照\n";
    std::wcout << L"  2. 打印进程树统计\n";
    std::wcout << L"  3. 按进程名查找链 (如 cmd.exe)\n";
    std::wcout << L"  4. 按 PID 查找链\n";
    std::wcout << L"  5. 打印当前进程链\n";
    std::wcout << L"  6. 打印所有根进程的子进程列表\n";
    std::wcout << L"  0. 退出\n";
    std::wcout << L"========================================\n";
    std::wcout << L"  选择> ";
}

void PrintCurrentProcessChain() {
    DWORD currentPid = GetCurrentProcessId();
    FindProcessChainByPid(currentPid);
}

void PrintRootChildren() {
    std::wcout << L"\n>>> 根进程及其子进程\n";

    // 收集每个根进程的子进程
    std::map<DWORD, std::vector<ProcRecord*>> rootChildren;
    for (auto& pair : g_ProcessTree) {
        ProcRecord& rec = pair.second;
        rootChildren[rec.dwRootPID].push_back(&rec);
    }

    // 打印每个根进程的子树
    for (const auto& rootPair : rootChildren) {
        DWORD rootPid = rootPair.first;
        const auto& children = rootPair.second;

        // 获取根进程名
        std::wstring rootName;
        for (const auto* p : children) {
            if (p->dwPID == rootPid) {
                rootName = p->ProcessName;
                break;
            }
        }
        if (rootName.empty()) {
            rootName = GetProcessNameById(rootPid);
        }

        std::wcout << L"\n  [ROOT] PID=" << std::setw(6) << rootPid
                  << L"  " << (rootName.empty() ? L"<unknown>" : rootName)
                  << L"  (" << children.size() << L" processes)\n";

        // 只显示前 20 个子进程
        int shown = 0;
        for (const auto* p : children) {
            if (p->dwPID == rootPid) continue;  // 跳过根进程自身
            if (shown++ >= 20) {
                std::wcout << L"    ... (" << children.size() - 21 << L" more)\n";
                break;
            }
            std::wcout << L"    PID=" << std::setw(6) << p->dwPID
                      << L"  L" << p->nLinkLevel
                      << L"  " << p->ProcessName;
            if (p->bFiltOver) std::wcout << L"  [RISK]";
            std::wcout << L"\n";
        }
    }
}

// ============================================================================
// main
// ============================================================================

int main() {
    // 设置控制台输出为 UTF-16
    _setmode(_fileno(stdout), _O_U16TEXT); // _O_U16TEXT
    _setmode(_fileno(stderr), _O_U16TEXT);

    std::wcout << L"\n";
    std::wcout << L"╔══════════════════════════════════════════════╗\n";
    std::wcout << L"║     Process Chain Demo - 进程链查找演示       ║\n";
    std::wcout << L"║     基于 360EDR FileAction 机制简化实现      ║\n";
    std::wcout << L"╚══════════════════════════════════════════════╝\n";

    // 初始构建
    BuildProcessTreeSnapshot();
    PrintTreeStats();
    PrintCurrentProcessChain();

    // 交互循环
    while (true) {
        PrintMenu();

        std::wstring input;
        std::getline(std::wcin, input);

        if (input.empty()) continue;

        int choice = _wtoi(input.c_str());
        switch (choice) {
        case 1:
            g_ProcessTree.clear();
            BuildProcessTreeSnapshot();
            PrintTreeStats();
            break;
        case 2:
            PrintTreeStats();
            break;
        case 3: {
            std::wcout << L"  进程名 (如 cmd.exe): ";
            std::wstring name;
            std::getline(std::wcin, name);
            if (!name.empty()) {
                std::transform(name.begin(), name.end(), name.begin(), ::towlower);
                FindProcessChain(name);
            }
            break;
        }
        case 4: {
            std::wcout << L"  PID: ";
            std::wstring pidStr;
            std::getline(std::wcin, pidStr);
            DWORD pid = (DWORD)_wtol(pidStr.c_str());
            if (pid > 0) {
                FindProcessChainByPid(pid);
            }
            break;
        }
        case 5:
            PrintCurrentProcessChain();
            break;
        case 6:
            PrintRootChildren();
            break;
        case 0:
            std::wcout << L"\n  Goodbye!\n";
            return 0;
        default:
            std::wcout << L"  无效选择\n";
            break;
        }
    }

    return 0;
}