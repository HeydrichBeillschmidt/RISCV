#include <iostream>
#include "pcpu.h"

int main(int argc, const char* argv[]) {
    const std::string path = argv[1];
    ddl::CPU cpu(path);
    cpu.CC();

    return 0;
}
