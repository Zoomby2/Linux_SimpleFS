#ifndef _SIMPLEFS_H
#define _SIMPLEFS_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SIMPLEFS_MAGIC 0x534D504C

struct simplefs_super_block {
    __le32 magic;
    __le32 version;
    __le32 hash;
    __le64 files_count;
    __le64 block_size;
    __le64 max_file_sectors;
};

struct simplefs_mapping {
    char filename[256];
    __u64 start_sector;
    __u64 sector_count;
};

#define SIMPLEFS_IOC_MAGIC 'S'
#define SIMPLEFS_IOC_ZERO_FILES _IO(SIMPLEFS_IOC_MAGIC, 1)
#define SIMPLEFS_IOC_ERASE_FS   _IO(SIMPLEFS_IOC_MAGIC, 2)
#define SIMPLEFS_IOC_GET_HASHES _IOR(SIMPLEFS_IOC_MAGIC, 3, __u32)
#define SIMPLEFS_IOC_GET_MAP    _IOWR(SIMPLEFS_IOC_MAGIC, 4, struct simplefs_mapping)

#endif
