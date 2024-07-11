#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <TinyNetwork/Buffer.h>

int main()
{
    Buffer* output = new Buffer;

    // 响应行
    char buf[32];
    memset(buf, '\0', sizeof(buf));
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", 200);
    output->append(buf, 32);
    output->append("OK", 2);
    output->append("\r\n", 2);

    std::cout << output->GetBufferAllAsString() << std::endl;

    return 0;
}

// g++ test.cc -g -o test

