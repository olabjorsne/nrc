#include <Windows.h>
#include <stdio.h>

#define MAIN_BUF_SIZE    (1024 * 10)

extern int nrc_main(const char *boot_cfg, unsigned int boot_cfg_size);

static char         _buf[MAIN_BUF_SIZE];
static unsigned int _buf_cnt = 0;

static void get_config(char *file_name, const char **config, unsigned int *size)
{
    FILE        *stream;
    errno_t     err;
    char        data;

    *config = NULL;
    *size = 0;

    if (file_name != NULL) {
        err = fopen_s(&stream, file_name, "r");
        if (err == 0) {
            data = 0;
            _buf_cnt = 0;
            do
            {
                data = fgetc(stream);
                if (data != EOF) {
                    _buf[_buf_cnt++] = data;
                }

            } while (data != EOF);

            if (_buf_cnt > 0) {
                *config = _buf;
                *size = _buf_cnt;
            }
        }
        if (stream) {
            err = fclose(stream);
        }
    }
}

int main(int argc, char** argv)
{
    const char      *boot_flow_cfg = NULL;
    unsigned int    boot_flow_cfg_size = 0;

    if (argc > 0) {
        // If file as input, assume it is the boot flow configuration file
        get_config(argv[1], &boot_flow_cfg, &boot_flow_cfg_size);
    }

    // Start nrc with the boot flow (if any)
    nrc_main(boot_flow_cfg, boot_flow_cfg_size);

    while (1) {
        SleepEx(1000, TRUE);
    }

    return 0;
}