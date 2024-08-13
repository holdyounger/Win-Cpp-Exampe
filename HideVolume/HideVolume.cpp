#include <windows.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

struct SaveContext
{
	wstring strNtPath;
	wstring strDosDevice;
};

typedef vector<SaveContext> svrVector;

void RestoreDevice(SaveContext& Context);

int main(void)
{
	//读磁盘配置并排除C盘
	DWORD dwDisks = GetLogicalDrives() & 0xfffffffb;

	svrVector NtDevices;

	WCHAR chDosDevice[8] = L"A:";

	WCHAR chNtPath[MAX_PATH] = { 0 };

	for (int Mask = 1; Mask; Mask <<= 1, chDosDevice[0]++)
	{
		if (dwDisks & Mask)
		{
			QueryDosDevice(chDosDevice, chNtPath, MAX_PATH);

			SaveContext Context;
			Context.strDosDevice = chDosDevice;
			Context.strNtPath = chNtPath;

			//先保存符号名和设备之间的关系
			NtDevices.push_back(Context);

			wcout << Context.strDosDevice << "<--->" << Context.strNtPath << endl;

			//除了C:以外其它的盘符正一个一个的不见了
			// DefineDosDevice(DDD_REMOVE_DEFINITION, Context.strDosDevice.c_str(), NULL);
		}
	}

	//现在打开"我的电脑"看看
	system("pause");

	//现在隐藏的盘符又出现了,呵呵
	// for_each(NtDevices.begin(), NtDevices.end(), RestoreDevice);

	return 0;
}

void RestoreDevice(SaveContext& Context)
{
	DefineDosDevice(DDD_RAW_TARGET_PATH, Context.strDosDevice.c_str(), Context.strNtPath.c_str());
}