
#include <windows.h>
#include <setupapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <winioctl.h>
#include <initguid.h>
#include <usbiodef.h>
#include <cfgmgr32.h>
#include <tchar.h>


#pragma comment(lib, "setupapi.lib")

BOOL GetDeviceSNByDeviceNum(TCHAR driveLetter, DWORD DeviceNumber, TCHAR* pSN) {
	HDEVINFO deviceInfo = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_DISK,
		nullptr,
		nullptr,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (deviceInfo == INVALID_HANDLE_VALUE) {
		std::cerr << "Failed to get device information set." << std::endl;
		return FALSE;
	}

	SP_DEVICE_INTERFACE_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD index = 0; SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &GUID_DEVINTERFACE_DISK, index, &deviceInfoData); ++index) {
		DWORD requiredLength = 0;
		SetupDiGetDeviceInterfaceDetail(deviceInfo, &deviceInfoData, nullptr, 0, &requiredLength, nullptr);

		auto detailDataBuffer = std::vector<BYTE>(requiredLength);
		auto deviceDetailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detailDataBuffer.data());
		deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (!SetupDiGetDeviceInterfaceDetail(deviceInfo, &deviceInfoData, deviceDetailData, requiredLength, nullptr, &devInfoData)) {
			std::cerr << "Failed to get device interface detail. Error: " << GetLastError() << std::endl;
			continue;
		}

		HANDLE hHCDev = CreateFile(
			deviceDetailData->DevicePath,
			0,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr);

		if (hHCDev == INVALID_HANDLE_VALUE) {
			std::cerr << "Failed to open device handle for " << deviceDetailData->DevicePath << ". Error: " << GetLastError() << std::endl;
			continue;
		}

		STORAGE_DEVICE_NUMBER sdn = { 0 };
		DWORD bytesReturned = 0;
		BOOL bDevIo = DeviceIoControl(
			hHCDev,
			IOCTL_STORAGE_GET_DEVICE_NUMBER,
			nullptr,
			0,
			&sdn,
			sizeof(sdn),
			&bytesReturned,
			nullptr);
		CloseHandle(hHCDev);
		if (!bDevIo || sdn.DeviceNumber != DeviceNumber)
		{
			continue;
		}
	
		TCHAR parentID[MAX_DEVICE_ID_LEN];
		DEVINST parentDevInst; // 父设备实例
		CONFIGRET cr = CM_Get_Parent(&parentDevInst, devInfoData.DevInst, 0);
		if (cr == CR_SUCCESS) 
		{
			cr = CM_Get_Device_ID(parentDevInst, parentID, MAX_DEVICE_ID_LEN, 0);
			if (cr == CR_SUCCESS) 
			{
				// 找到最后一个 '\\' 的位置
				const TCHAR* lastBackslash = _tcsrchr(parentID, _T('\\'));

				if (lastBackslash) {
					// 将 '\\' 后的内容拷贝到 pSN 中（使用安全版本）
					_tcsncpy_s(pSN, MAX_DEVICE_ID_LEN, lastBackslash + 1, _TRUNCATE);
				}
				else {
					// 如果没有找到 '\\'，直接复制整个字符串（使用安全版本）
					_tcsncpy_s(pSN, MAX_DEVICE_ID_LEN, parentID, _TRUNCATE);
				}
				SetupDiDestroyDeviceInfoList(deviceInfo);
				return TRUE;
			}

		}
		break;
		
	}

	SetupDiDestroyDeviceInfoList(deviceInfo);
	return FALSE;
}

BOOL GetDeviceNumber(TCHAR driveLetter,DWORD & DeviceNumber) {

		char drivePath[] = "\\\\.\\C:";
		drivePath[4] = (char)driveLetter; // 修改驱动器字母
		BOOL bresult = FALSE;

		HANDLE hDrive = CreateFileA(
			drivePath,
			0,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr);

		if (hDrive != INVALID_HANDLE_VALUE) {
			STORAGE_DEVICE_NUMBER sdn = { 0 };
			DWORD bytesReturned = 0;

			if (DeviceIoControl(
				hDrive,
				IOCTL_STORAGE_GET_DEVICE_NUMBER,
				nullptr,
				0,
				&sdn,
				sizeof(sdn),
				&bytesReturned,
				nullptr)) {
				DeviceNumber = sdn.DeviceNumber;
				bresult = TRUE;
			}
			CloseHandle(hDrive);
		}
		return bresult;
}


BOOL GetVolumeID(TCHAR driveLetter,DWORD & serialNumber) {
	// 确保驱动器名称格式为 "X:\"
	char chLetter= (char)driveLetter;
	std::string rootPath = ":\\";
	 rootPath = chLetter + rootPath;
	return GetVolumeInformationA(
		rootPath.c_str(),    // 驱动器路径
		nullptr,             // 卷名（不需要）
		0,                   // 卷名缓冲区大小
		&serialNumber,       // 序列号输出
		nullptr,             // 最大组件长度（不需要）
		nullptr,             // 文件系统标志（不需要）
		nullptr,             // 文件系统名称（不需要）
		0                    // 文件系统名称缓冲区大小
	);
}


BOOL GetDiskSN(TCHAR driveLetter,DWORD &dwUVN,TCHAR *pUSN/*MAX_DEVICE_ID_LEN*/)
{
	DWORD DeviceNumber = 0;
	memset(pUSN, 0, MAX_DEVICE_ID_LEN * sizeof(TCHAR));

	if (!GetVolumeID(driveLetter, dwUVN))
	{
		return FALSE;
	}
	if (GetDeviceNumber(driveLetter, DeviceNumber))
	{
		GetDeviceSNByDeviceNum(driveLetter, DeviceNumber, pUSN);
	}
	return TRUE;
}

int main(int argc, TCHAR* argv[]) {
	if (argc != 2) {
		_tprintf(L"Usage:  <drive_letter>\n");
		return 1;
	}

	TCHAR driveLetter = argv[1][0]; // 获取命令行参数中的第一个字符作为驱动器字母
	//driveLetter = 'H';
	if ((driveLetter < L'A' || driveLetter > L'Z') && (driveLetter < L'a' || driveLetter > L'z')) {
		_tprintf(L"Invalid drive letter: %c\n", driveLetter);
		return 1;
	}

	TCHAR psn[MAX_DEVICE_ID_LEN] = { 0 }; // 确保 psn 初始化为0
	DWORD dwUVN = 0;
	BOOL b = GetDiskSN(driveLetter, dwUVN, psn);

	if (b) {
		_tprintf(L"%c:\\ --> hi.uvn:%X  -->", driveLetter, dwUVN);
		_tprintf(L"hi.usn: %ws\n", psn);
	}
	else {
		_tprintf(L"Failed to get serial number for drive %c\n", driveLetter);
	}

	return 0;
}