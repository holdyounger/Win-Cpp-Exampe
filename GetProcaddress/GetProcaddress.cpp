#include <iostream>
#include <Windows.h>

using namespace std;
void GetKernel32Proc()
{

    typedef BOOL WINAPI func_CreateProcessW(
        LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
        LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);

    HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");

    if (kernel32)
    {
        func_CreateProcessW* fCreateProcessW =
            (func_CreateProcessW*)GetProcAddress(kernel32, "CreateProcessW");

        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        WCHAR szCommandLine[] = L"calc";
        if (fCreateProcessW)
        {
            fCreateProcessW(0, szCommandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        }
    }

}


int main()
{
    std::cout << "Hello World!\n";

    GetKernel32Proc();

    return 0;
}