// RemoveAndRestoreMountPoint.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <Windows.h>

void RemoveAndRestoreMountPoint(wchar_t* mountPoint) {
    // 删除挂载点
    DeleteVolumeMountPointW(mountPoint);

    // 重新为卷分配盘符
    // 假设 E: 是要重新分配的盘符
    wchar_t devicePath[] = L"\\Device\\HarddiskVolume1000"; // X代表具体的卷号，需要获取具体的卷路径
    DefineDosDeviceW(DDD_RAW_TARGET_PATH, L"H:", devicePath);

    // 删除
    DefineDosDeviceW(DDD_REMOVE_DEFINITION, L"H:", NULL);
}

int main() {
    RemoveAndRestoreMountPoint((wchar_t*)L"H:\\");  // 假设 E: 是 U 盘的挂载点
    return 0;
}
