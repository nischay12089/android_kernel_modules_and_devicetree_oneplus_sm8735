/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * onyx: OnePlus modules keep records for the OnePlus bootloader in SMEM items they allocate
 * (dump info 129, minidump 122, ramdump 124, PMIC history 134). On this Xiaomi firmware an item
 * may already exist with another owner and size (135 is 0 bytes), and writing a OnePlus struct
 * into it would corrupt memory the modem and DSPs share. Use an item only when its size is
 * exactly what qcom_smem_alloc(need) gives: the exact size in a partition, ALIGN(need, 8) in
 * the legacy global heap.
 */
#ifndef _ONYX_SMEM_H
#define _ONYX_SMEM_H

#include <linux/align.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <linux/soc/qcom/smem.h>

static inline void *onyx_smem_get_exact(unsigned int host, unsigned int item, size_t need)
{
	size_t size = 0;
	void *p = qcom_smem_get(host, item, &size);

	if (IS_ERR_OR_NULL(p))
		return NULL;
	if (size != need && size != ALIGN(need, 8)) {
		pr_warn_once("onyx: SMEM item %u holds %zu bytes, not a %zu-byte OnePlus record: unused\n",
			     item, size, need);
		return NULL;
	}
	return p;
}

#endif /* _ONYX_SMEM_H */
