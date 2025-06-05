#include <windows.h>
#include <commctrl.h>
#include <iostream>

// 递归查找指定类名的子窗口
HWND FindChildWindow(HWND parent, const char* className) {
    HWND child = GetWindow(parent, GW_CHILD);
    while (child) {
        char clsName[256];
        GetClassNameA(child, clsName, sizeof(clsName));
        if (strcmp(clsName, className) == 0) {
            return child;
        }
        HWND found = FindChildWindow(child, className);
        if (found) return found;
        child = GetWindow(child, GW_HWNDNEXT);
    }
    return NULL;
}

void GetHoveredFileInfo() {
    POINT pt;
    GetCursorPos(&pt);  // 获取鼠标位置

    HWND hWnd = WindowFromPoint(pt);  // 获取鼠标下的窗口
    if (!hWnd) return;

    char className[256];
    GetClassNameA(hWnd, className, sizeof(className));

    // 检测是否是 Explorer 窗口
    if (strcmp(className, "CabinetWClass") == 0 || strcmp(className, "ExploreWClass") == 0) {
        // 递归查找 SysListView32
        HWND hListView = FindChildWindow(hWnd, "SysListView32");
        if (!hListView) return;

        LVHITTESTINFO lvhti = { 0 };
        lvhti.pt = pt;
        ScreenToClient(hListView, &lvhti.pt);

        // 进行命中测试
        SendMessage(hListView, LVM_HITTEST, 0, (LPARAM)&lvhti);
        if (lvhti.iItem != -1) {
            char buffer[256] = { 0 };
            LVITEMA lvi = { 0 };
            lvi.iSubItem = 0;
            lvi.pszText = buffer;
            lvi.cchTextMax = sizeof(buffer);
            lvi.mask = LVIF_TEXT;
            lvi.iItem = lvhti.iItem;

            // 获取文件名
            SendMessageA(hListView, LVM_GETITEMTEXTA, lvhti.iItem, (LPARAM)&lvi);
            std::cout << "Hovered file: " << buffer << std::endl;
        }
    }
}

int main() {
    while (true) {
        GetHoveredFileInfo();
        Sleep(1000);  // 每秒检测一次
    }
    return 0;
}
