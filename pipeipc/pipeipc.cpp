// pipeipc.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "TAPipeIPC.h"

int main()
{
    std::cout << "Hello World!\n";

    PipeIPC* s = new PipeIPC(USER_SERVER);
    PipeIPC* c = new PipeIPC(USER_CLIENT);

    s->InitPipeIPC();
    c->InitPipeIPC();
}
