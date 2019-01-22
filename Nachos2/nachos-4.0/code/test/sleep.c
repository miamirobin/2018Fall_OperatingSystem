#include "syscall.h"

main() {
    int i;
    for(i = 1; i < 4; i++) {
        Sleep(i*1000);
        PrintInt(i);
    }
    return 0;
}
