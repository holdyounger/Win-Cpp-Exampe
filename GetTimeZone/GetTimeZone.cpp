// GetTimeZone.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

int main() 
{
    TIME_ZONE_INFORMATION timeZoneInfo;
    LANGID langId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED); // 2052
    LANGID langId1 = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL); // 1028
    LANGID langId2 = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US); // 1033

    DWORD result = GetTimeZoneInformation(&timeZoneInfo);
    if (result == TIME_ZONE_ID_INVALID) {
        std::cerr << "获取时区信息失败" << std::endl;
        return 1;
    }

    // 显示时区信息
    std::cout << "标准名称: " << timeZoneInfo.StandardName << std::endl;
    std::cout << "夏令时名称: " << timeZoneInfo.DaylightName << std::endl;
    std::cout << "与 UTC 的偏移量（分钟）: " << timeZoneInfo.Bias << std::endl;
    std::cout << "标准时间开始: " << timeZoneInfo.StandardDate.wMonth << "/" << timeZoneInfo.StandardDate.wDay << " "
        << timeZoneInfo.StandardDate.wHour << ":" << timeZoneInfo.StandardDate.wMinute << std::endl;
    std::cout << "夏令时开始: " << timeZoneInfo.DaylightDate.wMonth << "/" << timeZoneInfo.DaylightDate.wDay << " "
        << timeZoneInfo.DaylightDate.wHour << ":" << timeZoneInfo.DaylightDate.wMinute << std::endl;

    std::cout << "矫正日期：" << -timeZoneInfo.Bias / 60 << std::endl;

    system("pause");

    return 0;
}