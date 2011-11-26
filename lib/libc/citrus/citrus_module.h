/* $NetBSD: src/lib/libc/citrus/citrus_module.h,v 1.1 2002/03/17 22:14:20 tshiozak Exp $ */
/* $DragonFly: src/lib/libc/citrus/citrus_module.h,v 1.3 2008/04/10 10:21:01 hasso Exp $ */

/*-
 * Copyright (c)2002 Citrus Project,
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
 */

#ifndef _CITRUS_MODULE_H_
#define _CITRUS_MODULE_H_

#include <sys/linker_set.h>

struct citrus_metadata {
    const char *module_name;
    const char *interface_name;
    void *module_ops;
};

#ifdef __PIC__
#define CITRUS_MODULE(name, interface, ops)
#else

#define	CITRUS_MODULE(name, interface, ops)				\
static const char __citrus_module_ ## name ## _ ## interface ## _str[] = \
    #name; \
static struct citrus_metadata						\
    __citrus_module_ ## name ## _ ## interface = { 			\
	    __citrus_module_ ## name ## _ ##interface ## _str,		\
	    #interface, ops						\
};									\
__weak_reference(__citrus_module_ ## name ## _ ## interface ## _str,	\
		 _citrus_module_ ## name);				\
DATA_SET(citrus_set, __citrus_module_ ## name ## _ ## interface);
#endif

typedef struct _citrus_module_rec *_citrus_module_t;

__BEGIN_DECLS
void *_citrus_find_getops(_citrus_module_t __restrict,
			  const char * __restrict, const char * __restrict);
int _citrus_load_module(_citrus_module_t * __restrict,
			const char * __restrict);
void _citrus_unload_module(_citrus_module_t);
__END_DECLS

#endif
