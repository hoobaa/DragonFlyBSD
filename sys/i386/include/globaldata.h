/*-
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Only machine-dependant code should ever include this file.  MI
 *	code and header files do NOT include this file.  e.g. sys/globaldata.h
 *	should not include this file.
 *
 * $FreeBSD: src/sys/i386/include/globaldata.h,v 1.11.2.1 2000/05/16 06:58:10 dillon Exp $
 * $DragonFly: src/sys/i386/include/Attic/globaldata.h,v 1.13 2003/06/28 04:16:03 dillon Exp $
 */

#ifndef _MACHINE_GLOBALDATA_H_
#define _MACHINE_GLOBALDATA_H_

#ifndef _SYS_GLOBALDATA_H_
#include <sys/globaldata.h>	/* struct globaldata */
#endif
#ifndef _SYS_THREAD_H_
#include <sys/thread.h>		/* struct thread */
#endif
#ifndef _MACHINE_SEGMENTS_H_
#include <machine/segments.h>	/* struct segment_descriptor */
#endif
#ifndef _MACHINE_TSS_H_
#include <machine/tss.h>	/* struct i386tss */
#endif


struct mdglobaldata {
	struct globaldata mi;
	struct thread   gd_idlethread;
	struct segment_descriptor gd_common_tssd;
	struct segment_descriptor *gd_tss_gdt;
	struct thread   *gd_npxthread;
	struct i386tss  gd_common_tss;
	int		gd_currentldt;	/* USER_LDT */
	u_int		gd_cpu_lockid;
	u_int		gd_other_cpus;
	u_int		gd_ss_eflags;
	pt_entry_t	*gd_CMAP1;
	pt_entry_t	*gd_CMAP2;
	pt_entry_t	*gd_CMAP3;
	pt_entry_t	*gd_PMAP1;
	caddr_t		gd_CADDR1;
	caddr_t		gd_CADDR2;
	caddr_t		gd_CADDR3;
	unsigned	*gd_PADDR1;
};

/*
 * This is the upper (0xff800000) address space layout that is per-cpu.
 * It is setup in locore.s and pmap.c for the BSP and in mp_machdep.c for
 * each AP.  genassym helps export this to the assembler code.
 */
struct privatespace {
	/* page 0 - data page */
	struct mdglobaldata mdglobaldata;
	char		__filler0[PAGE_SIZE - sizeof(struct mdglobaldata)];

	/* page 1..4 - CPAGE1,CPAGE2,CPAGE3,PPAGE1 */
	char		CPAGE1[PAGE_SIZE];
	char		CPAGE2[PAGE_SIZE];
	char		CPAGE3[PAGE_SIZE];
	char		PPAGE1[PAGE_SIZE];

	/* page 5..4+UPAGES - idle stack (UPAGES pages) */
	char		idlestack[UPAGES * PAGE_SIZE];
};

extern struct privatespace CPU_prvspace[];

#define mdcpu  		((struct mdglobaldata *)_get_mycpu())
#define npxthread       mdcpu->gd_npxthread

#endif
