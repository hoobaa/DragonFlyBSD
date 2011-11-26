/*	$KAME: altq_rmclass_debug.h,v 1.3 2002/11/29 04:36:24 kjc Exp $	*/
/*	$DragonFly: src/sys/net/altq/altq_rmclass_debug.h,v 1.3 2005/06/03 18:20:36 swildner Exp $ */

/*
 * Copyright (c) Sun Microsystems, Inc. 1998 All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the SMCC Technology
 *      Development Group at Sun Microsystems, Inc.
 *
 * 4. The name of the Sun Microsystems, Inc nor may not be used to endorse or
 *      promote products derived from this software without specific prior
 *      written permission.
 *
 * SUN MICROSYSTEMS DOES NOT CLAIM MERCHANTABILITY OF THIS SOFTWARE OR THE
 * SUITABILITY OF THIS SOFTWARE FOR ANY PARTICULAR PURPOSE.  The software is
 * provided "as is" without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this software.
 */

#ifndef _ALTQ_ALTQ_RMCLASS_DEBUG_H_
#define	_ALTQ_ALTQ_RMCLASS_DEBUG_H_

#ifdef _KERNEL
#include <sys/thread2.h>
#endif

/* #pragma ident	"@(#)rm_class_debug.h	1.7	98/05/04 SMI" */

/*
 * Cbq debugging macros
 */

#ifdef	CBQ_TRACE
#ifndef NCBQTRACE
#define	NCBQTRACE (16 * 1024)
#endif

/*
 * To view the trace output, using adb, type:
 *	adb -k /dev/ksyms /dev/mem <cr>, then type
 *	cbqtrace_count/D to get the count, then type
 *	cbqtrace_buffer,0tcount/Dp4C" "Xn
 *	This will dump the trace buffer from 0 to count.
 */
/*
 * in ALTQ, "call cbqtrace_dump(N)" from DDB to display 20 events
 * from Nth event in the circular buffer.
 */

struct cbqtrace {
	int count;
	int function;		/* address of function */
	int trace_action;	/* descriptive 4 characters */
	int object;		/* object operated on */
};

#define	CBQTRACEINIT() {				\
	if (cbqtrace_ptr == NULL)		\
		cbqtrace_ptr = cbqtrace_buffer; \
	else { \
		cbqtrace_ptr = cbqtrace_buffer; \
		bzero((void *)cbqtrace_ptr, sizeof(cbqtrace_buffer)); \
		cbqtrace_count = 0; \
	} \
}

#define	LOCK_TRACE()	crit_enter();
#define	UNLOCK_TRACE()	crit_exit();

#define	CBQTRACE(func, act, obj) {		\
	LOCK_TRACE();				\
	int *_p = &cbqtrace_ptr->count;	\
	*_p++ = ++cbqtrace_count;		\
	*_p++ = (int)(func);			\
	*_p++ = (int)(act);			\
	*_p++ = (int)(obj);			\
	if ((struct cbqtrace *)(void *)_p >= &cbqtrace_buffer[NCBQTRACE])\
		cbqtrace_ptr = cbqtrace_buffer; \
	else					\
		cbqtrace_ptr = (struct cbqtrace *)(void *)_p; \
	UNLOCK_TRACE();				\
	}
#else

/* If no tracing, define no-ops */
#define	CBQTRACEINIT()
#define	CBQTRACE(a, b, c)

#endif	/* !CBQ_TRACE */

#endif	/* _ALTQ_ALTQ_RMCLASS_DEBUG_H_ */
