#include "define.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2def.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <bitset> //输出二进制的头文件

#pragma comment(lib, "iphlpapi.lib")

int maskToDigit(std::string strIp)
{
	int nMaskNum = 0;
	DWORD dwMask = 0;
	uint32_t ip = inet_addr(strIp.c_str());

	if (ip == INADDR_NONE)
	{
		return -1;
	}

	dwMask = (DWORD)ip;

	while (dwMask)
	{
		if (ip & 0x80000000)
		{
			nMaskNum++;
		}
		else
		{
			return nMaskNum;
		}

		dwMask <<= 1;
	}

	return nMaskNum;
}

int maskToDigit(DWORD ip)
{
	int nMaskNum = 0;
	cout << "二进制： " << bitset<sizeof(ip) * 8>(ip) << endl;
	bitset<32> bitBinary = bitset<sizeof(ip) * 8>(ip);
	cout << bitBinary.count();
	while (ip)
	{
		if (ip & 1)
		{	
			nMaskNum++;
		}

		ip >>= 1;
	}

	return nMaskNum;
}

string ipToString(DWORD dwIP)
{
	std::string strDestIp = "";
	std::string strMaskIp = "";

	struct in_addr network;
	network.S_un.S_addr = dwIP;    //为s_addr赋值--网络字节序
	strDestIp = inet_ntoa(network);

	return strDestIp;
}

uint32_t stringToIP(std::string strIP)
{
	int nMaskNum = 0;
	DWORD ip = inet_addr(strIP.c_str());

	if (ip == INADDR_NONE)
	{
		return -1;
	}

	return ip;
}

// 辅助打印函数 
void PrintIPv6(const uint8_t ip[16]) 
{
	for (int i = 0; i < 16; i += 2) {
		printf("%02x%02x:", ip[i], ip[i + 1]);
	}
	printf("\b \n");
}

// 辅助函数: 计算IPv6地址范围
void CalculateIPv6Range(UINT8 firstIp[16], UINT8 lastIp[16], int prefixLen)
{
	// 处理前缀部分 
	int fullBytes = prefixLen / 8;
	int remainingBits = prefixLen % 8;

	// 清零主机部分
	for (int i = fullBytes + 1; i < 16; i++)
	{
		firstIp[i] = 0;
		lastIp[i] = 0xFF;
	}

	// 处理部分字节 
	if (remainingBits > 0)
	{
		uint8_t mask = 0xFF << (8 - remainingBits);
		firstIp[fullBytes] &= mask;
		lastIp[fullBytes] |= ~mask;
	}

	// 对于起始地址，通常不需要+1，因为IPv6没有广播地址概念 
	// 对于结束地址，通常不需要-1 
}

// IPv6范围比较辅助函数 
bool CompareIPv6WithRange(const uint8_t* ip, const uint8_t* start, const uint8_t* end)
{
	// 逐字节比较 
	for (int i = 0; i < 16; ++i)
	{
		if (ip[i] < start[i])
			return false;
		if (ip[i] > end[i])
			return false;
		if (ip[i] > start[i] || ip[i] < end[i])
			break; // 已经确定在范围内 
	}
	return true;
}

void TestWithDetailedOutput() 
{
	std::cout << "\n=== Detailed Test Output ===\n";

	struct IPv6Address
	{
		UINT8 bytes[16];
	};

	// 测试案例1: 常规/64前缀 
	{
		uint8_t ipv6[16] = { 0x20,0x01,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,1 };
		uint8_t ipv6Mid[16] = { 0x20,0x01,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,10 };
		uint8_t ipv6Mid1[16] = { 0x20,0x01,0x0d,0xb8,0,0,0,0,10,0,0,0,0,0,0,10 };
		uint8_t first[16], last[16];

		// 计算IPv6地址范围
		IPv6Address Ipv6Start{ 0 };
		IPv6Address ipv6End{ 0 };
		// 计算起始和结束地址 

		memcpy(Ipv6Start.bytes, ipv6, 16);
		memcpy(ipv6End.bytes, ipv6, 16);

		CalculateIPv6Range(Ipv6Start.bytes, ipv6End.bytes, 64);
		std::cout << "CompareIPv6WithRange ipv6Mid:" << CompareIPv6WithRange(ipv6Mid, Ipv6Start.bytes, ipv6End.bytes) << std::endl;
		std::cout << "CompareIPv6WithRange ipv6Mid1:" << CompareIPv6WithRange(ipv6Mid1, Ipv6Start.bytes, ipv6End.bytes);

		memcpy(first, ipv6, 16);
		CalculateIPv6Range(first, last, 64);


		std::cout << "Case1 /64:\nInput:  ";
		PrintIPv6(ipv6);
		std::cout << "First: ";
		PrintIPv6(first);
		std::cout << "Last:  ";
		PrintIPv6(last);
		std::cout << (memcmp(first, "\x20\x01\x0d\xb8", 4) == 0 ? "PASS" : "FAIL") << "\n\n";
	}

	// 测试案例2: 部分掩码/116 
	{
		uint8_t ip[16] = { 0xfd,0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0,0,0,0,0,0,0,1 };
		uint8_t first[16], last[16];
		memcpy(first, ip, 16);

		CalculateIPv6Range(first, last, 116);

		std::cout << "Case2 /116:\nInput:  ";
		PrintIPv6(ip);
		std::cout << "First: ";
		PrintIPv6(first);
		std::cout << "Last:  ";
		PrintIPv6(last);
		std::cout << (first[14] == 0 && last[14] == 0xFF ? "PASS" : "FAIL") << "\n\n";
	}
}

int Ipv6Convert()
{
	TestWithDetailedOutput();

	return 0;
}

int IpConvert() 
{
	uint32_t ip = stringToIP("10.41.0.0");
	uint32_t ip1 = stringToIP("255.255.255.0");
	uint32_t ip2 = stringToIP("255.255.255.128"); // 192
	uint32_t ip3 = stringToIP("255.255.255.255"); // 4294967295
	uint32_t ip4 = 0xC0A81380; // 4294967295

	string strip4 = ipToString(ip4);
	string strip = ipToString(ip);
	string strip1 = ipToString(ip1);
	string strip2 = ipToString(ip2);
	string strip3 = ipToString(ip3);

	cout << NAMEPRINTFORMAT((ip)) << endl;
	cout << NAMEPRINTFORMAT((ip1)) << endl;
	cout << NAMEPRINTFORMAT((ip2)) << endl;
	cout << NAMEPRINTFORMAT((ip3)) << endl;
	cout << endl;
	cout << NAMEPRINTFORMAT(maskToDigit(ip)) << endl;
	cout << NAMEPRINTFORMAT(maskToDigit(ip1)) << endl;
	cout << NAMEPRINTFORMAT(maskToDigit(ip2)) << endl;
	cout << NAMEPRINTFORMAT(maskToDigit(ip3)) << endl;

	cout << endl;

	cout << NAMEPRINTFORMAT(maskToDigit(strip)) << endl;
	cout << NAMEPRINTFORMAT(maskToDigit(strip1)) << endl;
	cout << NAMEPRINTFORMAT(maskToDigit(strip2)) << endl;
	cout << NAMEPRINTFORMAT(maskToDigit(strip3)) << endl;

	return 0;
}