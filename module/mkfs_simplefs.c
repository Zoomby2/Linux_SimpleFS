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
    if (argc < 2) return 1;

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) return 1;

    struct simplefs_super_block sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = SIMPLEFS_MAGIC;
    sb.version = 1;
    sb.files_count = 10;
    sb.block_size = 4096;
    sb.max_file_sectors = 4;
    sb.hash = calculate_hash(&sb);

    char buf[4096];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &sb, sizeof(sb));

    lseek(fd, 0, SEEK_SET);
    write(fd, buf, 4096);

    lseek(fd, 1024 * 4096, SEEK_SET);
    write(fd, buf, 4096);

    close(fd);
    return 0;
}
