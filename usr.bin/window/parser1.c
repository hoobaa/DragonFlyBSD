/*	@(#)parser1.c	8.1 (Berkeley) 6/6/93	*/
/*	$NetBSD: parser1.c,v 1.6 2003/08/07 11:17:28 agc Exp $	*/

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

#include "defs.h"
#include "parser.h"

void
p_start(void)
{
	char flag = 1;

	(void) s_gettok();
	for (;;) {
		p_statementlist(flag);
		if (token == T_EOF || p_abort())
			break;
		flag = 0;
		p_synerror();
		while (token != T_EOL && token != T_EOF) {
			if (token == T_STR)
				str_free(token_str);
			(void) s_gettok();
		}
		if (token == T_EOL)
			(void) s_gettok();
		p_clearerr();
	}
}

void
p_statementlist(char flag)
{
	for (; p_statement(flag) >= 0; p_clearerr())
		;
}

int
p_statement(char flag)
{
	switch (token) {
	case T_EOL:
		(void) s_gettok();
		return 0;
	case T_IF:
		return p_if(flag);
	default:
		return p_expression(flag);
	}
}

int
p_if(char flag)
{
	struct value t;
	char true = 0;

top:
	(void) s_gettok();

	if (p_expr(&t, flag) < 0) {
		p_synerror();
		return -1;
	}
	switch (t.v_type) {
	case V_NUM:
		true = !true && t.v_num != 0;
		break;
	case V_STR:
		p_error("if: Numeric value required.");
		str_free(t.v_str);
	case V_ERR:
		flag = 0;
		break;
	}

	if (token != T_THEN) {
		p_synerror();
		return -1;
	}

	(void) s_gettok();
	p_statementlist(flag && true);
	if (p_erred())
		return -1;

	if (token == T_ELSIF)
		goto top;

	if (token == T_ELSE) {
		(void) s_gettok();
		p_statementlist(flag && !true);
		if (p_erred())
			return -1;
	}

	if (token == T_ENDIF) {
		(void) s_gettok();
		return 0;
	}

	p_synerror();
	return -1;
}

int
p_expression(char flag)
{
	struct value t;
	char *cmd;

	switch (token) {
	case T_NUM:
		t.v_type = V_NUM;
		t.v_num = token_num;
		(void) s_gettok();
		break;
	case T_STR:
		t.v_type = V_STR;
		t.v_str = token_str;
		(void) s_gettok();
		break;
	default:
		if (p_expr(&t, flag) < 0)
			return -1;
		if (token == T_EOF) {
			val_free(t);
			return 0;
		}
	}
	if (token != T_ASSIGN && p_convstr(&t) < 0)
		return -1;
	cmd = t.v_type == V_STR ? t.v_str : 0;
	if ((*(token == T_ASSIGN ? p_assign : p_function))(cmd, &t, flag) < 0) {
		if (cmd)
			str_free(cmd);
		return -1;
	}
	if (cmd)
		str_free(cmd);
	val_free(t);
	if (token == T_EOL)
		(void) s_gettok();
	else if (token != T_EOF) {
		p_synerror();
		return -1;
	}
	return 0;
}

int
p_convstr(struct value *v)
{
	if (v->v_type != V_NUM)
		return 0;
	if ((v->v_str = str_itoa(v->v_num)) == 0) {
		p_memerror();
		v->v_type = V_ERR;
		return -1;
	}
	v->v_type = V_STR;
	return 0;
}

void
p_synerror(void)
{
	if (!cx.x_synerred) {
		cx.x_synerred = cx.x_erred = 1;
		error("Syntax error.");
	}
}

void
p_error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	if (!cx.x_erred) {
		cx.x_erred = 1;
		verror(msg, ap);
	}
	va_end(ap);
}

void
p_memerror(void)
{
	cx.x_erred = cx.x_abort = 1;
	error("Out of memory.");
}
