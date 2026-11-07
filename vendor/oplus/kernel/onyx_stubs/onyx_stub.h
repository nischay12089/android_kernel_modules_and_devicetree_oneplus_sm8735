/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * onyx: placeholder for a OnePlus module that cannot work on this phone (see docs/STATUS.md,
 * "Not ported"). It keeps the OnePlus module name so the set is complete, says once why it is
 * empty, and does nothing else: no hooks, no nodes, no hardware access.
 */
#ifndef _ONYX_STUB_H
#define _ONYX_STUB_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define ONYX_STUB(why)							\
static int __init onyx_stub_init(void)					\
{									\
	pr_info("%s: placeholder on onyx, %s\n", KBUILD_MODNAME, why);	\
	return 0;							\
}									\
static void __exit onyx_stub_exit(void)					\
{									\
}									\
module_init(onyx_stub_init);						\
module_exit(onyx_stub_exit);						\
MODULE_DESCRIPTION("onyx placeholder: " why);				\
MODULE_LICENSE("GPL v2")

#endif /* _ONYX_STUB_H */
