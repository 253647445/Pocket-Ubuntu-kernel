/*
 * Machine check exception handling CPU-side for power7 and power8
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright 2013 IBM Corporation
 * Author: Mahesh Salgaonkar <mahesh@linux.vnet.ibm.com>
 */

#undef DEBUG
#define pr_fmt(fmt) "mce_power: " fmt

#include <linux/types.h>
#include <linux/ptrace.h>
#include <asm/mmu.h>
#include <asm/mce.h>
#include <asm/machdep.h>

static void flush_tlb_206(unsigned int num_sets, unsigned int action)
{
	unsigned long rb;
	unsigned int i;

	switch (action) {
	case TLB_INVAL_SCOPE_GLOBAL:
		rb = TLBIEL_INVAL_SET;
		break;
	case TLB_INVAL_SCOPE_LPID:
		rb = TLBIEL_INVAL_SET_LPID;
		break;
	default:
		BUG();
		break;
	}

	asm volatile("ptesync" : : : "memory");
	for (i = 0; i < num_sets; i++) {
		asm volatile("tlbiel %0" : : "r" (rb));
		rb += 1 << TLBIEL_INVAL_SET_SHIFT;
	}
	asm volatile("ptesync" : : : "memory");
}

/*
 * Generic routines to flush TLB on POWER processors. These routines
 * are used as flush_tlb hook in the cpu_spec.
 *
 * action => TLB_INVAL_SCOPE_GLOBAL:  Invalidate all TLBs.
 *	     TLB_INVAL_SCOPE_LPID: Invalidate TLB for current LPID.
 */
void __flush_tlb_power7(unsigned int action)
{
	flush_tlb_206(POWER7_TLB_SETS, action);
}

void __flush_tlb_power8(unsigned int action)
{
	flush_tlb_206(POWER8_TLB_SETS, action);
}

void __flush_tlb_power9(unsigned int action)
{
	unsigned int num_sets;

	if (radix_enabled())
		num_sets = POWER9_TLB_SETS_RADIX;
	else
		num_sets = POWER9_TLB_SETS_HASH;

	flush_tlb_206(num_sets, action);
}


/* flush SLBs and reload */
#ifdef CONFIG_PPC_STD_MMU_64
static void flush_and_reload_slb(void)
{
	struct slb_shadow *slb;
	unsigned long i, n;

	/* Invalidate all SLBs */
	asm volatile("slbmte %0,%0; slbia" : : "r" (0));

#ifdef CONFIG_KVM_BOOK3S_HANDLER
	/*
	 * If machine check is hit when in guest or in transition, we will
	 * only flush the SLBs and continue.
	 */
	if (get_paca()->kvm_hstate.in_guest)
		return;
#endif

	/* For host kernel, reload the SLBs from shadow SLB buffer. */
	slb = get_slb_shadow();
	if (!slb)
		return;

	n = min_t(u32, be32_to_cpu(slb->persistent), SLB_MIN_SIZE);

	/* Load up the SLB entries from shadow SLB */
	for (i = 0; i < n; i++) {
		unsigned long rb = be64_to_cpu(slb->save_area[i].esid);
		unsigned long rs = be64_to_cpu(slb->save_area[i].vsid);

		rb = (rb & ~0xFFFul) | i;
		asm volatile("slbmte %0,%1" : : "r" (rs), "r" (rb));
	}
}
#endif

static void flush_erat(void)
{
	asm volatile(PPC_INVALIDATE_ERAT : : :"memory");
}

#define MCE_FLUSH_SLB 1
#define MCE_FLUSH_TLB 2
#define MCE_FLUSH_ERAT 3

static int mce_flush(int what)
{
#ifdef CONFIG_PPC_STD_MMU_64
	if (what == MCE_FLUSH_SLB) {
		flush_and_reload_slb();
		return 1;
	}
#endif
	if (what == MCE_FLUSH_ERAT) {
		flush_erat();
		return 1;
	}
	if (what == MCE_FLUSH_TLB) {
		if (cur_cpu_spec && cur_cpu_spec->flush_tlb) {
			cur_cpu_spec->flush_tlb(TLB_INVAL_SCOPE_GLOBAL);
			return 1;
		}
	}

	return 0;
}

#define SRR1_MC_LOADSTORE(srr1)	((srr1) & PPC_BIT(42))

struct mce_ierror_table {
	unsigned long srr1_mask;
	unsigned long srr1_value;
	bool nip_valid; /* nip is a valid indicator of faulting address */
	unsigned int error_type;
	unsigned int error_subtype;
	unsigned int initiator;
	unsigned int severity;
};

static const struct mce_ierror_table mce_p7_ierror_table[] = {
{ 0x00000000001c0000, 0x0000000000040000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000001c0000, 0x0000000000080000, true,
  MCE_ERROR_TYPE_SLB, MCE_SLB_ERROR_PARITY,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000001c0000, 0x00000000000c0000, true,
  MCE_ERROR_TYPE_SLB, MCE_SLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000001c0000, 0x0000000000100000, true,
  MCE_ERROR_TYPE_SLB, MCE_SLB_ERROR_INDETERMINATE, /* BOTH */
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000001c0000, 0x0000000000140000, true,
  MCE_ERROR_TYPE_TLB, MCE_TLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000001c0000, 0x0000000000180000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_PAGE_TABLE_WALK_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000001c0000, 0x00000000001c0000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0, 0, 0, 0, 0, 0 } };

static const struct mce_ierror_table mce_p8_ierror_table[] = {
{ 0x00000000081c0000, 0x0000000000040000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000080000, true,
  MCE_ERROR_TYPE_SLB, MCE_SLB_ERROR_PARITY,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x00000000000c0000, true,
  MCE_ERROR_TYPE_SLB, MCE_SLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000100000, true,
  MCE_ERROR_TYPE_ERAT,MCE_ERAT_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000140000, true,
  MCE_ERROR_TYPE_TLB, MCE_TLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000180000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_PAGE_TABLE_WALK_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x00000000001c0000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000008000000, true,
  MCE_ERROR_TYPE_LINK,MCE_LINK_ERROR_IFETCH_TIMEOUT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000008040000, true,
  MCE_ERROR_TYPE_LINK,MCE_LINK_ERROR_PAGE_TABLE_WALK_IFETCH_TIMEOUT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0, 0, 0, 0, 0, 0 } };

static const struct mce_ierror_table mce_p9_ierror_table[] = {
{ 0x00000000081c0000, 0x0000000000040000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000080000, true,
  MCE_ERROR_TYPE_SLB, MCE_SLB_ERROR_PARITY,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x00000000000c0000, true,
  MCE_ERROR_TYPE_SLB, MCE_SLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000100000, true,
  MCE_ERROR_TYPE_ERAT,MCE_ERAT_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000140000, true,
  MCE_ERROR_TYPE_TLB, MCE_TLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000000180000, true,
  MCE_ERROR_TYPE_UE,  MCE_UE_ERROR_PAGE_TABLE_WALK_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000008000000, true,
  MCE_ERROR_TYPE_LINK,MCE_LINK_ERROR_IFETCH_TIMEOUT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000008040000, true,
  MCE_ERROR_TYPE_LINK,MCE_LINK_ERROR_PAGE_TABLE_WALK_IFETCH_TIMEOUT,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x00000000080c0000, true,
  MCE_ERROR_TYPE_RA,  MCE_RA_ERROR_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000008100000, true,
  MCE_ERROR_TYPE_RA,  MCE_RA_ERROR_PAGE_TABLE_WALK_IFETCH,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0x00000000081c0000, 0x0000000008140000, false,
  MCE_ERROR_TYPE_RA,  MCE_RA_ERROR_STORE,
  MCE_INITIATOR_CPU,  MCE_SEV_FATAL, }, /* ASYNC is fatal */
{ 0x00000000081c0000, 0x0000000008180000, false,
  MCE_ERROR_TYPE_LINK,MCE_LINK_ERROR_STORE_TIMEOUT,
  MCE_INITIATOR_CPU,  MCE_SEV_FATAL, }, /* ASYNC is fatal */
{ 0x00000000081c0000, 0x00000000081c0000, true,
  MCE_ERROR_TYPE_RA,  MCE_RA_ERROR_PAGE_TABLE_WALK_IFETCH_FOREIGN,
  MCE_INITIATOR_CPU,  MCE_SEV_ERROR_SYNC, },
{ 0, 0, 0, 0, 0, 0 } };

struct mce_derror_table {
	unsigned long dsisr_value;
	bool dar_valid; /* dar is a valid indicator of faulting address */
	unsigned int error_type;
	unsigned int error_subtype;
	unsigned int initiator;
	unsigned int severity;
};

static const struct mce_derror_table mce_p7_derror_table[] = {
{ 0x00008000, false,
  MCE_ERROR_TYPE_UE,   MCE_UE_ERROR_LOAD_STORE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00004000, true,
  MCE_ERROR_TYPE_UE,   MCE_UE_ERROR_PAGE_TABLE_WALK_LOAD_STORE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000800, true,
  MCE_ERROR_TYPE_ERAT, MCE_ERAT_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000400, true,
  MCE_ERROR_TYPE_TLB,  MCE_TLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000100, true,
  MCE_ERROR_TYPE_SLB,  MCE_SLB_ERROR_PARITY,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000080, true,
  MCE_ERROR_TYPE_SLB,  MCE_SLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000040, true,
  MCE_ERROR_TYPE_SLB,  MCE_SLB_ERROR_INDETERMINATE, /* BOTH */
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0, false, 0, 0, 0, 0 } };

static const struct mce_derror_table mce_p8_derror_table[] = {
{ 0x00008000, false,
  MCE_ERROR_TYPE_UE,   MCE_UE_ERROR_LOAD_STORE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00004000, true,
  MCE_ERROR_TYPE_UE,   MCE_UE_ERROR_PAGE_TABLE_WALK_LOAD_STORE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00002000, true,
  MCE_ERROR_TYPE_LINK, MCE_LINK_ERROR_LOAD_TIMEOUT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00001000, true,
  MCE_ERROR_TYPE_LINK, MCE_LINK_ERROR_PAGE_TABLE_WALK_LOAD_STORE_TIMEOUT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000800, true,
  MCE_ERROR_TYPE_ERAT, MCE_ERAT_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000400, true,
  MCE_ERROR_TYPE_TLB,  MCE_TLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000200, true,
  MCE_ERROR_TYPE_ERAT, MCE_ERAT_ERROR_MULTIHIT, /* SECONDARY ERAT */
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000100, true,
  MCE_ERROR_TYPE_SLB,  MCE_SLB_ERROR_PARITY,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000080, true,
  MCE_ERROR_TYPE_SLB,  MCE_SLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0, false, 0, 0, 0, 0 } };

static const struct mce_derror_table mce_p9_derror_table[] = {
{ 0x00008000, false,
  MCE_ERROR_TYPE_UE,   MCE_UE_ERROR_LOAD_STORE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00004000, true,
  MCE_ERROR_TYPE_UE,   MCE_UE_ERROR_PAGE_TABLE_WALK_LOAD_STORE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00002000, true,
  MCE_ERROR_TYPE_LINK, MCE_LINK_ERROR_LOAD_TIMEOUT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00001000, true,
  MCE_ERROR_TYPE_LINK, MCE_LINK_ERROR_PAGE_TABLE_WALK_LOAD_STORE_TIMEOUT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000800, true,
  MCE_ERROR_TYPE_ERAT, MCE_ERAT_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000400, true,
  MCE_ERROR_TYPE_TLB,  MCE_TLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000200, false,
  MCE_ERROR_TYPE_USER, MCE_USER_ERROR_TLBIE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000100, true,
  MCE_ERROR_TYPE_SLB,  MCE_SLB_ERROR_PARITY,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000080, true,
  MCE_ERROR_TYPE_SLB,  MCE_SLB_ERROR_MULTIHIT,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000040, true,
  MCE_ERROR_TYPE_RA,   MCE_RA_ERROR_LOAD,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000020, false,
  MCE_ERROR_TYPE_RA,   MCE_RA_ERROR_PAGE_TABLE_WALK_LOAD_STORE,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000010, false,
  MCE_ERROR_TYPE_RA,   MCE_RA_ERROR_PAGE_TABLE_WALK_LOAD_STORE_FOREIGN,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0x00000008, false,
  MCE_ERROR_TYPE_RA,   MCE_RA_ERROR_LOAD_STORE_FOREIGN,
  MCE_INITIATOR_CPU,   MCE_SEV_ERROR_SYNC, },
{ 0, false, 0, 0, 0, 0 } };

static int mce_handle_ierror(struct pt_regs *regs,
		const struct mce_ierror_table table[],
		struct mce_error_info *mce_err, uint64_t *addr)
{
	uint64_t srr1 = regs->msr;
	int handled = 0;
	int i;

	*addr = 0;

	for (i = 0; table[i].srr1_mask; i++) {
		if ((srr1 & table[i].srr1_mask) != table[i].srr1_value)
			continue;

		/* attempt to correct the error */
		switch (table[i].error_type) {
		case MCE_ERROR_TYPE_SLB:
			handled = mce_flush(MCE_FLUSH_SLB);
			break;
		case MCE_ERROR_TYPE_ERAT:
			handled = mce_flush(MCE_FLUSH_ERAT);
			break;
		case MCE_ERROR_TYPE_TLB:
			handled = mce_flush(MCE_FLUSH_TLB);
			break;
		}

		/* now fill in mce_error_info */
		mce_err->error_type = table[i].error_type;
		switch (table[i].error_type) {
		case MCE_ERROR_TYPE_UE:
			mce_err->u.ue_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_SLB:
			mce_err->u.slb_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_ERAT:
			mce_err->u.erat_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_TLB:
			mce_err->u.tlb_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_USER:
			mce_err->u.user_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_RA:
			mce_err->u.ra_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_LINK:
			mce_err->u.link_error_type = table[i].error_subtype;
			break;
		}
		mce_err->severity = table[i].severity;
		mce_err->initiator = table[i].initiator;
		if (table[i].nip_valid)
			*addr = regs->nip;
		return handled;
	}

	mce_err->error_type = MCE_ERROR_TYPE_UNKNOWN;
	mce_err->severity = MCE_SEV_ERROR_SYNC;
	mce_err->initiator = MCE_INITIATOR_CPU;

	return 0;
}

static int mce_handle_derror(struct pt_regs *regs,
		const struct mce_derror_table table[],
		struct mce_error_info *mce_err, uint64_t *addr)
{
	uint64_t dsisr = regs->dsisr;
	int handled = 0;
	int found = 0;
	int i;

	*addr = 0;

	for (i = 0; table[i].dsisr_value; i++) {
		if (!(dsisr & table[i].dsisr_value))
			continue;

		/* attempt to correct the error */
		switch (table[i].error_type) {
		case MCE_ERROR_TYPE_SLB:
			if (mce_flush(MCE_FLUSH_SLB))
				handled = 1;
			break;
		case MCE_ERROR_TYPE_ERAT:
			if (mce_flush(MCE_FLUSH_ERAT))
				handled = 1;
			break;
		case MCE_ERROR_TYPE_TLB:
			if (mce_flush(MCE_FLUSH_TLB))
				handled = 1;
			break;
		}

		/*
		 * Attempt to handle multiple conditions, but only return
		 * one. Ensure uncorrectable errors are first in the table
		 * to match.
		 */
		if (found)
			continue;

		/* now fill in mce_error_info */
		mce_err->error_type = table[i].error_type;
		switch (table[i].error_type) {
		case MCE_ERROR_TYPE_UE:
			mce_err->u.ue_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_SLB:
			mce_err->u.slb_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_ERAT:
			mce_err->u.erat_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_TLB:
			mce_err->u.tlb_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_USER:
			mce_err->u.user_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_RA:
			mce_err->u.ra_error_type = table[i].error_subtype;
			break;
		case MCE_ERROR_TYPE_LINK:
			mce_err->u.link_error_type = table[i].error_subtype;
			break;
		}
		mce_err->severity = table[i].severity;
		mce_err->initiator = table[i].initiator;
		if (table[i].dar_valid)
			*addr = regs->dar;

		found = 1;
	}

	if (found)
		return handled;

	mce_err->error_type = MCE_ERROR_TYPE_UNKNOWN;
	mce_err->severity = MCE_SEV_ERROR_SYNC;
	mce_err->initiator = MCE_INITIATOR_CPU;

	return 0;
}

static long mce_handle_ue_error(struct pt_regs *regs)
{
	long handled = 0;

	/*
	 * On specific SCOM read via MMIO we may get a machine check
	 * exception with SRR0 pointing inside opal. If that is the
	 * case OPAL may have recovery address to re-read SCOM data in
	 * different way and hence we can recover from this MC.
	 */

	if (ppc_md.mce_check_early_recovery) {
		if (ppc_md.mce_check_early_recovery(regs))
			handled = 1;
	}
	return handled;
}

static long mce_handle_error(struct pt_regs *regs,
		const struct mce_derror_table dtable[],
		const struct mce_ierror_table itable[])
{
	struct mce_error_info mce_err = { 0 };
	uint64_t addr;
	uint64_t srr1 = regs->msr;
	long handled;

	if (SRR1_MC_LOADSTORE(srr1))
		handled = mce_handle_derror(regs, dtable, &mce_err, &addr);
	else
		handled = mce_handle_ierror(regs, itable, &mce_err, &addr);

	if (!handled && mce_err.error_type == MCE_ERROR_TYPE_UE)
		handled = mce_handle_ue_error(regs);

	save_mce_event(regs, handled, &mce_err, regs->nip, addr);

	return handled;
}

long __machine_check_early_realmode_p7(struct pt_regs *regs)
{
	/* P7 DD1 leaves top bits of DSISR undefined */
	regs->dsisr &= 0x0000ffff;

	return mce_handle_error(regs, mce_p7_derror_table, mce_p7_ierror_table);
}

long __machine_check_early_realmode_p8(struct pt_regs *regs)
{
	return mce_handle_error(regs, mce_p8_derror_table, mce_p8_ierror_table);
}

long __machine_check_early_realmode_p9(struct pt_regs *regs)
{
	return mce_handle_error(regs, mce_p9_derror_table, mce_p9_ierror_table);
}
