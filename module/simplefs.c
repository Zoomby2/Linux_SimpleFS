#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "simplefs.h"

MODULE_LICENSE("GPL");

static char *disk_name = "/dev/loop0";
static int sb_first_offset = 0;
static int sb_second_offset = 1024;
static int max_name_len = 32;
static int max_file_sectors = 4;

module_param(disk_name, charp, 0444);
module_param(sb_first_offset, int, 0444);
module_param(sb_second_offset, int, 0444);
module_param(max_name_len, int, 0444);
module_param(max_file_sectors, int, 0444);

static uint32_t kernel_calculate_hash(struct simplefs_super_block *sb) {
    uint32_t sum = 0;
    uint8_t *ptr = (uint8_t *)sb;
    size_t i;
    for (i = 0; i < sizeof(struct simplefs_super_block); i++) {
        if (i >= 8 && i < 12) continue;
        sum += ptr[i];
    }
    return sum;
}

static long simplefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct super_block *sb = file_inode(file)->i_sb;
    struct buffer_head *bh;
    int i, j;

    switch (cmd) {
        case SIMPLEFS_IOC_ZERO_FILES:
            for (i = 0; i < 10; i++) {
                for (j = 0; j < max_file_sectors; j++) {
                    bh = sb_bread(sb, sb_second_offset + 1 + (i * max_file_sectors) + j);
                    if (bh) {
                        memset(bh->b_data, 0, sb->s_blocksize);
                        mark_buffer_dirty(bh);
                        sync_dirty_buffer(bh);
                        brelse(bh);
                    }
                }
            }
            return 0;

        case SIMPLEFS_IOC_ERASE_FS:
            bh = sb_bread(sb, sb_first_offset);
            if (bh) {
                memset(bh->b_data, 0, sb->s_blocksize);
                mark_buffer_dirty(bh);
                sync_dirty_buffer(bh);
                brelse(bh);
            }
            bh = sb_bread(sb, sb_second_offset);
            if (bh) {
                memset(bh->b_data, 0, sb->s_blocksize);
                mark_buffer_dirty(bh);
                sync_dirty_buffer(bh);
                brelse(bh);
            }
            return 0;

        case SIMPLEFS_IOC_GET_MAP: {
            struct simplefs_mapping map;
            int idx;
            if (copy_from_user(&map, (void __user *)arg, sizeof(map))) return -EFAULT;
            if (sscanf(map.filename, "file%d", &idx) != 1) return -EINVAL;
            if (idx < 0 || idx >= 10) return -EINVAL;
            map.start_sector = sb_second_offset + 1 + (idx * max_file_sectors);
            map.sector_count = max_file_sectors;
            if (copy_to_user((void __user *)arg, &map, sizeof(map))) return -EFAULT;
            return 0;
        }

        case SIMPLEFS_IOC_GET_HASHES: {
            __u32 dummy_hash = 0xABCDEF12;
            if (copy_to_user((void __user *)arg, &dummy_hash, sizeof(dummy_hash))) return -EFAULT;
            return 0;
        }
    }
    return -ENOTTY;
}

static ssize_t simplefs_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh;
    int file_idx = inode->i_ino - 2;
    long long block = sb_second_offset + 1 + (file_idx * max_file_sectors) + (*ppos / sb->s_blocksize);
    int offset = *ppos % sb->s_blocksize;
    size_t bytes;

    if (*ppos >= inode->i_size) return 0;
    if (*ppos + len > inode->i_size) len = inode->i_size - *ppos;

    bytes = min((size_t)(sb->s_blocksize - offset), len);
    bh = sb_bread(sb, block);
    if (!bh) return -EIO;

    if (copy_to_user(buf, bh->b_data + offset, bytes)) {
        brelse(bh);
        return -EFAULT;
    }

    brelse(bh);
    *ppos += bytes;
    return bytes;
}

static ssize_t simplefs_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh;
    int file_idx = inode->i_ino - 2;
    long long block = sb_second_offset + 1 + (file_idx * max_file_sectors) + (*ppos / sb->s_blocksize);
    int offset = *ppos % sb->s_blocksize;
    size_t bytes;

    if (*ppos >= inode->i_size) return -ENOSPC;
    if (*ppos + len > inode->i_size) len = inode->i_size - *ppos;

    bytes = min((size_t)(sb->s_blocksize - offset), len);
    bh = sb_bread(sb, block);
    if (!bh) return -EIO;

    if (copy_from_user(bh->b_data + offset, buf, bytes)) {
        brelse(bh);
        return -EFAULT;
    }

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    *ppos += bytes;
    return bytes;
}

const struct file_operations simplefs_file_operations = {
    .read       = simplefs_read,
    .write      = simplefs_write,
    .unlocked_ioctl = simplefs_ioctl,
    .llseek     = generic_file_llseek,
};

static int simplefs_iterate(struct file *file, struct dir_context *ctx)
{
    char name[32];
    int i;

    if (ctx->pos >= 12) return 0;
    if (ctx->pos == 0) {
        if (!dir_emit_dot(file, ctx)) return 0;
        ctx->pos++;
    }
    if (ctx->pos == 1) {
        if (!dir_emit_dotdot(file, ctx)) return 0;
        ctx->pos++;
    }

    for (i = ctx->pos - 2; i < 10; i++) {
        snprintf(name, sizeof(name), "file%d", i);
        if (!dir_emit(ctx, name, strlen(name), i + 2, DT_REG)) return 0;
        ctx->pos++;
    }
    return 0;
}

const struct file_operations simplefs_dir_operations = {
    .open           = dcache_dir_open,
    .release        = dcache_dir_close,
    .llseek         = dcache_dir_lseek,
    .read           = generic_read_dir,
    .iterate_shared = simplefs_iterate,
    .unlocked_ioctl = simplefs_ioctl,
};

static struct dentry *simplefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    struct inode *inode;
    int idx;

    if (sscanf(dentry->d_name.name, "file%d", &idx) != 1) return NULL;
    if (idx < 0 || idx >= 10) return NULL;

    inode = new_inode(dir->i_sb);
    if (!inode) return ERR_PTR(-ENOMEM);

    inode->i_ino = idx + 2;
    inode->i_mode = S_IFREG | 0666;
    inode->i_fop = &simplefs_file_operations;
    inode->i_size = max_file_sectors * dir->i_sb->s_blocksize;
    insert_inode_hash(inode);
    d_add(dentry, inode);
    return NULL;
}

static const struct inode_operations simplefs_dir_inode_operations = {
    .lookup = simplefs_lookup,
};

static const struct super_operations simplefs_s_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_delete_inode,
};

static int simplefs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;
    struct buffer_head *bh;
    struct simplefs_super_block *disk_sb;
    uint32_t calc_hash;

    sb_set_blocksize(sb, 4096);

    bh = sb_bread(sb, sb_first_offset);
    if (!bh) {
        printk(KERN_ERR "SimpleFS: Failed to read primary SB at block       %d\n", sb_first_offset);
        return -EIO;
    }

    disk_sb = (struct simplefs_super_block *)bh->b_data;
    calc_hash = kernel_calculate_hash(disk_sb);

    if (le32_to_cpu(disk_sb->magic) != SIMPLEFS_MAGIC || le32_to_cpu(disk_sb->hash) != calc_hash) {
        brelse(bh);
        printk(KERN_WARNING "SimpleFS: Primary SB corrupted. Trying backup at block %d...\n", sb_second_offset);
        
        bh = sb_bread(sb, sb_second_offset);
        if (!bh) {
            printk(KERN_ERR "SimpleFS: Failed to read secondary SB at block %d\n", sb_second_offset);
            return -EIO;
        }
        
        disk_sb = (struct simplefs_super_block *)bh->b_data;
        calc_hash = kernel_calculate_hash(disk_sb);
        
        if (le32_to_cpu(disk_sb->magic) != SIMPLEFS_MAGIC || le32_to_cpu(disk_sb->hash) != calc_hash) {
            brelse(bh);
            printk(KERN_ERR "SimpleFS: Both SBs corrupted or invalid!\n");
            return -EINVAL;
        }
        printk(KERN_INFO "SimpleFS: Recovered using secondary SB.\n");
    } else {
        printk(KERN_INFO "SimpleFS: Mounted using primary SB.\n");
    }

    sb->s_magic = SIMPLEFS_MAGIC;
    sb->s_op = &simplefs_s_ops;

    root_inode = new_inode(sb);
    if (!root_inode) {
        brelse(bh);
        return -ENOMEM;
    }

    root_inode->i_ino = 1;
    root_inode->i_mode = S_IFDIR | 0755;
    root_inode->i_fop = &simplefs_dir_operations;
    root_inode->i_op = &simplefs_dir_inode_operations;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        brelse(bh);
        return -ENOMEM;
    }

    brelse(bh);
    return 0;
}

static struct dentry *simplefs_mount(struct file_system_type *fs_type,
                                     int flags, const char *dev_name, void *data)
{
    return mount_bdev(fs_type, flags, dev_name, data, simplefs_fill_super);
}

static void simplefs_kill_sb(struct super_block *sb)
{
    kill_block_super(sb);
}

static struct file_system_type simplefs_type = {
    .owner   = THIS_MODULE,
    .name    = "simplefs",
    .mount   = simplefs_mount,
    .kill_sb = simplefs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init simplefs_init(void)
{
    return register_filesystem(&simplefs_type);
}

static void __exit simplefs_exit(void)
{
    unregister_filesystem(&simplefs_type);
}

module_init(simplefs_init);
module_exit(simplefs_exit);
