// SPDX-License-Identifier: GPL-2.0-only
/*
 * onyx: placeholder /proc and /sys nodes that ColorOS userspace opens (its init rc chowns or writes
 * them, or a framework service reads them) but no driver on onyx provides: the OnePlus driver is
 * not in this source drop, or it drives hardware onyx does not have (the PMIC watchdog). Each node
 * only holds a short text value; nothing in the kernel acts on it. See docs/STATUS.md.
 */
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>

#define VAL_LEN 64

struct onyx_node {
	const char *dir;	/* proc: dir under /proc, NULL for /proc itself; sysfs: unused */
	const char *name;
	umode_t mode;
	const char *init;	/* what a read returns until a write changes it */
	bool fixed;		/* a "feature on" switch onyx cannot honour: reads keep init */
	char val[VAL_LEN];
	struct kobj_attribute attr;
};

static DEFINE_MUTEX(val_lock);

/* rc: init.oplus.nandswap.rc, init.oplus.rootdir.rc, self_init_theia.rc, init.oplus.debug.rc */
static struct onyx_node proc_nodes[] = {
	{ "nandswap", "fn_enable", 0660, "0", true },
	{ "nandswap", "swap_ctl", 0660, "0", true },
	{ "nandswap", "life_protect", 0440, "0" },
	{ "nandswap", "stat_info", 0440, "0" },
	{ "nandswap", "dev_life", 0200, "" },
	{ "nandswap", "swap_limit", 0200, "" },
	{ "rpmh_modem", "sleepinfo", 0666, "" },
	{ "clk", "clk_enabled_list", 0444, "" },
	{ "oplus_afs_config", "afs_enable", 0666, "0", true },	/* the kernel AFS feature is not here */
	{ NULL, "lpm_message", 0644, "" },
	{ NULL, "suspend_clks", 0644, "" },
	{ NULL, "mm_fb", 0660, "" },
	{ NULL, "pmicWd", 0660, "0", true },	/* no PMIC watchdog on onyx: reads report it off */
	{ NULL, "kpdpwr", 0660, "0", true },
};

/* /sys/kernel/wakelock_profiler, rc: init.oplus.rootdir.rc */
static struct onyx_node sys_nodes[] = {
	{ NULL, "active_max", 0664, "0" },
	{ NULL, "kernel_time", 0664, "0" },
	{ NULL, "wakeup_stastisc_reset", 0664, "0" },
};

/* val_lock held; buf is the NUL-terminated, whitespace-trimmed text a writer sent */
static void onyx_node_store(struct onyx_node *n, const char *buf)
{
	/* echo config/stat values like a real node; never let a read claim a feature runs */
	if (!n->fixed)
		strscpy(n->val, buf, sizeof(n->val));
}

static int node_show(struct onyx_node *n, char *out, size_t len)
{
	int k;

	mutex_lock(&val_lock);
	k = n->val[0] ? scnprintf(out, len, "%s\n", n->val) : 0;
	mutex_unlock(&val_lock);
	return k;
}

/* the whole write is accepted (writers do not retry), only the first VAL_LEN - 1 bytes are kept */
static void node_write(struct onyx_node *n, char *tmp)
{
	mutex_lock(&val_lock);
	onyx_node_store(n, strim(tmp));
	mutex_unlock(&val_lock);
}

static ssize_t proc_node_read(struct file *f, char __user *ubuf, size_t len, loff_t *ppos)
{
	char tmp[VAL_LEN + 1];

	return simple_read_from_buffer(ubuf, len, ppos, tmp,
				       node_show(pde_data(file_inode(f)), tmp, sizeof(tmp)));
}

static ssize_t proc_node_write(struct file *f, const char __user *ubuf, size_t len, loff_t *ppos)
{
	char tmp[VAL_LEN];
	size_t k = min(len, sizeof(tmp) - 1);

	if (copy_from_user(tmp, ubuf, k))
		return -EFAULT;
	tmp[k] = '\0';
	node_write(pde_data(file_inode(f)), tmp);
	return len;
}

static const struct proc_ops proc_node_ops = {
	.proc_read = proc_node_read,
	.proc_write = proc_node_write,
	.proc_lseek = default_llseek,
};

static ssize_t sys_node_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return node_show(container_of(attr, struct onyx_node, attr), buf, PAGE_SIZE);
}

static ssize_t sys_node_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,
			      size_t count)
{
	char tmp[VAL_LEN];

	strscpy(tmp, buf, sizeof(tmp));
	node_write(container_of(attr, struct onyx_node, attr), tmp);
	return count;
}

/*
 * /proc/oplus_afs_config/afs_config is a scene table driven by ioctl (layouts read from
 * libafsConfig.so): at boot system_server (afsConfigUpdate) loads ColorOS's afsConfig.pb as
 * one 8-byte record per scene type, and SystemUI and the launcher query their scene's record.
 */
#define AFS_IOC_UPDATE	0x40107101	/* { u32 version; u32 max_scene; u64 table } */
#define AFS_IOC_GET	0x800c7102	/* { u32 scene; u8 rec[8] }, rec filled in */
#define AFS_REC		8
#define AFS_MAX_SCENE	63
static u8 afs_table[(AFS_MAX_SCENE + 1) * AFS_REC];
static u32 afs_scenes;	/* records loaded; 0 until system_server's update */

static long afs_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct { u32 version, max_scene; u64 table; } up;
	struct { u32 scene; u8 rec[AFS_REC]; } get;
	long ret = 0;

	switch (cmd) {
	case AFS_IOC_UPDATE:
		if (copy_from_user(&up, (void __user *)arg, sizeof(up)))
			return -EFAULT;
		if (up.max_scene > AFS_MAX_SCENE)
			return -EINVAL;
		mutex_lock(&val_lock);
		if (copy_from_user(afs_table, u64_to_user_ptr(up.table), (up.max_scene + 1) * AFS_REC))
			ret = -EFAULT;
		else
			afs_scenes = up.max_scene + 1;
		mutex_unlock(&val_lock);
		return ret;
	case AFS_IOC_GET:
		if (copy_from_user(&get, (void __user *)arg, sizeof(get)))
			return -EFAULT;
		mutex_lock(&val_lock);
		if (get.scene < afs_scenes)
			memcpy(get.rec, afs_table + get.scene * AFS_REC, AFS_REC);
		else
			ret = -ENODATA;
		mutex_unlock(&val_lock);
		if (!ret && copy_to_user((void __user *)arg, &get, sizeof(get)))
			ret = -EFAULT;
		return ret;
	}
	return -ENOTTY;
}

static const struct proc_ops afs_ops = {
	.proc_ioctl = afs_ioctl,
};

static const char * const proc_dirs[] = { "nandswap", "rpmh_modem", "clk", "oplus_afs_config" };
static struct proc_dir_entry *dir_pde[ARRAY_SIZE(proc_dirs)];
static struct kobject *wl_kobj;

static struct proc_dir_entry *parent_of(const char *dir)
{
	int i;

	for (i = 0; dir && i < ARRAY_SIZE(proc_dirs); i++)
		if (!strcmp(dir, proc_dirs[i]))
			return dir_pde[i];
	return NULL;
}

static void onyx_compat_nodes_exit(void)
{
	int i;

	kobject_put(wl_kobj);	/* drops the sysfs files with it */
	for (i = 0; i < ARRAY_SIZE(proc_nodes); i++)
		if (!proc_nodes[i].dir)
			remove_proc_entry(proc_nodes[i].name, NULL);
	for (i = 0; i < ARRAY_SIZE(proc_dirs); i++)
		if (dir_pde[i])
			remove_proc_subtree(proc_dirs[i], NULL);
}

static int __init onyx_compat_nodes_init(void)
{
	int i, made = 0;

	for (i = 0; i < ARRAY_SIZE(proc_dirs); i++)
		dir_pde[i] = proc_mkdir(proc_dirs[i], NULL);

	for (i = 0; i < ARRAY_SIZE(proc_nodes); i++) {
		struct onyx_node *n = &proc_nodes[i];

		strscpy(n->val, n->init, sizeof(n->val));
		if (n->dir && !parent_of(n->dir))
			continue;	/* its dir could not be made; skip, do not fail the rest */
		made += !!proc_create_data(n->name, n->mode, parent_of(n->dir), &proc_node_ops, n);
	}
	if (parent_of("oplus_afs_config"))
		made += !!proc_create("afs_config", 0666, parent_of("oplus_afs_config"), &afs_ops);

	wl_kobj = kobject_create_and_add("wakelock_profiler", kernel_kobj);
	for (i = 0; wl_kobj && i < ARRAY_SIZE(sys_nodes); i++) {
		struct onyx_node *n = &sys_nodes[i];

		strscpy(n->val, n->init, sizeof(n->val));
		sysfs_attr_init(&n->attr.attr);
		n->attr.attr.name = n->name;
		n->attr.attr.mode = n->mode;
		n->attr.show = sys_node_show;
		n->attr.store = sys_node_store;
		made += !sysfs_create_file(wl_kobj, &n->attr.attr);
	}

	pr_info("onyx_compat_nodes: %d of %zu placeholder nodes\n", made,
		ARRAY_SIZE(proc_nodes) + ARRAY_SIZE(sys_nodes) + 1);
	return 0;
}

module_init(onyx_compat_nodes_init);
module_exit(onyx_compat_nodes_exit);
MODULE_DESCRIPTION("onyx placeholder /proc and /sys nodes ColorOS expects");
MODULE_LICENSE("GPL v2");
