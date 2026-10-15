#include "../win32/WG_BACKEND.h"
int main(int argc, char **argv)
{
    int no_pause = 0;
    return WG_LaunchBackends(argc, argv, &no_pause);
}
