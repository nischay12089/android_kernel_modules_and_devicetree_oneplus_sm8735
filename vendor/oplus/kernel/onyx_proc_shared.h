/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * onyx: several oplus modules create the same top-level /proc dir (oplus_mem, task_info) and
 * init loads them in parallel. proc_mkdir() on an existing dir WARNs with a stack dump, and
 * procfs exports no lookup, so check the path first. Returns NULL when the dir already exists:
 * callers then create their entries as "dir/child", which they already do on NULL.
 */
#ifndef _ONYX_PROC_SHARED_H
#define _ONYX_PROC_SHARED_H

#include <linux/module.h>
#include <linux/namei.h>
#include <linux/proc_fs.h>

/* kern_path() lives in this namespace; other oplus modules import it the same way */
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

static inline struct proc_dir_entry *onyx_proc_mkdir_shared(const char *name)
{
	char buf[64];
	struct path p;

	snprintf(buf, sizeof(buf), "/proc/%s", name);
	if (!kern_path(buf, LOOKUP_DIRECTORY, &p)) {
		path_put(&p);
		return NULL;
	}
	/* shortcut: two first creators in the same instant can still WARN once; needs a single owner module to close */
	return proc_mkdir(name, NULL);
}

#endif /* _ONYX_PROC_SHARED_H */
