/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * onyx: GKI exports only some built-in tracepoints (__tracepoint_X in Module.symvers), so
 * register_trace_X() fails to link for the rest. for_each_kernel_tracepoint() and
 * tracepoint_probe_register() are exported, so attach by name instead. The unevaluated
 * register_trace_X() call keeps the compile-time check that the probe matches TP_PROTO.
 */
#ifndef _ONYX_TRACEPOINT_H
#define _ONYX_TRACEPOINT_H

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/tracepoint.h>

struct onyx_tp_lookup {
	const char *name;
	struct tracepoint *tp;
};

static inline void onyx_tp_match(struct tracepoint *tp, void *priv)
{
	struct onyx_tp_lookup *l = priv;

	if (!l->tp && !strcmp(tp->name, l->name))
		l->tp = tp;
}

static inline struct tracepoint *onyx_tp_find(const char *name)
{
	struct onyx_tp_lookup l = { .name = name };

	for_each_kernel_tracepoint(onyx_tp_match, &l);
	return l.tp;
}

#define onyx_tp_register(name, probe, data) ({					\
	struct tracepoint *__tp = onyx_tp_find(#name);				\
	(void)sizeof(register_trace_##name(probe, data));			\
	__tp ? tracepoint_probe_register(__tp, (void *)(probe), data) : -ENOENT;	\
})

#define onyx_tp_unregister(name, probe, data) ({				\
	struct tracepoint *__tp = onyx_tp_find(#name);				\
	(void)sizeof(unregister_trace_##name(probe, data));			\
	__tp ? tracepoint_probe_unregister(__tp, (void *)(probe), data) : -ENOENT;	\
})

#endif /* _ONYX_TRACEPOINT_H */
