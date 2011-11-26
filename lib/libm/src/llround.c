/*
 * Written by Matthias Drochner <drochner@NetBSD.org>.
 * Public domain.
 *
 * $NetBSD: llround.c,v 1.2 2004/10/13 15:18:32 drochner Exp $
 * $DragonFly: src/lib/libm/src/llround.c,v 1.1 2005/07/26 21:15:20 joerg Exp $
 */

#define LROUNDNAME llround
#define RESTYPE long long int
#define RESTYPE_MIN LLONG_MIN
#define RESTYPE_MAX LLONG_MAX

#include "lround.c"
