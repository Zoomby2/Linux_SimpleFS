#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "simplefs.h"

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc > 1) {
        int fd = open("/mnt/simplefs", O_RDONLY);
        if (fd < 0) {
            perror("Не удалось открыть каталог ФС");
            return 1;
        }

        if (strcmp(argv[1], "--zero") == 0) {
            ioctl(fd, SIMPLEFS_IOC_ZERO_FILES);
            printf("Файлы успешно обнулены через IOCTL.\n");
        } else if (strcmp(argv[1], "--erase") == 0) {
            ioctl(fd, SIMPLEFS_IOC_ERASE_FS);
            printf("ФС стерта. Доступ к директории заблокирован ядром.\n");
        } else if (strcmp(argv[1], "--hashes") == 0) {
            __u32 h[10]; 
            if (ioctl(fd, SIMPLEFS_IOC_GET_HASHES, h) < 0) {
                perror("Ошибка при вызове IOCTL_GET_HASHES");
                close(fd);
                return 1;
            }
            printf("----- Список хэшей файлов -----\n");
            for (int j = 0; j < 10; j++) {
                printf("file%d: хэш 0x%X\n", j, h[j]);
            }
        } else if (strcmp(argv[1], "--map") == 0 && argc > 2) {
            struct simplefs_mapping map;
            strncpy(map.filename, argv[2], sizeof(map.filename));
            ioctl(fd, SIMPLEFS_IOC_GET_MAP, &map);
            printf("Файл %s: начальный сектор = %llu, секторов = %llu\n", map.filename, (unsigned long long)map.start_sector, (unsigned long long)map.sector_count);
        }
        close(fd);
        return 0;
    }

    char path[64];
    int i, fd;
    int wr_val, rd_val;

    printf("Запуск автоматического теста чтения/записи для файлов:\n");
    for (i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "/mnt/simplefs/file%d", i);
        fd = open(path, O_RDWR);
        if (fd < 0) {
            printf("/mnt/simplefs/file%d открыть не удалось (возможно, ФС стерта)\n", i);
            continue;
        }

        wr_val = rand() % 1000;
        write(fd, &wr_val, sizeof(wr_val));

        lseek(fd, 0, SEEK_SET);
        if (read(fd, &rd_val, sizeof(rd_val)) > 0 && rd_val == wr_val) {
            printf("file%d: записано '%d', прочитано '%d' -> ОК\n", i, wr_val, rd_val);
        } else {
            printf("file%d: записано '%d', прочитано '%d' -> Провал\n", i, wr_val, rd_val);
        }
        close(fd);
    }

    return 0;
}
