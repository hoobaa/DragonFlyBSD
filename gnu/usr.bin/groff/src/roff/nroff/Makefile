# $FreeBSD: src/gnu/usr.bin/groff/src/roff/nroff/Makefile,v 1.1 2001/04/17 12:37:13 ru Exp $
# $DragonFly: src/gnu/usr.bin/groff/src/roff/nroff/Makefile,v 1.3 2005/09/01 20:47:14 asmodai Exp $

PATCHES!=       echo ${.CURDIR}/patches/*.patch
CONTRIBDIR=     ${DIST_DIR}

SRCS=		${PATCHES}
SCRIPTS=	nroff
MAN=		nroff.1
CLEANFILES=	${SCRIPTS} ${MAN}

.include <bsd.prog.mk>
