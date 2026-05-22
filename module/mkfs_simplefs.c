#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#define SIMPLEFS_MAGIC 0x534D504C

struct simplefs_super_block {
    uint32_t magic;
    uint32_t version;
    uint32_t hash;
    uint64_t f_count;
    uint64_t b_size;
    uint64_t max_sec;
};

uint32_t calc_hash(struct simplefs_super_block *sb) {
    uint32_t sum = 0;
    uint8_t *ptr = (uint8_t *)sb;
    for (size_t i = 0; i < sizeof(struct simplefs_super_block); i++) {
        if (i >= 8 && i < 12) continue;
        sum += ptr[i];
    }
    return sum;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Использование: %s <образ_диска>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("Ошибка открытия диска");
        return 1;
    }

    struct simplefs_super_block sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = SIMPLEFS_MAGIC;
    sb.version = 1;
    sb.f_count = 10;
    sb.b_size = 4096;
    sb.max_sec = 4;
    sb.hash = calc_hash(&sb);

    /* Основной суперблок (сектор 0) */
    lseek(fd, 0, SEEK_SET);
    write(fd, &sb, sizeof(sb));

    /* Резервный суперблок (сектор 1024) */
    lseek(fd, 1024 * 4096, SEEK_SET);
    write(fd, &sb, sizeof(sb));

    /* На всякий случай забьем нулями область данных под 10 файлов */
    char *zeros = calloc(4, 4096);
    lseek(fd, (1024 + 1) * 4096, SEEK_SET);
    for (int i = 0; i < 10; i++) {
        write(fd, zeros, 4 * 4096);
    }
    free(zeros);

    close(fd);
    printf("Диск отформатирован успешно.\n");
    return 0;
}
