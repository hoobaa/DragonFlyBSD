/*
 * Copyright (c) 2003 Matthew Dillon <dillon@backplane.com>
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
 * $DragonFly: src/sys/platform/pc32/i386/bcopy.s,v 1.4 2004/05/01 03:38:36 dillon Exp $
 */
/*
 * bcopy(source:%esi, target:%edi, count:%ecx)
 *
 *	note: esi, edi, eax, ecx, and edx may be destroyed
 */

#include "use_npx.h"

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#include "assym.s"

	.text

	/*
	 * bcopyb() is a 'dumb' byte-granular bcopy.  It is only used by
	 * devices which need to bcopy device-mapped memory which cannot
	 * otherwise handle 16 or 32 bit ops.
	 */
	ALIGN_TEXT
ENTRY(bcopyb)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax			/* overlapping && src < dst? */
	jb	1f
	cld					/* nope, copy forwards */
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi			/* copy backwards. */
	addl	%ecx,%esi
	decl	%edi
	decl	%esi
	std
	rep
	movsb
	popl	%edi
	popl	%esi
	cld
	ret


	/*
	 * If memcpy/bcopy is called as part of a copyin or copyout, the
	 * on-fault routine is set up to do a 'ret'.  We have to restore
	 * %ebx and return to the copyin/copyout fault handler.
	 */
generic_onfault:
	popl	%ebx
	addl	$4,%esp		/* skip normal return vector */
	ret			/* return to copyin/copyout fault handler */

	/*
	 * GENERIC BCOPY() - COPY DIRECTION CHECK AND FORWARDS COPY
	 *
	 *	Reasonably optimal on all modern machines.
	 */

	SUPERALIGN_TEXT
ENTRY(asm_generic_memcpy)	/* memcpy() entry point use optimal copy */
	pushl	%ebx
	pushl	$generic_onfault
	jmp	2f

	SUPERALIGN_TEXT
ENTRY(asm_generic_bcopy)
	pushl	%ebx
	pushl	$generic_onfault
	cmpl	%esi,%edi	/* if (edi < esi) fwd copy ok */
	jb	2f
	addl	%ecx,%esi
	cmpl	%esi,%edi	/* if (edi < esi + count) do bkwrds copy */
	jb	10f
	subl	%ecx,%esi
	jmp	2f

	SUPERALIGN_TEXT
1:
	movl	(%esi),%eax
	movl	4(%esi),%ebx
	movl	8(%esi),%edx
	movl	%eax,(%edi)
	movl	12(%esi),%eax
	movl	%ebx,4(%edi)
	movl	16(%esi),%ebx
	movl	%edx,8(%edi)
	movl	20(%esi),%edx
	movl	%eax,12(%edi)
	movl	24(%esi),%eax
	movl	%ebx,16(%edi)
	movl	28(%esi),%ebx
	movl	%edx,20(%edi)
	movl	%eax,24(%edi)
	addl	$32,%esi
	movl	%ebx,28(%edi)
	addl	$32,%edi
2:
	subl	$32,%ecx
	jae	1b
	addl	$32,%ecx
	jz	3f
	cld
	rep
	movsb
3:
	addl	$4,%esp
	popl	%ebx
	ret

	/*
	 * GENERIC_BCOPY() - BACKWARDS COPY
	 */
	SUPERALIGN_TEXT
10:
	addl	%ecx,%edi
	jmp	12f

	SUPERALIGN_TEXT
11:
	movl    -4(%esi),%eax
	movl    -8(%esi),%ebx
	movl    -12(%esi),%edx
	movl    %eax,-4(%edi)
	movl    -16(%esi),%eax
	movl    %ebx,-8(%edi)
	movl    -20(%esi),%ebx
	movl    %edx,-12(%edi)
	movl    -24(%esi),%edx
	movl    %eax,-16(%edi)
	movl    -28(%esi),%eax
	movl    %ebx,-20(%edi)
	movl    -32(%esi),%ebx
	movl    %edx,-24(%edi)
	movl    %eax,-28(%edi)
	subl    $32,%esi
	movl    %ebx,-32(%edi)
	subl    $32,%edi
12:
	subl    $32,%ecx
	jae	11b
	addl	$32,%ecx
	jz	13f
	decl	%esi
	decl	%edi
	std
	rep
	movsb
	cld
13:
	addl	$4,%esp
	popl	%ebx
	ret

	/*
	 * MMX BCOPY() - COPY DIRECTION CHECK AND FORWARDS COPY
	 *
	 * Reasonably optimal on all modern machines with MMX or SSE2.
	 * XXX But very messy, we need a better way to use fp in the kernel.
	 *
	 * note: esi, edi, eax, ecx, and edx may be destroyed
	 *
	 * In order for the kernel to be able to use the FPU:
	 *
	 *	(1) The kernel may not already be using the fpu
	 *
	 *	(2) If the fpu is owned by the application, we must save
	 *	    its state.  If the fpu is not owned by the application
	 *	    the application's saved fp state may already exist
	 *	    in TD_SAVEFPU.
	 *
	 *	(3) We cannot allow the kernel overwrite the application's
	 *	    FPU state with our own, so we allocate space on the
	 *	    stack and create a new TD_SAVEFPU, saving the old
	 *	    pointer.
	 *	    
	 *	(4) While we are using the FP unit, an interrupt may come
	 *	    along and preempt us, causing our FP state to be saved.
	 *	    We will fault/restore upon resumption.  Our FP state
	 *	    will be saved on the stack.
	 *
	 *	(5) To clean up we throw away our FP state and, zero out
	 *	    npxthread to indicate that the application's FP state
	 *	    is stored in TD_SAVEFPU, and we then restore the original
	 *	    TD_SAVEFPU.
	 *
	 *	    We do not attempt to restore the application's FP state.
	 *	    We set the TS bit to guarentee that the application will
	 *	    fault when it next tries to access the FP (to restore its
	 *	    state).
	 *
	 *  NOTE: fxsave requires a 16-byte aligned address
	 *
	 *  NOTE: RACES (which are ok): 
	 *
	 *	+ interrupt saves fp state after we check npxthread but
	 *	  before we call fxsave
	 *	+ interrupt saves application fp state after we change
	 *	  TD_SAVEFPU.  Data will be ignored.
	 *	+ interrupt occurs in critical section.  interrupt will be
	 *	  delayed until we return or block (unless we check for
	 *	  pending interrupts but I'm not going to bother for now).
	 *
	 *  MMX+XMM (SSE2): Typical on Athlons, later P4s. 128 bit media insn.
	 *  MMX: Typical on XPs and P3s.  64 bit media insn.
	 */

#define MMX_SAVE_BLOCK(missfunc)		\
	cmpl	$2048,%ecx ;			\
	jb	missfunc ;			\
	btsl	$1,PCPU(kernel_fpu_lock) ;	\
	jc	missfunc ;			\
	pushl	%ebx ;				\
	pushl	%ebp ;				\
	movl	%esp, %ebp ;			\
	movl	PCPU(curthread),%edx ;		\
	movl	TD_SAVEFPU(%edx),%ebx ;		\
	subl	$512,%esp ;			\
	andl	$0xfffffff0,%esp ;		\
	addl	$TDPRI_CRIT,TD_PRI(%edx) ;	\
	cmpl	%edx,PCPU(npxthread) ;		\
	jne	100f ;				\
	fxsave	0(%ebx) ;			\
100: ;						\
	movl	%esp,TD_SAVEFPU(%edx) ;		\
	movl	%edx,PCPU(npxthread) ;		\
	clts ;					\
	fninit ;				\
	subl	$TDPRI_CRIT,TD_PRI(%edx) ;	\
	pushl	$mmx_onfault

	/*
	 *  NOTE: RACES (which are ok): 
	 *
	 *	+ interrupt occurs after we store NULL to npxthread.  No
	 *	  state will be saved (because npxthread is NULL).  Thread
	 *	  switches never restore npxthread, only a DNA trap does that.
	 *	+ we can safely restore TD_SAFEFPU after NULLing npxthread.
	 *	+ we can safely set TS any time after NULLing npxthread.
	 */

#define MMX_RESTORE_BLOCK			\
	addl	$4,%esp ;			\
	MMX_RESTORE_BLOCK2

#define MMX_RESTORE_BLOCK2			\
	movl	PCPU(curthread),%edx ;		\
	movl	$0,PCPU(npxthread) ;		\
	movl	%ebx,TD_SAVEFPU(%edx) ;		\
	smsw	%ax ;				\
	movl	%ebp,%esp ;			\
	orb	$CR0_TS,%al ;			\
	popl	%ebp ;				\
	lmsw	%ax ;				\
	popl	%ebx ;				\
	movl	$0,PCPU(kernel_fpu_lock)

	/*
	 * xmm/mmx_onfault routine.  Restore the fpu state, skip the normal
	 * return vector, and return to the caller's on-fault routine
	 * (which was pushed on the callers stack just before he calle us)
	 */
mmx_onfault:
	MMX_RESTORE_BLOCK2
	addl	$4,%esp
	ret

	/*
	 * MXX entry points - only support 64 bit media instructions
	 */
	SUPERALIGN_TEXT
ENTRY(asm_mmx_memcpy)		/* memcpy() entry point use optimal copy */
	MMX_SAVE_BLOCK(asm_generic_memcpy)
	jmp	5f

	SUPERALIGN_TEXT
ENTRY(asm_mmx_bcopy)
	MMX_SAVE_BLOCK(asm_generic_bcopy)
	cmpl	%esi,%edi	/* if (edi < esi) fwd copy ok */
	jb	5f
	addl	%ecx,%esi
	cmpl	%esi,%edi	/* if (edi < esi + count) do bkwrds copy */
	jb	10f
	subl	%ecx,%esi
	jmp	5f

	/*
	 * XMM entry points - support 128 bit media instructions
	 */
	SUPERALIGN_TEXT
ENTRY(asm_xmm_memcpy)		/* memcpy() entry point use optimal copy */
	MMX_SAVE_BLOCK(asm_generic_memcpy)
	jmp	1f

	SUPERALIGN_TEXT
ENTRY(asm_xmm_bcopy)
	MMX_SAVE_BLOCK(asm_generic_bcopy)
	cmpl	%esi,%edi	/* if (edi < esi) fwd copy ok */
	jb	1f
	addl	%ecx,%esi
	cmpl	%esi,%edi	/* if (edi < esi + count) do bkwrds copy */
	jb	10f
	subl	%ecx,%esi
1:
	movl	%esi,%eax	/* skip xmm if the data is not aligned */
	andl	$15,%eax
	jnz	5f
	movl	%edi,%eax
	andl	$15,%eax
	jz	3f
	jmp	5f

	SUPERALIGN_TEXT
2:
	movdqa	(%esi),%xmm0
	movdqa  16(%esi),%xmm1
	movdqa  32(%esi),%xmm2
	movdqa  48(%esi),%xmm3
	movdqa  64(%esi),%xmm4
	movdqa  80(%esi),%xmm5
	movdqa  96(%esi),%xmm6
	movdqa  112(%esi),%xmm7
	/*prefetchnta 128(%esi) 3dNOW */
	addl	$128,%esi

	/*
	 * movdqa or movntdq can be used.
	 */
	movdqa  %xmm0,(%edi)
	movdqa  %xmm1,16(%edi)
	movdqa  %xmm2,32(%edi)
	movdqa  %xmm3,48(%edi)
	movdqa  %xmm4,64(%edi)
	movdqa  %xmm5,80(%edi)
	movdqa  %xmm6,96(%edi)
	movdqa  %xmm7,112(%edi)
	addl	$128,%edi
3:
	subl	$128,%ecx
	jae	2b
	addl	$128,%ecx
	jz	6f
	jmp	5f
	SUPERALIGN_TEXT
4:
	movq	(%esi),%mm0
	movq	8(%esi),%mm1
	movq	16(%esi),%mm2
	movq	24(%esi),%mm3
	movq	32(%esi),%mm4
	movq	40(%esi),%mm5
	movq	48(%esi),%mm6
	movq	56(%esi),%mm7
	/*prefetchnta 128(%esi) 3dNOW */
	addl	$64,%esi
	movq	%mm0,(%edi)
	movq	%mm1,8(%edi)
	movq	%mm2,16(%edi)
	movq	%mm3,24(%edi)
	movq	%mm4,32(%edi)
	movq	%mm5,40(%edi)
	movq	%mm6,48(%edi)
	movq	%mm7,56(%edi)
	addl	$64,%edi
5:
	subl	$64,%ecx
	jae	4b
	addl	$64,%ecx
	jz	6f
	cld
	rep
	movsb
6:
	MMX_RESTORE_BLOCK
	ret

	/*
	 * GENERIC_BCOPY() - BACKWARDS COPY
	 *
	 * Don't bother using xmm optimizations, just stick with mmx.
	 */
	SUPERALIGN_TEXT
10:
	addl	%ecx,%edi
	jmp	12f

	SUPERALIGN_TEXT
11:
	movq	-64(%esi),%mm0
	movq	-56(%esi),%mm1
	movq	-48(%esi),%mm2
	movq	-40(%esi),%mm3
	movq	-32(%esi),%mm4
	movq	-24(%esi),%mm5
	movq	-16(%esi),%mm6
	movq	-8(%esi),%mm7
	/*prefetchnta -128(%esi)*/
	subl	$64,%esi
	movq	%mm0,-64(%edi)
	movq	%mm1,-56(%edi)
	movq	%mm2,-48(%edi)
	movq	%mm3,-40(%edi)
	movq	%mm4,-32(%edi)
	movq	%mm5,-24(%edi)
	movq	%mm6,-16(%edi)
	movq	%mm7,-8(%edi)
	subl	$64,%edi
12:
	subl    $64,%ecx
	jae	11b
	addl	$64,%ecx
	jz	13f
	decl	%esi
	decl	%edi
	std
	rep
	movsb
	cld
13:
	MMX_RESTORE_BLOCK
	ret

