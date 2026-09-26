#include <snes.h>

#include <loom/port.h>

int main(void)
{
    loom_runtime_main();
    for (;;) {
        WaitForVBlank();
    }
    return 0;
}
