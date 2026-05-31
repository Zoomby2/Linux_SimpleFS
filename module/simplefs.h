#ifndef _SIMPLEFS_H
#define _SIMPLEFS_H

#include <linux/ioctl.h>
#include <linux/types.h>

#ifndef __user
#define __user
#endif

#define SIMPLEFS_MAGIC 0x534D504C

/* Структура суперблока файловой системы */
struct simplefs_super_block {
    __le32 magic;
    __le32 version;
    __le32 hash;
    __le64 files_count;
    __le64 block_size;
    __le64 max_file_sectors;
};

/* Структура сопоставления файла и дисковых секторов */
struct simplefs_mapping {
    char filename[256];
    __u64 start_sector;
    __u64 sector_count;
};

/* Структура для передачи динамического массива хэшей */
struct simplefs_hash_list {
    __u32 __user *list;
    __u32 count;
};

#define SIMPLEFS_IOC_MAGIC 'S'
#define SIMPLEFS_IOC_ZERO_FILES _IO(SIMPLEFS_IOC_MAGIC, 1)
#define SIMPLEFS_IOC_ERASE_FS   _IO(SIMPLEFS_IOC_MAGIC, 2)
#define SIMPLEFS_IOC_GET_HASHES _IOWR(SIMPLEFS_IOC_MAGIC, 3, struct simplefs_hash_list)
#define SIMPLEFS_IOC_GET_MAP    _IOWR(SIMPLEFS_IOC_MAGIC, 4, struct simplefs_mapping)
#define SIMPLEFS_IOC_GET_FCOUNT _IOR(SIMPLEFS_IOC_MAGIC, 5, __u64)

#endif
