#ifndef _SIMPLEFS_H
#define _SIMPLEFS_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SIMPLEFS_MAGIC 0x534D504C

/* Суперблок на диске */
struct simplefs_super_block {
    __le32 magic;
    __le32 version;
    __le32 hash;
    __le64 f_count;   /* кол-во файлов */
    __le64 b_size;    /* размер сектора */
    __le64 max_sec;   /* макс. секторов на файл */
};

/* Маппинг для IOCTL */
struct simplefs_mapping {
    char name[32];
    __u64 start_sec;
    __u64 sec_cnt;
};

#define SIMPLEFS_IOC_MAGIC 'S'
#define SIMPLEFS_IOC_ZERO_FILES _IO(SIMPLEFS_IOC_MAGIC, 1)
#define SIMPLEFS_IOC_ERASE_FS   _IO(SIMPLEFS_IOC_MAGIC, 2)
#define SIMPLEFS_IOC_GET_HASHES _IOR(SIMPLEFS_IOC_MAGIC, 3, __u32[10])
#define SIMPLEFS_IOC_GET_MAP    _IOWR(SIMPLEFS_IOC_MAGIC, 4, struct simplefs_mapping)

#endif
