#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>

#define SIMPLEFS_MAGIC 0x534D504C

struct simplefs_super_block {
    uint32_t magic;
    uint32_t version;
    uint32_t hash;
    uint64_t files_count;
    uint64_t block_size;
    uint64_t max_file_sectors;
};

uint32_t calculate_hash(struct simplefs_super_block *sb) {
    uint32_t sum = 0;
    uint8_t *ptr = (uint8_t *)sb;
    for (size_t i = 0; i < sizeof(struct simplefs_super_block); i++) {
        if (i >= 8 && i < 12) continue;
        sum += ptr[i];
    }
    return sum;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Использование: %s <устройство/образ> <смещение_sb1> <смещение_sb2> <секторов_на_файл>\n", argv[0]);
        printf("Пример: %s /dev/loop0 0 1024 4\n", argv[0]);
        return 1;
    }

    const char *disk_path = argv[1];
    uint32_t sb_first = atoi(argv[2]);
    uint32_t sb_second = atoi(argv[3]);
    uint64_t max_file_sectors = atoi(argv[4]);
    uint64_t block_size = 4096;

    int fd = open(disk_path, O_RDWR);
    if (fd < 0) {
        perror("Ошибка открытия диска");
        return 1;
    }

    /* определение размера диска или файла-образа */
    uint64_t dev_size = 0;
    struct stat st;
    if (fstat(fd, &st) == 0) {
        if (S_ISBLK(st.st_mode)) {
            ioctl(fd, BLKGETSIZE64, &dev_size);
        } else {
            dev_size = st.st_size;
        }
    }

    if (dev_size == 0) {
        printf("Ошибка: не удалось определить размер диска.\n");
        close(fd);
        return 1;
    }

    uint64_t total_blocks = dev_size / block_size;
    uint64_t data_start_block = (sb_first > sb_second ? sb_first : sb_second) + 1;
    
    if (total_blocks <= data_start_block) {
        printf("Ошибка: диск слишком мал для такого расположения суперблоков.\n");
        close(fd);
        return 1;
    }

    /* Выделение всего оставшегося места под файлы */
    uint64_t available_blocks = total_blocks - data_start_block;
    uint64_t files_count = available_blocks / max_file_sectors;

    printf("Форматирование: %s\n", disk_path);
    printf("Всего блоков на диске: %llu\n", (unsigned long long)total_blocks);
    printf("Блок начала данных: %llu\n", (unsigned long long)data_start_block);
    printf("Файлов создано (M=%llu): %llu\n", (unsigned long long)max_file_sectors, (unsigned long long)files_count);

    struct simplefs_super_block sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = SIMPLEFS_MAGIC;
    sb.version = 1;
    sb.files_count = files_count;
    sb.block_size = block_size;
    sb.max_file_sectors = max_file_sectors;
    sb.hash = calculate_hash(&sb);

    char buf[4096];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &sb, sizeof(sb));

    /* Запись основного и резервного суперблоков */
    lseek(fd, (off_t)sb_first * block_size, SEEK_SET);
    write(fd, buf, block_size);

    lseek(fd, (off_t)sb_second * block_size, SEEK_SET);
    write(fd, buf, block_size);

    close(fd);
    return 0;
}
