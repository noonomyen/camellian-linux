#include <linux/version.h>
#include <linux/string.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/namei.h>
#include <linux/list.h>
#include <linux/init_task.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/fdtable.h>
#include <linux/mnt_namespace.h>
#include <linux/statfs.h>
#include "internal.h"
#include "mount.h"
#include <linux/susfs.h>
#include "../drivers/kernelsu/core_hook.h"
#ifdef CONFIG_KSU_SUSFS_SUS_SU
#include <linux/sus_su.h>
#endif


spinlock_t susfs_spin_lock;

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
bool is_log_enable __read_mostly = true;
#define SUSFS_LOGI(fmt, ...) if (is_log_enable) pr_info("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#define SUSFS_LOGE(fmt, ...) if (is_log_enable) pr_err("susfs:[%u][%d][%s]" fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#else
#define SUSFS_LOGI(fmt, ...) 
#define SUSFS_LOGE(fmt, ...) 
#endif

/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
DEFINE_HASHTABLE(SUS_PATH_HLIST, 10);
static void susfs_update_sus_path_inode(char *target_pathname) {
	struct path p;
	struct inode *inode = NULL;
	int err = 0;

	err = kern_path(target_pathname, LOOKUP_FOLLOW, &p);
	if (err) {
		SUSFS_LOGE("Failed opening file '%s'\n", target_pathname);
		return;
	}

	inode = d_inode(p.dentry);
	if (!inode) {
		path_put(&p);
		SUSFS_LOGE("inode is NULL\n");
		return;
	}

	spin_lock(&inode->i_lock);
	inode->i_state |= INODE_STATE_SUS_PATH;
	spin_unlock(&inode->i_lock);

	path_put(&p);
}

int susfs_add_sus_path(struct st_susfs_sus_path* __user user_info) {
	struct st_susfs_sus_path info;
	struct st_susfs_sus_path_hlist *new_entry, *tmp_entry;
	struct hlist_node *tmp_node;
	int bkt;
	bool update_hlist = false;

	if (copy_from_user(&info, user_info, sizeof(info))) {
		SUSFS_LOGE("failed copying from userspace\n");
		return 1;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';

	spin_lock(&susfs_spin_lock);
	hash_for_each_safe(SUS_PATH_HLIST, bkt, tmp_node, tmp_entry, node) {
        if (!strcmp(tmp_entry->target_pathname, info.target_pathname)) {
			hash_del(&tmp_entry->node);
			kfree(tmp_entry);
			update_hlist = true;
			break;
		}
    }
	spin_unlock(&susfs_spin_lock);

	new_entry = kzalloc(sizeof(struct st_susfs_sus_path_hlist), GFP_KERNEL);
	if (!new_entry) {
		SUSFS_LOGE("no enough memory\n");
		return 1;
	}

	new_entry->target_ino = info.target_ino;
	strscpy(new_entry->target_pathname, info.target_pathname, sizeof(new_entry->target_pathname));
	susfs_update_sus_path_inode(new_entry->target_pathname);
	spin_lock(&susfs_spin_lock);
	hash_add(SUS_PATH_HLIST, &new_entry->node, info.target_ino);
	if (update_hlist) {
		SUSFS_LOGI("target_ino: '%lu', target_pathname: '%s' is successfully updated to SUS_PATH_HLIST\n",
				new_entry->target_ino, new_entry->target_pathname);	
	} else {
		SUSFS_LOGI("target_ino: '%lu', target_pathname: '%s' is successfully added to SUS_PATH_HLIST\n",
				new_entry->target_ino, new_entry->target_pathname);
	}
	spin_unlock(&susfs_spin_lock);
	return 0;
}

int susfs_sus_ino_for_filldir64(unsigned long ino) {
	struct st_susfs_sus_path_hlist *entry;
	int found = 0;

	spin_lock(&susfs_spin_lock);
	hash_for_each_possible(SUS_PATH_HLIST, entry, node, ino) {
		if (entry->target_ino == ino) {
			found = 1;
			break;
		}
	}
	spin_unlock(&susfs_spin_lock);

	return found;
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_PATH

/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
LIST_HEAD(LH_SUS_MOUNT);
static void susfs_update_sus_mount_inode(char *target_pathname) {
	struct path p;
	struct inode *inode = NULL;
	int err = 0;

	err = kern_path(target_pathname, LOOKUP_FOLLOW, &p);
	if (err) {
		SUSFS_LOGE("Failed opening file '%s'\n", target_pathname);
		return;
	}

	inode = d_inode(p.dentry);
	if (!inode) {
		path_put(&p);
		SUSFS_LOGE("inode is NULL\n");
		return;
	}

	spin_lock(&inode->i_lock);
	inode->i_state |= INODE_STATE_SUS_MOUNT;
	spin_unlock(&inode->i_lock);

	path_put(&p);
}

int susfs_add_sus_mount(struct st_susfs_sus_mount* __user user_info) {
	struct st_susfs_sus_mount_list *cursor = NULL, *temp = NULL;
	struct st_susfs_sus_mount_list *new_list = NULL;
	struct st_susfs_sus_mount info;
	char pathname[SUSFS_MAX_LEN_PATHNAME];
	bool found = false;

	if (copy_from_user(&info, user_info, sizeof(info))) {
		SUSFS_LOGE("failed copying from userspace\n");
		return 1;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';

#if defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64)
#ifdef CONFIG_MIPS
	info.target_dev = new_decode_dev(info.target_dev);
#else
	info.target_dev = huge_decode_dev(info.target_dev);
#endif /* CONFIG_MIPS */
#else
	info.target_dev = old_decode_dev(info.target_dev);
#endif /* defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64) */

	spin_lock(&susfs_spin_lock);
	list_for_each_entry_safe(cursor, temp, &LH_SUS_MOUNT, list) {
		if (unlikely(!strcmp(cursor->info.target_pathname, info.target_pathname))) {
			memcpy(&cursor->info, &info, sizeof(info));
			strscpy(pathname, cursor->info.target_pathname, sizeof(pathname));
			found = true;
			break;
		}
	}
	spin_unlock(&susfs_spin_lock);

	if (found) {
		susfs_update_sus_mount_inode(pathname);
		SUSFS_LOGI("target_pathname: '%s', target_dev: '%lu', is successfully updated to LH_SUS_MOUNT\n",
					pathname, info.target_dev);
		return 0;
	}

	new_list = kmalloc(sizeof(struct st_susfs_sus_mount_list), GFP_KERNEL);
	if (!new_list) {
		SUSFS_LOGE("no enough memory\n");
		return 1;
	}

	memcpy(&new_list->info, &info, sizeof(info));
	susfs_update_sus_mount_inode(new_list->info.target_pathname);

	INIT_LIST_HEAD(&new_list->list);
	spin_lock(&susfs_spin_lock);
	list_add_tail(&new_list->list, &LH_SUS_MOUNT);
	SUSFS_LOGI("target_pathname: '%s', target_dev: '%lu', is successfully added to LH_SUS_MOUNT\n",
				new_list->info.target_pathname, new_list->info.target_dev);
	spin_unlock(&susfs_spin_lock);
	return 0;
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT

/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
LIST_HEAD(LH_SUS_KSTAT_SPOOFER);
static void susfs_update_sus_kstat_inode(struct st_susfs_sus_kstat* info) {
	struct path p;
	struct inode *inode = NULL;
	int err = 0;

	err = kern_path(info->target_pathname, LOOKUP_FOLLOW, &p);
	if (err) {
		SUSFS_LOGE("Failed opening file '%s'\n", info->target_pathname);
		return;
	}

	inode = d_inode(p.dentry);
	if (!inode) {
		path_put(&p);
		SUSFS_LOGE("inode is NULL\n");
		return;
	}

	spin_lock(&inode->i_lock);
	inode->android_kabi_reserved1 = info->spoofed_ino;
	inode->android_kabi_reserved2 = info->spoofed_dev;
	inode->i_mode = inode->i_mode;
	inode->android_kabi_reserved3 = info->spoofed_nlink;
	inode->i_uid = inode->i_uid;
	inode->i_gid = inode->i_gid;
	inode->i_rdev = inode->i_rdev;
	inode->android_kabi_reserved4 = info->spoofed_size;
	// time will not be changed no matter what
	inode->i_atime.tv_sec = info->spoofed_atime_tv_sec;
	inode->i_atime.tv_nsec = info->spoofed_atime_tv_nsec;
	inode->i_mtime.tv_sec = info->spoofed_mtime_tv_sec;
	inode->i_mtime.tv_nsec = info->spoofed_mtime_tv_nsec;
	inode->i_ctime.tv_sec = info->spoofed_ctime_tv_sec;
	inode->i_ctime.tv_nsec = info->spoofed_ctime_tv_nsec;
	//inode->i_blksize = info->spoofed_blksize;
	inode->i_blkbits = inode->i_blkbits;
	inode->android_kabi_reserved5 = info->spoofed_blocks;
	inode->i_state |= INODE_STATE_SUS_KSTAT;
	spin_unlock(&inode->i_lock);

	path_put(&p);
}

int susfs_add_sus_kstat(struct st_susfs_sus_kstat* __user user_info) {
	struct st_susfs_sus_kstat_list *cursor = NULL, *temp = NULL;
	struct st_susfs_sus_kstat_list *new_list = NULL;
	struct st_susfs_sus_kstat info;
	struct st_susfs_sus_kstat info_copy;
	bool found = false;

	if (copy_from_user(&info, user_info, sizeof(info))) {
		SUSFS_LOGE("failed copying from userspace\n");
		return 1;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';

#if defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64)
#ifdef CONFIG_MIPS
	info.spoofed_dev = new_decode_dev(info.spoofed_dev);
#else
	info.spoofed_dev = huge_decode_dev(info.spoofed_dev);
#endif /* CONFIG_MIPS */
#else
	info.spoofed_dev = old_decode_dev(info.spoofed_dev);
#endif /* defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64) */

	spin_lock(&susfs_spin_lock);
	list_for_each_entry_safe(cursor, temp, &LH_SUS_KSTAT_SPOOFER, list) {
		if (unlikely(!strcmp(cursor->info.target_pathname, info.target_pathname))) {
			memcpy(&cursor->info, &info, sizeof(info));
			memcpy(&info_copy, &cursor->info, sizeof(info_copy));
			found = true;
			break;
		}
	}
	spin_unlock(&susfs_spin_lock);

	if (found) {
		susfs_update_sus_kstat_inode(&info_copy);
		SUSFS_LOGI("is_statically: '%d', target_ino: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%lld', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', is successfully updated to LH_SUS_KSTAT_SPOOFER\n",
			info_copy.is_statically, info_copy.target_ino, info_copy.target_pathname,
			info_copy.spoofed_ino, info_copy.spoofed_dev,
			info_copy.spoofed_nlink, info_copy.spoofed_size,
			info_copy.spoofed_atime_tv_sec, info_copy.spoofed_mtime_tv_sec, info_copy.spoofed_ctime_tv_sec,
			info_copy.spoofed_atime_tv_nsec, info_copy.spoofed_mtime_tv_nsec, info_copy.spoofed_ctime_tv_nsec,
			info_copy.spoofed_blksize, info_copy.spoofed_blocks);
		return 0;
	}

	new_list = kmalloc(sizeof(struct st_susfs_sus_kstat_list), GFP_KERNEL);
	if (!new_list) {
		SUSFS_LOGE("no enough memory\n");
		return 1;
	}

	memcpy(&new_list->info, &info, sizeof(info));
	if (new_list->info.target_pathname[0] != '\0') {
		susfs_update_sus_kstat_inode(&new_list->info);
	}

	INIT_LIST_HEAD(&new_list->list);
	spin_lock(&susfs_spin_lock);
	list_add_tail(&new_list->list, &LH_SUS_KSTAT_SPOOFER);
	SUSFS_LOGI("is_statically: '%d', target_ino: '%lu', target_pathname: '%s', spoofed_ino: '%lu', spoofed_dev: '%lu', spoofed_nlink: '%u', spoofed_size: '%lld', spoofed_atime_tv_sec: '%ld', spoofed_mtime_tv_sec: '%ld', spoofed_ctime_tv_sec: '%ld', spoofed_atime_tv_nsec: '%ld', spoofed_mtime_tv_nsec: '%ld', spoofed_ctime_tv_nsec: '%ld', spoofed_blksize: '%lu', spoofed_blocks: '%llu', is successfully added to LH_SUS_KSTAT_SPOOFER\n",
		new_list->info.is_statically, new_list->info.target_ino, new_list->info.target_pathname,
		new_list->info.spoofed_ino, new_list->info.spoofed_dev,
		new_list->info.spoofed_nlink, new_list->info.spoofed_size,
		new_list->info.spoofed_atime_tv_sec, new_list->info.spoofed_mtime_tv_sec, new_list->info.spoofed_ctime_tv_sec,
		new_list->info.spoofed_atime_tv_nsec, new_list->info.spoofed_mtime_tv_nsec, new_list->info.spoofed_ctime_tv_nsec,
		new_list->info.spoofed_blksize, new_list->info.spoofed_blocks);
	spin_unlock(&susfs_spin_lock);
	return 0;
}

int susfs_update_sus_kstat(struct st_susfs_sus_kstat* __user user_info) {
	struct st_susfs_sus_kstat_list *cursor = NULL, *temp = NULL;
	struct st_susfs_sus_kstat info;
	struct st_susfs_sus_kstat info_copy;
	bool found = false;

	if (copy_from_user(&info, user_info, sizeof(info))) {
		SUSFS_LOGE("failed copying from userspace\n");
		return 1;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';

	spin_lock(&susfs_spin_lock);
	list_for_each_entry_safe(cursor, temp, &LH_SUS_KSTAT_SPOOFER, list) {
		if (unlikely(!strcmp(info.target_pathname, cursor->info.target_pathname))) {
			SUSFS_LOGI("updating target_ino from '%lu' to '%lu' for pathname: '%s' in LH_SUS_KSTAT_SPOOFER\n", cursor->info.target_ino, info.target_ino, info.target_pathname);
			cursor->info.target_ino = info.target_ino;
			if (info.spoofed_size > 0) {
				cursor->info.spoofed_size = info.spoofed_size;
			}
			if (info.spoofed_blocks > 0) {
				cursor->info.spoofed_blocks = info.spoofed_blocks;
			}
			memcpy(&info_copy, &cursor->info, sizeof(info_copy));
			found = true;
			break;
		}
	}
	spin_unlock(&susfs_spin_lock);

	if (found) {
		susfs_update_sus_kstat_inode(&info_copy);
		return 0;
	}

	SUSFS_LOGE("target_pathname: '%s' is not found in LH_SUS_KSTAT_SPOOFER\n", info.target_pathname);
	return 1;
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_KSTAT

/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
LIST_HEAD(LH_TRY_UMOUNT_PATH);
int susfs_add_try_umount(struct st_susfs_try_umount* __user user_info) {
	struct st_susfs_try_umount_list *cursor = NULL, *temp = NULL;
	struct st_susfs_try_umount_list *new_list = NULL;
	struct st_susfs_try_umount info;

	if (copy_from_user(&info, user_info, sizeof(info))) {
		SUSFS_LOGE("failed copying from userspace\n");
		return 1;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';

	list_for_each_entry_safe(cursor, temp, &LH_TRY_UMOUNT_PATH, list) {
		if (unlikely(!strcmp(info.target_pathname, cursor->info.target_pathname))) {
			SUSFS_LOGE("target_pathname: '%s' is already created in LH_TRY_UMOUNT_PATH\n", info.target_pathname);
			return 1;
		}
	}

	new_list = kmalloc(sizeof(struct st_susfs_try_umount_list), GFP_KERNEL);
	if (!new_list) {
		SUSFS_LOGE("no enough memory\n");
		return 1;
	}

	memcpy(&new_list->info, &info, sizeof(info));

	INIT_LIST_HEAD(&new_list->list);
	spin_lock(&susfs_spin_lock);
	list_add_tail(&new_list->list, &LH_TRY_UMOUNT_PATH);
	spin_unlock(&susfs_spin_lock);
	SUSFS_LOGI("target_pathname: '%s', mnt_mode: %d, is successfully added to LH_TRY_UMOUNT_PATH\n", new_list->info.target_pathname, new_list->info.mnt_mode);
	return 0;
}

void susfs_try_umount(uid_t target_uid) {
	struct st_susfs_try_umount_list *cursor = NULL, *temp = NULL;

	list_for_each_entry_safe(cursor, temp, &LH_TRY_UMOUNT_PATH, list) {
		SUSFS_LOGI("umounting '%s' for uid: %d\n", cursor->info.target_pathname, target_uid);
		if (cursor->info.mnt_mode == 0) {
			try_umount(cursor->info.target_pathname, false, 0);
		} else if (cursor->info.mnt_mode == 1) {
			try_umount(cursor->info.target_pathname, false, MNT_DETACH);
		}
	}
}
#endif // #ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT

/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
spinlock_t susfs_uname_spin_lock;
struct st_susfs_uname my_uname;
static void susfs_my_uname_init(void) {
	memset(&my_uname, 0, sizeof(my_uname));
}

int susfs_set_uname(struct st_susfs_uname* __user user_info) {
	struct st_susfs_uname info;

	if (copy_from_user(&info, user_info, sizeof(struct st_susfs_uname))) {
		SUSFS_LOGE("failed copying from userspace.\n");
		return 1;
	}
	info.release[sizeof(info.release) - 1] = '\0';
	info.version[sizeof(info.version) - 1] = '\0';

	spin_lock(&susfs_uname_spin_lock);
	if (!strcmp(info.release, "default")) {
		strscpy(my_uname.release, utsname()->release, sizeof(my_uname.release));
	} else {
		strscpy(my_uname.release, info.release, sizeof(my_uname.release));
	}
	if (!strcmp(info.version, "default")) {
		strscpy(my_uname.version, utsname()->version, sizeof(my_uname.version));
	} else {
		strscpy(my_uname.version, info.version, sizeof(my_uname.version));
	}
	spin_unlock(&susfs_uname_spin_lock);
	SUSFS_LOGI("setting spoofed release: '%s', version: '%s'\n",
				my_uname.release, my_uname.version);
	return 0;
}

int susfs_spoof_uname(struct new_utsname* tmp) {
	spin_lock(&susfs_uname_spin_lock);
	if (unlikely(my_uname.release[0] == '\0')) {
		spin_unlock(&susfs_uname_spin_lock);
		return 1;
	}
	strscpy(tmp->sysname, utsname()->sysname, sizeof(tmp->sysname));
	strscpy(tmp->nodename, utsname()->nodename, sizeof(tmp->nodename));
	strscpy(tmp->release, my_uname.release, sizeof(tmp->release));
	strscpy(tmp->version, my_uname.version, sizeof(tmp->version));
	strscpy(tmp->machine, utsname()->machine, sizeof(tmp->machine));
	strscpy(tmp->domainname, utsname()->domainname, sizeof(tmp->domainname));
	spin_unlock(&susfs_uname_spin_lock);
	return 0;
}
#endif // #ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME

/* set_log */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_set_log(bool enabled) {
	spin_lock(&susfs_spin_lock);
	is_log_enable = enabled;
	spin_unlock(&susfs_spin_lock);
	if (is_log_enable) {
		pr_info("susfs: enable logging to kernel");
	} else {
		pr_info("susfs: disable logging to kernel");
	}
}
#endif // #ifdef CONFIG_KSU_SUSFS_ENABLE_LOG

/* sus_su */
#ifdef CONFIG_KSU_SUSFS_SUS_SU
int susfs_sus_su(struct st_sus_su* __user user_info) {
	struct st_sus_su info;

	if (copy_from_user(&info, user_info, sizeof(struct st_sus_su))) {
		SUSFS_LOGE("failed copying from userspace\n");
		return 1;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';

	if (info.enabled) {
		sus_su_fifo_init(&info.maj_dev_num, info.drv_path);
		ksu_susfs_enable_sus_su();
		if (info.maj_dev_num < 0) {
			SUSFS_LOGI("failed to get proper info.maj_dev_num: %d\n", info.maj_dev_num);
			return 1;
		}
		SUSFS_LOGI("core kprobe hooks for ksu are disabled!\n");
		SUSFS_LOGI("sus_su driver '%s' is enabled!\n", info.drv_path);
		if (copy_to_user(user_info, &info, sizeof(info)))
			SUSFS_LOGE("copy_to_user() failed\n");
		return 0;
	} else {
		ksu_susfs_disable_sus_su();
		sus_su_fifo_exit(&info.maj_dev_num, info.drv_path);
		if (info.maj_dev_num != -1) {
			SUSFS_LOGI("failed to set proper info.maj_dev_num to '-1'\n");
			return 1;
		}
		SUSFS_LOGI("core kprobe hooks for ksu are enabled! sus_su driver '%s' is disabled!\n", info.drv_path);
		if (copy_to_user(user_info, &info, sizeof(info)))
			SUSFS_LOGE("copy_to_user() failed\n");
		return 0;
	}
	return 1;
}
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_SU

/* susfs_init */
void susfs_init(void) {
	spin_lock_init(&susfs_spin_lock);
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	spin_lock_init(&susfs_uname_spin_lock);
	susfs_my_uname_init();
#endif
	SUSFS_LOGI("susfs is initialized!\n");
}

/* No module exit is needed becuase it should never be a loadable kernel module */
//void __init susfs_exit(void)

