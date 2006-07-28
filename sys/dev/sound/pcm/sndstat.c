/*
 * Copyright (c) 2001 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 * $FreeBSD: src/sys/dev/sound/pcm/sndstat.c,v 1.4.2.2 2002/04/22 15:49:36 cg Exp $
 * $DragonFly: src/sys/dev/sound/pcm/sndstat.c,v 1.9 2006/07/28 02:17:38 dillon Exp $
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>

SND_DECLARE_FILE("$DragonFly: src/sys/dev/sound/pcm/sndstat.c,v 1.9 2006/07/28 02:17:38 dillon Exp $");

#define	SS_TYPE_MODULE		0
#define	SS_TYPE_FIRST		1
#define	SS_TYPE_PCM		1
#define	SS_TYPE_MIDI		2
#define	SS_TYPE_SEQUENCER	3
#define	SS_TYPE_LAST		3

static d_open_t sndstat_open;
static d_close_t sndstat_close;
static d_read_t sndstat_read;

static struct dev_ops sndstat_ops = {
	{ "sndstat", SND_CDEV_MAJOR, 0 },
	.d_open =	sndstat_open,
	.d_close =	sndstat_close,
	.d_read =	sndstat_read,
};

struct sndstat_entry {
	SLIST_ENTRY(sndstat_entry) link;
	device_t dev;
	char *str;
	sndstat_handler handler;
	int type, unit;
};

static struct sbuf sndstat_sbuf;
static int sndstat_isopen = 0;
static int sndstat_bufptr;
static int sndstat_maxunit = -1;
static int sndstat_files = 0;

static SLIST_HEAD(, sndstat_entry) sndstat_devlist = SLIST_HEAD_INITIALIZER(none);

static int sndstat_verbose = 1;
#ifdef	USING_MUTEX
TUNABLE_INT("hw.snd.verbose", &sndstat_verbose);
#else
TUNABLE_INT_DECL("hw.snd.verbose", 1, sndstat_verbose);
#endif

static int sndstat_prepare(struct sbuf *s);

static int
sysctl_hw_sndverbose(SYSCTL_HANDLER_ARGS)
{
	int error, verbose;

	verbose = sndstat_verbose;
	error = sysctl_handle_int(oidp, &verbose, sizeof(verbose), req);
	if (error == 0 && req->newptr != NULL) {
		crit_enter();
		if (verbose < 0 || verbose > 3)
			error = EINVAL;
		else
			sndstat_verbose = verbose;
		crit_exit();
	}
	return error;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, verbose, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_sndverbose, "I", "");

static int
sndstat_open(struct dev_open_args *ap)
{
	int err;

	crit_enter();
	if (sndstat_isopen) {
		crit_exit();
		return EBUSY;
	}
	if (sbuf_new(&sndstat_sbuf, NULL, 4096, 0) == NULL) {
		crit_exit();
		return ENXIO;
	}
	sndstat_bufptr = 0;
	err = (sndstat_prepare(&sndstat_sbuf) > 0)? 0 : ENOMEM;
	if (!err)
		sndstat_isopen = 1;

	crit_exit();
	return err;
}

static int
sndstat_close(struct dev_close_args *ap)
{
	crit_enter();
	if (!sndstat_isopen) {
		crit_exit();
		return EBADF;
	}
	sbuf_delete(&sndstat_sbuf);
	sndstat_isopen = 0;

	crit_exit();
	return 0;
}

static int
sndstat_read(struct dev_read_args *ap)
{
	struct uio *uio = ap->a_uio;
	int l, err;

	crit_enter();
	if (!sndstat_isopen) {
		crit_exit();
		return EBADF;
	}
    	l = min(uio->uio_resid, sbuf_len(&sndstat_sbuf) - sndstat_bufptr);
	if (l > 0)
	    err = uiomove(sbuf_data(&sndstat_sbuf) + sndstat_bufptr, l, uio);
	else
	    err = 0;
	sndstat_bufptr += l;

	crit_exit();
	return err;
}

/************************************************************************/

static struct sndstat_entry *
sndstat_find(int type, int unit)
{
	struct sndstat_entry *ent;

	SLIST_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->type == type && ent->unit == unit)
			return ent;
	}

	return NULL;
}

int
sndstat_register(device_t dev, char *str, sndstat_handler handler)
{
	struct sndstat_entry *ent;
	const char *devtype;
	int type, unit;

	if (dev) {
		unit = device_get_unit(dev);
		devtype = device_get_name(dev);
		if (!strcmp(devtype, "pcm"))
			type = SS_TYPE_PCM;
		else if (!strcmp(devtype, "midi"))
			type = SS_TYPE_MIDI;
		else if (!strcmp(devtype, "sequencer"))
			type = SS_TYPE_SEQUENCER;
		else
			return EINVAL;
	} else {
		type = SS_TYPE_MODULE;
		unit = -1;
	}

	ent = malloc(sizeof *ent, M_DEVBUF, M_ZERO | M_WAITOK);
	if (!ent)
		return ENOSPC;

	ent->dev = dev;
	ent->str = str;
	ent->type = type;
	ent->unit = unit;
	ent->handler = handler;

	crit_enter();
	SLIST_INSERT_HEAD(&sndstat_devlist, ent, link);
	if (type == SS_TYPE_MODULE)
		sndstat_files++;
	sndstat_maxunit = (unit > sndstat_maxunit)? unit : sndstat_maxunit;
	crit_exit();

	return 0;
}

int
sndstat_registerfile(char *str)
{
	return sndstat_register(NULL, str, NULL);
}

int
sndstat_unregister(device_t dev)
{
	struct sndstat_entry *ent;

	crit_enter();
	SLIST_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->dev == dev) {
			SLIST_REMOVE(&sndstat_devlist, ent, sndstat_entry, link);
			free(ent, M_DEVBUF);
			crit_exit();

			return 0;
		}
	}
	crit_exit();

	return ENXIO;
}

int
sndstat_unregisterfile(char *str)
{
	struct sndstat_entry *ent;

	crit_enter();
	SLIST_FOREACH(ent, &sndstat_devlist, link) {
		if (ent->dev == NULL && ent->str == str) {
			SLIST_REMOVE(&sndstat_devlist, ent, sndstat_entry, link);
			free(ent, M_DEVBUF);
			sndstat_files--;
			crit_exit();

			return 0;
		}
	}
	crit_exit();

	return ENXIO;
}

/************************************************************************/

static int
sndstat_prepare(struct sbuf *s)
{
	struct sndstat_entry *ent;
    	int i, j;

	sbuf_printf(s, "FreeBSD Audio Driver (newpcm)\n");
	if (SLIST_EMPTY(&sndstat_devlist)) {
		sbuf_printf(s, "No devices installed.\n");
		sbuf_finish(s);
    		return sbuf_len(s);
	}

	sbuf_printf(s, "Installed devices:\n");

    	for (i = 0; i <= sndstat_maxunit; i++) {
		for (j = SS_TYPE_FIRST; j <= SS_TYPE_LAST; j++) {
			ent = sndstat_find(j, i);
			if (!ent)
				continue;
			sbuf_printf(s, "%s:", device_get_nameunit(ent->dev));
			sbuf_printf(s, " <%s>", device_get_desc(ent->dev));
			sbuf_printf(s, " %s", ent->str);
			if (ent->handler)
				ent->handler(s, ent->dev, sndstat_verbose);
			else
				sbuf_printf(s, " [no handler]");
			sbuf_printf(s, "\n");
		}
    	}

	if (sndstat_verbose >= 3 && sndstat_files > 0) {
		sbuf_printf(s, "\nFile Versions:\n");

		SLIST_FOREACH(ent, &sndstat_devlist, link) {
			if (ent->dev == NULL && ent->str != NULL)
				sbuf_printf(s, "%s\n", ent->str);
		}
	}

	sbuf_finish(s);
    	return sbuf_len(s);
}

static int
sndstat_init(void)
{
	dev_ops_add(&sndstat_ops, -1, SND_DEV_STATUS);
	make_dev(&sndstat_ops, SND_DEV_STATUS, 
		UID_ROOT, GID_WHEEL, 0444, "sndstat");
	return (0);
}

static int
sndstat_uninit(void)
{
	crit_enter();
	if (sndstat_isopen) {
		crit_exit();
		return EBUSY;
	}
	dev_ops_remove(&sndstat_ops, -1, SND_DEV_STATUS);
	crit_exit();
	return 0;
}

int
sndstat_busy(void)
{
	return (sndstat_isopen);
}

static void
sndstat_sysinit(void *p)
{
	sndstat_init();
}

static void
sndstat_sysuninit(void *p)
{
	sndstat_uninit();
}

SYSINIT(sndstat_sysinit, SI_SUB_DRIVERS, SI_ORDER_FIRST, sndstat_sysinit, NULL);
SYSUNINIT(sndstat_sysuninit, SI_SUB_DRIVERS, SI_ORDER_FIRST, sndstat_sysuninit, NULL);


