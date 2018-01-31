#include <Windows.h>

extern int nrc_main(int argc, char **argv);


int main(int argc, char** argv)
{
    int result = nrc_main(argc, argv);

    while (1) {
        SleepEx(1000, TRUE);
    }

    return result;
}