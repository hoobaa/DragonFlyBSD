#
# acpibin - Binary ACPI table utility
#

UTILDIR=${.CURDIR}/../../../sys/contrib/dev/acpica/source/tools/acpibin

.PATH:	${UTILDIR}

PROG=	acpibin
NOMAN=

SRCS=	\
	abcompare.c \
	abmain.c

SRCS+=	\
	utalloc.c \
	utbuffer.c \
	utcache.c \
	utdecode.c \
	utexcep.c \
	utglobal.c \
	utlock.c \
	utmath.c \
	utmisc.c \
	utmutex.c \
	utstate.c \
	utstring.c \
	utxferror.c \
	osunixxf.c \
	getopt.c

CFLAGS+= \
	-DACPI_BIN_APP \
	-I${UTILDIR}

.include <bsd.prog.mk>
