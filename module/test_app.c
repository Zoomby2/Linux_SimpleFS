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

    int fd = open("/mnt/simplefs", O_RDONLY);
    if (fd < 0) {
        perror("Не удалось открыть каталог ФС");
        return 1;
    }

    /* Запрашиваю реальное количество файлов у ядра */
    uint64_t files_count = 0;
    if (ioctl(fd, SIMPLEFS_IOC_GET_FCOUNT, &files_count) < 0) {
        perror("Ошибка при получении количества файлов");
        close(fd);
        return 1;
    }

    if (argc > 1) {
        if (strcmp(argv[1], "--zero") == 0) {
            ioctl(fd, SIMPLEFS_IOC_ZERO_FILES);
            printf("Файлы успешно обнулены.\n");
        } else if (strcmp(argv[1], "--erase") == 0) {
            ioctl(fd, SIMPLEFS_IOC_ERASE_FS);
            printf("ФС стерта. Доступ к директории заблокирован ядром.\n");
        } else if (strcmp(argv[1], "--hashes") == 0) {
            struct simplefs_hash_list hl;
            hl.count = files_count;
            hl.list = malloc(files_count * sizeof(__u32));
            if (!hl.list) {
                perror("Ошибка выделения памяти");
                return 1;
            }

            if (ioctl(fd, SIMPLEFS_IOC_GET_HASHES, &hl) < 0) {
                perror("Ошибка при вызове IOCTL_GET_HASHES");
            } else {
                printf("----- Список хэшей (%llu файлов) -----\n", (unsigned long long)files_count);
                for (int j = 0; j < files_count; j++) {
                    printf("file%d: хэш 0x%X\n", j, hl.list[j]);
                }
            }
            free(hl.list);
        } else if (strcmp(argv[1], "--map") == 0 && argc > 2) {
            struct simplefs_mapping map;
            strncpy(map.filename, argv[2], sizeof(map.filename));
            if (ioctl(fd, SIMPLEFS_IOC_GET_MAP, &map) < 0) {
                perror("Ошибка IOCTL_GET_MAP");
            } else {
                printf("Файл %s: начальный сектор = %llu, секторов = %llu\n", 
                       map.filename, (unsigned long long)map.start_sector, (unsigned long long)map.sector_count);
            }
        }
        close(fd);
        return 0;
    }

    printf("Найдено файлов на диске: %llu. Запуск теста чтения/записи:\n", (unsigned long long)files_count);
    char path[64];
    int i, f;
    int wr_val, rd_val;

    for (i = 0; i < files_count; i++) {
        snprintf(path, sizeof(path), "/mnt/simplefs/file%d", i);
        f = open(path, O_RDWR);
        if (f < 0) {
            printf("file%d открыть не удалось\n", i);
            continue;
        }

        wr_val = rand() % 1000;
        write(f, &wr_val, sizeof(wr_val));

        lseek(f, 0, SEEK_SET);
        if (read(f, &rd_val, sizeof(rd_val)) > 0 && rd_val == wr_val) {
            printf("file%d: записано '%d', прочитано '%d' -> ОК\n", i, wr_val, rd_val);
        } else {
            printf("file%d: записано '%d', прочитано '%d' -> Провал\n", i, wr_val, rd_val);
        }
        close(f);
    }

    close(fd);
    return 0;
}
