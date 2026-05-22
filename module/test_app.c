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
        if (fd < 0) {
            perror("Ошибка открытия точки монтирования");
            return 1;
        }

        if (strcmp(argv[1], "--zero") == 0) {
            if (ioctl(fd, SIMPLEFS_IOC_ZERO_FILES) == 0)
                printf("Файлы успешно обнулены\n");
        } else if (strcmp(argv[1], "--erase") == 0) {
            if (ioctl(fd, SIMPLEFS_IOC_ERASE_FS) == 0)
                printf("Суперблоки уничтожены.\n");
        } else if (strcmp(argv[1], "--hashes") == 0) {
            uint32_t h[10];
            if (ioctl(fd, SIMPLEFS_IOC_GET_HASHES, h) == 0) {
                printf("------ Список хэшей файлов -------\n");
                for (int i = 0; i < 10; i++) {
                    printf("file%d: хэш 0x%08X\n", i, h[i]);
                }
            }
        } else if (strcmp(argv[1], "--map") == 0 && argc > 2) {
            struct simplefs_mapping map;
            strncpy(map.name, argv[2], sizeof(map.name));
            if (ioctl(fd, SIMPLEFS_IOC_GET_MAP, &map) == 0) {
                printf("Файл %s: начальный сектор = %llu, секторов = %llu\n", 
                       map.name, (unsigned long long)map.start_sec, (unsigned long long)map.sec_cnt);
            }
        }
        close(fd);
        return 0;
    }

    /* Прогон без параметров — запись случайного числа и чтение */
    printf("Запуск теста для файлов:\n");
    for (int i = 0; i < 10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/mnt/simplefs/file%d", i);

        int fd = open(path, O_RDWR);
        if (fd < 0) {
            printf("%s открыть не удалось\n", path);
            continue;
        }

        int wr_val = rand() % 99999;
        if (write(fd, &wr_val, sizeof(wr_val)) != sizeof(wr_val)) {
            printf("Ошибка записи в %s\n", path);
            close(fd);
            continue;
        }

        lseek(fd, 0, SEEK_SET);
        int rd_val = 0;
        if (read(fd, &rd_val, sizeof(rd_val)) != sizeof(rd_val)) {
            printf("Ошибка чтения из %s\n", path);
            close(fd);
            continue;
        }

        printf("file%d: записано %d, прочитано %d -> %s\n", 
               i, wr_val, rd_val, (wr_val == rd_val) ? "ОК" : "Провал");
        close(fd);
    }

    return 0;
}
