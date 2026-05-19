#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include "simplefs.h"

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc > 1) {
        int fd = open("/mnt/simplefs", O_RDONLY);
        if (fd < 0) return 1;

        if (strcmp(argv[1], "--zero") == 0) {
            ioctl(fd, SIMPLEFS_IOC_ZERO_FILES);
        } else if (strcmp(argv[1], "--erase") == 0) {
            ioctl(fd, SIMPLEFS_IOC_ERASE_FS);
        } else if (strcmp(argv[1], "--hashes") == 0) {
            __u32 h;
            ioctl(fd, SIMPLEFS_IOC_GET_HASHES, &h);
            printf("Metadata hash: 0x%X\n", h);
        } else if (strcmp(argv[1], "--map") == 0 && argc > 2) {
            struct simplefs_mapping map;
            strncpy(map.filename, argv[2], sizeof(map.filename));
            ioctl(fd, SIMPLEFS_IOC_GET_MAP, &map);
            printf("File %s maps to sector %llu (count: %llu)\n", map.filename, (unsigned long long)map.start_sector, (unsigned long long)map.sector_count);
        }
        close(fd);
        return 0;
    }

    char path[64];
    int i, fd;
    int wr_val, rd_val;

    for (i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "/mnt/simplefs/file%d", i);
        fd = open(path, O_RDWR);
        if (fd < 0) continue;

        wr_val = rand() % 1000;
        write(fd, &wr_val, sizeof(wr_val));

        lseek(fd, 0, SEEK_SET);
        read(fd, &rd_val, sizeof(rd_val));

        printf("Path %s -> Wrote: %d, Read: %d\n", path, wr_val, rd_val);
        close(fd);
    }

    return 0;
}
