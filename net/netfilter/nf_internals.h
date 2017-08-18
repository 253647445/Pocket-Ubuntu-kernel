#ifndef _NF_INTERNALS_H
#define _NF_INTERNALS_H

#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#ifdef CONFIG_NETFILTER_DEBUG
#define NFDEBUG(format, args...)  printk(KERN_DEBUG format , ## args)
#else
#define NFDEBUG(format, args...)
#endif

/* nf_queue.c */
int nf_queue(struct sk_buff *skb, struct nf_hook_state *state,
	     struct nf_hook_entry **entryp, unsigned int verdict);
unsigned int nf_queue_nf_hook_drop(struct net *net);
int __init netfilter_queue_init(void);

/* nf_log.c */
int __init netfilter_log_init(void);

#endif
