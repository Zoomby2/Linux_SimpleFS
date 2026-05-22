#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "simplefs.h"

MODULE_LICENSE("GPL");

/* Параметры модуля */
static char *d_name = "/dev/loop0";
static int sb1 = 0;
static int sb2 = 1024;
static int n_len = 32;
static int m_sec = 4;

module_param(d_name, charp, 0444);
module_param(sb1, int, 0444);
module_param(sb2, int, 0444);
module_param(n_len, int, 0444);
module_param(m_sec, int, 0444);

static uint32_t kernel_calculate_hash(struct simplefs_super_block *sb) {
    uint32_t sum = 0;
    uint8_t *ptr = (uint8_t *)sb;
    size_t i;
    for (i = 0; i < sizeof(struct simplefs_super_block); i++) {
        if (i >= 8 && i < 12) continue; /* Пропускаем поле hash */
        sum += ptr[i];
    }
    return sum;
}

/* чтобы VFS не ругалась */
static const struct file_operations simplefs_file_ops;
static const struct file_operations simplefs_dir_operations;
static const struct inode_operations simplefs_dir_inode_operations;

static long simplefs_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct super_block *sb = file_inode(file)->i_sb;
    struct buffer_head *bh;
    int i, s;

    switch (cmd) {
    case SIMPLEFS_IOC_ZERO_FILES:
        printk(KERN_INFO "Зануление файлов через IOCTL\n");
        for (i = 0; i < 10; i++) {
            for (s = 0; s < m_sec; s++) {
                int sector = sb2 + 1 + (i * m_sec) + s;
                bh = sb_bread(sb, sector);
                if (bh) {
                    memset(bh->b_data, 0, bh->b_size);
                    set_buffer_dirty(bh);
                    sync_dirty_buffer(bh);
                    brelse(bh);
                }
            }
        }
        return 0;

    case SIMPLEFS_IOC_ERASE_FS:
        printk(KERN_WARNING "Стирание суперблоков!\n");
        bh = sb_bread(sb, sb1);
        if (bh) {
            memset(bh->b_data, 0, bh->b_size);
            set_buffer_dirty(bh);
            sync_dirty_buffer(bh);
            brelse(bh);
        }
        bh = sb_bread(sb, sb2);
        if (bh) {
            memset(bh->b_data, 0, bh->b_size);
            set_buffer_dirty(bh);
            sync_dirty_buffer(bh);
            brelse(bh);
        }
        return 0;

    case SIMPLEFS_IOC_GET_HASHES: {
        __u32 hashes[10];
        printk(KERN_INFO "Сбор хэшей метаинформации файлов\n");
        for (i = 0; i < 10; i++) {
            int sector = sb2 + 1 + (i * m_sec);
            bh = sb_bread(sb, sector);
            if (bh) {
                __u32 sum = 0;
                int k;
                for (k = 0; k < 256; k++) { /* Считаем контрольную сумму куска данных */
                    sum += bh->b_data[k];
                }
                hashes[i] = sum ^ 0x12345678;
                brelse(bh);
            } else {
                hashes[i] = 0;
            }
        }
        if (copy_to_user((void __user *)arg, hashes, sizeof(hashes)))
            return -EFAULT;
        return 0;
    }

    case SIMPLEFS_IOC_GET_MAP: {
        struct simplefs_mapping m;
        int idx = -1;
        if (copy_from_user(&m, (void __user *)arg, sizeof(m)))
            return -EFAULT;

        if (sscanf(m.name, "file%d", &idx) == 1 && idx >= 0 && idx < 10) {
            m.start_sec = sb2 + 1 + (idx * m_sec);
            m.sec_cnt = m_sec;
            printk(KERN_INFO "Маппинг для %s -> сектор %llu\n", m.name, m.start_sec);
        } else {
            m.start_sec = 0;
            m.sec_cnt = 0;
        }

        if (copy_to_user((void __user *)arg, &m, sizeof(m)))
            return -EFAULT;
        return 0;
    }
    }
    return -EINVAL;
}

static ssize_t simplefs_read(struct file *file, char __user *buf, size_t len, loff_t *ppos) {
    struct inode *inode = file_inode(file);
    int idx = inode->i_ino - 2; /* корень=1, файлы от 2 до 11 */
    struct super_block *sb = inode->i_sb;
    size_t bytes_read = 0;
    loff_t max_sz = m_sec * 4096;

    if (idx < 0 || idx >= 10) return -EINVAL;
    if (*ppos >= max_sz) return 0;
    if (*ppos + len > max_sz) len = max_sz - *ppos;

    while (len > 0) {
        int sector = sb2 + 1 + (idx * m_sec) + (*ppos / 4096);
        int offset = *ppos % 4096;
        int chunk = 4096 - offset;
        struct buffer_head *bh;

        if (chunk > len) chunk = len;

        bh = sb_bread(sb, sector);
        if (!bh) return -EIO;

        if (copy_to_user(buf + bytes_read, bh->b_data + offset, chunk)) {
            brelse(bh);
            return -EFAULT;
        }

        brelse(bh);
        *ppos += chunk;
        bytes_read += chunk;
        len -= chunk;
    }
    return bytes_read;
}

static ssize_t simplefs_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos) {
    struct inode *inode = file_inode(file);
    int idx = inode->i_ino - 2;
    struct super_block *sb = inode->i_sb;
    size_t bytes_written = 0;
    loff_t max_sz = m_sec * 4096;

    if (idx < 0 || idx >= 10) return -EINVAL;
    if (*ppos >= max_sz) return -ENOSPC;
    if (*ppos + len > max_sz) len = max_sz - *ppos;

    while (len > 0) {
        int sector = sb2 + 1 + (idx * m_sec) + (*ppos / 4096);
        int offset = *ppos % 4096;
        int chunk = 4096 - offset;
        struct buffer_head *bh;

        if (chunk > len) chunk = len;

        bh = sb_bread(sb, sector);
        if (!bh) return -EIO;

        if (copy_from_user(bh->b_data + offset, buf + bytes_written, chunk)) {
            brelse(bh);
            return -EFAULT;
        }

        set_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);

        *ppos += chunk;
        bytes_written += chunk;
        len -= chunk;
    }
    return bytes_written;
}

static int simplefs_dir_iterate(struct file *file, struct dir_context *ctx) {
    int i;
    if (!dir_emit_dots(file, ctx)) return 0;

    for (i = ctx->pos - 2; i < 10; i++) {
        char name[16];
        snprintf(name, sizeof(name), "file%d", i);
        if (!dir_emit(ctx, name, strlen(name), i + 2, DT_REG))
            break;
        ctx->pos++;
    }
    return 0;
}

static struct dentry *simplefs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    int idx = -1;
    if (sscanf(dentry->d_name.name, "file%d", &idx) == 1 && idx >= 0 && idx < 10) {
        struct inode *inode = new_inode(dir->i_sb);
        if (!inode) return ERR_PTR(-ENOMEM);
        inode->i_ino = idx + 2;
        inode->i_mode = S_IFREG | 0666;
        inode->i_fop = &simplefs_file_ops;
        inode->i_size = m_sec * 4096;
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        d_add(dentry, inode);
        return NULL;
    }
    return NULL;
}

static const struct file_operations simplefs_file_ops = {
    .read = simplefs_read,
    .write = simplefs_write,
};

static const struct file_operations simplefs_dir_operations = {
    .iterate_shared = simplefs_dir_iterate,
    .unlocked_ioctl = simplefs_ioctl,
};

static const struct inode_operations simplefs_dir_inode_operations = {
    .lookup = simplefs_lookup,
};

static const struct super_operations simplefs_s_ops = {
    .drop_inode = generic_delete_inode,
};

static int simplefs_fill_super(struct super_block *sb, void *data, int silent) {
    struct buffer_head *bh;
    struct simplefs_super_block *disk_sb;
    struct inode *root_inode;
    uint32_t calc_h;

    sb_set_blocksize(sb, 4096);

    /* Читаем первый SB */
    bh = sb_bread(sb, sb1);
    if (!bh) return -EIO;

    disk_sb = (struct simplefs_super_block *)bh->b_data;
    calc_h = kernel_calculate_hash(disk_sb);

    if (le32_to_cpu(disk_sb->magic) != SIMPLEFS_MAGIC || le32_to_cpu(disk_sb->hash) != calc_h) {
        brelse(bh);
        printk(KERN_WARNING "Первый SB побился, лезем во второй...\n");

        bh = sb_bread(sb, sb2);
        if (!bh) return -EIO;

        disk_sb = (struct simplefs_super_block *)bh->b_data;
        calc_h = kernel_calculate_hash(disk_sb);
        if (le32_to_cpu(disk_sb->magic) != SIMPLEFS_MAGIC || le32_to_cpu(disk_sb->hash) != calc_h) {
            brelse(bh);
            printk(KERN_ERR "Оба суперблока мертвы!\n");
            return -EINVAL;
        }
        printk(KERN_INFO "Успешно восстановились по резервному SB\n");
    } else {
        printk(KERN_INFO "Успешный старт с основного SB\n");
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

static struct dentry *simplefs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    return mount_bdev(fs_type, flags, dev_name, data, simplefs_fill_super);
}

static void simplefs_kill_sb(struct super_block *sb) {
    kill_block_super(sb);
}

static struct file_system_type simplefs_type = {
    .owner = THIS_MODULE,
    .name = "simplefs",
    .mount = simplefs_mount,
    .kill_sb = simplefs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init init_simplefs(void) {
    printk(KERN_INFO "Модуль загружен\n");
    return register_filesystem(&simplefs_type);
}

static void __exit exit_simplefs(void) {
    unregister_filesystem(&simplefs_type);
    printk(KERN_INFO "Модуль выгружен\n");
}

module_init(init_simplefs);
module_exit(exit_simplefs);
