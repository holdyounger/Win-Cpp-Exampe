# CrashReproDemo

复现 EPSVHRule.dll 5.dmp 崩溃 — ATL CString 未初始化导致空指针访问

## 崩溃背景

### 原始崩溃 (5.dmp)
- **崩溃模块**: EPSVHRule.dll
- **崩溃函数**: `CAtlStringMgr::GetInstance+0x34`
- **崩溃指令**: `mov edx, dword ptr [ecx+eax*4]` with `ecx=0` → 0xC0000005
- **根因**: `dllmain.cpp` 中 DllMain 未初始化 ATL 模块 (`_Module.Init()`)，但 `stdafx.h` 包含了 `<atlstr.h>`，`CShimDB` 类使用了 `CString` 成员变量

### 崩溃调用链
```
uninst.exe
  → LoadLibrary("EPSVHRule.dll")
    → _DllMainCRTStartup
      → _initterm
        → InitializeCAtlStringMgr (静态初始化器)
          → CAtlStringMgr::GetInstance()
            → 返回 NULL → 空指针读取 → 0xC0000005
```

> **注意**: 在 VS2017 (v141_xp) + XP SP3 环境下，`atlstr.h` 会注册静态初始化器 `InitializeCAtlStringMgr`，在 `_initterm` 阶段（DllMain 之前）执行 `CAtlStringMgr::GetInstance()`，如果 ATL 模块未正确初始化，该函数返回 NULL 导致崩溃。

## 项目结构

```
CrashReproDemo/
├── CrashReproDemo.sln
├── README.md
├── BadDll/              # 缺陷版 DLL (使用 ATL CString，不初始化 ATL)
│   ├── BadDll.cpp
│   ├── BadDll.def
│   └── BadDll.vcxproj
├── GoodDll/             # 修复版 DLL (使用 std::wstring，无 ATL 依赖)
│   ├── GoodDll.cpp
│   ├── GoodDll.def
│   └── GoodDll.vcxproj
└── CrashReproDemo/      # 测试程序
    ├── CrashReproDemo.cpp
    └── CrashReproDemo.vcxproj
```

## 项目说明

### BadDll (缺陷版)
- 包含 `#include <atlstr.h>`，使用 `CString` 成员变量
- DllMain 中**不初始化** ATL 模块
- 在 XP SP3 + v141_xp 下，DLL 加载阶段即崩溃（`CAtlStringMgr::GetInstance` 返回 NULL）
- **预期结果**: `LoadLibrary` 返回 NULL，`GetLastError()` = 0xC0000005

### GoodDll (修复版)
- **移除 `#include <atlstr.h>`**，使用 `std::wstring` 代替 `CString`
- 不依赖任何 ATL 初始化
- **预期结果**: `LoadLibrary` 成功，`Uninstall()` 正常执行

### CrashReproDemo (测试程序)
- 依次加载 BadDll 和 GoodDll
- 调用各自的 `Uninstall()` 导出函数
- 使用 `__try/__except` 捕获异常
- 显示崩溃信息或成功结果

## 编译要求

- **工具集**: v141_xp (VS2017 + XP 支持)
- **平台**: Win32 (x86)
- **字符集**: Unicode
- **运行时库**: /MT (静态链接 CRT)
- **目标系统**: Windows XP SP3 / Windows 7+ / Windows 10+

## 使用方法

```
CrashReproDemo.exe           # 测试 BadDll + GoodDll
CrashReproDemo.exe bad       # 仅测试 BadDll
CrashReproDemo.exe good      # 仅测试 GoodDll
```

## EPSVHRule_Win 修复方案

### 方案 A: 移除 CString 依赖（推荐，最彻底）
将 `ShimDB.h` 中的 `CString` 替换为 `std::wstring`：
```cpp
// 修改前
CString m_strInstalledDB;
CString m_strInsExePath;

// 修改后
std::wstring m_strInstalledDB;
std::wstring m_strInsExePath;
```
需要同步修改所有使用这些成员的代码（`.GetString()` → `.c_str()`, `.Format()` → `swprintf` 等）。

### 方案 B: 添加 ATL 模块初始化（最小改动）
```cpp
// dllmain.cpp
#include <atlbase.h>
CComModule _Module;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hModule;
        _Module.Init(NULL, hModule);  // 添加
        break;
    case DLL_PROCESS_DETACH:
        _Module.Term();              // 添加
        break;
    }
    return TRUE;
}
```
> 注意: 如果崩溃发生在 `_initterm` 静态初始化阶段（DllMain 之前），此方案无效。需要确认原始 5.dmp 崩溃的确切时机。

### 方案 C: 在导出函数内部初始化 ATL
```cpp
void __stdcall Uninstall(void)
{
    CComModule _module;
    _module.Init(NULL, g_hInstance);
    
    CShimDB db;
    db.UninsatllDB();
    
    _module.Term();
}
```

## 原始项目信息
- **项目路径**: `/mnt/i/CWPP/trunk/EPSVHRule_Win/`
- **工具集**: v141_xp (Win32) / v141 (x64)
- **导出函数**: `Uninstall` (通过 Export.def)
- **关键文件**: `dllmain.cpp`, `stdafx.h`, `ShimDB.h`, `ShimDB.cpp`, `ProtectRule.cpp`
