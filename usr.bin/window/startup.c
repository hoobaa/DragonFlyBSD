/*	@(#)startup.c	8.1 (Berkeley) 6/6/93	*/
/*	$NetBSD: startup.c,v 1.8 2009/04/14 08:50:06 lukem Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include "defs.h"
#include "var.h"
#include "char.h"
#include "local.h"

int
doconfig(void)
{
	char buf[100];
	const char *home;
	static char runcom[] = RUNCOM;

	if ((home = getenv("HOME")) == NULL)
		home = ".";
	(void) sprintf(buf, "%.*s/%s",
		(int)((sizeof buf - sizeof runcom) / sizeof (char) - 1),
		home, runcom);
	return dosource(buf);
}

/*
 * The default is two windows of equal size.
 */
void
dodefault(void)
{
	struct ww *w;
	int r = wwnrow / 2 - 1;

	if (openwin(1, r + 2, 0, wwnrow - r - 2, wwncol, default_nline,
	    (char *) 0, WWT_PTY, WWU_HASFRAME, default_shellfile,
	    default_shell) == 0)
		return;
	if ((w = openwin(0, 1, 0, r, wwncol, default_nline,
	    (char *) 0, WWT_PTY, WWU_HASFRAME, default_shellfile,
	    default_shell)) == NULL)
		return;
	wwprintf(w, "Escape character is %s.\r\n", unctrl(escapec));
}

void
setvars(void)
{
	/* try to use a random ordering to balance the tree */
	(void) var_setnum("nrow", wwnrow);
	(void) var_setnum("ncol", wwncol);
	(void) var_setnum("baud", wwbaud);
	(void) var_setnum("m_rev", WWM_REV);
	(void) var_setnum("m_blk", WWM_BLK);
	(void) var_setnum("m_ul", WWM_UL);
	(void) var_setnum("m_grp", WWM_GRP);
	(void) var_setnum("m_dim", WWM_DIM);
	(void) var_setnum("m_usr", WWM_USR);
	(void) var_setstr("term", wwterm);
	(void) var_setnum("modes", wwavailmodes);
}
