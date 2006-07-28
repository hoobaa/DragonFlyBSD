/* $FreeBSD: src/sys/dev/iir/iir_ctrl.c,v 1.2.2.4 2002/05/05 08:18:12 asmodai Exp $ */
/* $DragonFly: src/sys/dev/raid/iir/iir_ctrl.c,v 1.10 2006/07/28 02:17:37 dillon Exp $ */
/*
 *       Copyright (c) 2000-01 Intel Corporation
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * iir_ctrl.c: Control functions and /dev entry points for /dev/iir*
 *
 * Written by: Achim Leubner <achim.leubner@intel.com>
 * Fixes/Additions: Boji Tony Kannanthanam <boji.t.kannanthanam@intel.com>
 *
 * TODO:     
 */

#ident "$Id: iir_ctrl.c 1.2 2001/07/18 11:17:22 achim Exp $"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/thread2.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include "iir.h"

/* Entry points and other prototypes */
static struct gdt_softc *gdt_minor2softc(int minor_no);

static d_open_t		iir_open;
static d_close_t	iir_close;
static d_write_t	iir_write;
static d_read_t		iir_read;
static d_ioctl_t	iir_ioctl;

#define CDEV_MAJOR          IIR_CDEV_MAJOR

/* Normally, this is a static structure.  But we need it in pci/iir_pci.c */
static struct dev_ops iir_ops = {
	{ "iir", CDEV_MAJOR, 0 },
        .d_open =	iir_open,
        .d_close =	iir_close,
        .d_read =	iir_read,
        .d_write =	iir_write,
        .d_ioctl =	iir_ioctl,
};

static int iir_devsw_installed = 0;
#ifndef SDEV_PER_HBA
static int sdev_made = 0;
#endif
extern int gdt_cnt;
extern char ostype[];
extern char osrelease[];
extern gdt_statist_t gdt_stat;

/*
 * Given a controller number,
 * make a special device and return the dev_t
 */
dev_t 
gdt_make_dev(int unit)
{
    dev_t dev;

#ifdef SDEV_PER_HBA
    dev = make_dev(&iir_ops, hba2minor(unit), UID_ROOT, GID_OPERATOR,
                   S_IRUSR | S_IWUSR, "iir%d", unit);
#else
    if (sdev_made)
        return (0);
    dev = make_dev(&iir_ops, 0, UID_ROOT, GID_OPERATOR,
                   S_IRUSR | S_IWUSR, "iir");
    sdev_made = 1;
#endif
    reference_dev(dev);
    return (dev);
}

void
gdt_destroy_dev(dev_t dev)
{
    if (dev != NULL)
        destroy_dev(dev);
}

/*
 * Given a minor device number,
 * return the pointer to its softc structure
 */
static struct gdt_softc *
gdt_minor2softc(int minor_no)
{
    struct gdt_softc *gdt;
    int hanum;

#ifdef SDEV_PER_HBA
    hanum = minor2hba(minor_no);
#else
    hanum = minor_no;
#endif

    for (gdt = TAILQ_FIRST(&gdt_softcs);
         gdt != NULL && gdt->sc_hanum != hanum;
         gdt = TAILQ_NEXT(gdt, links));

    return (gdt);
}

static int
iir_open(struct dev_open_args *ap)
{
    GDT_DPRINTF(GDT_D_DEBUG, ("iir_open()\n"));

#ifdef SDEV_PER_HBA
    int minor_no;
    struct gdt_softc *gdt;

    minor_no = minor(dev);
    gdt = gdt_minor2softc(minor_no);
    if (gdt == NULL)
        return (ENXIO);
#endif
                
    return (0);
}

static int
iir_close(struct dev_close_args *ap)
{
    GDT_DPRINTF(GDT_D_DEBUG, ("iir_close()\n"));
                
#ifdef SDEV_PER_HBA
    int minor_no;
    struct gdt_softc *gdt;

    minor_no = minor(dev);
    gdt = gdt_minor2softc(minor_no);
    if (gdt == NULL)
        return (ENXIO);
#endif

    return (0);
}

static int
iir_write(struct dev_write_args *ap)
{
    GDT_DPRINTF(GDT_D_DEBUG, ("iir_write()\n"));
                
#ifdef SDEV_PER_HBA
    int minor_no;
    struct gdt_softc *gdt;

    minor_no = minor(dev);
    gdt = gdt_minor2softc(minor_no);
    if (gdt == NULL)
        return (ENXIO);
#endif

    return (0);
}

static int
iir_read(struct dev_read_args *ap)
{
    GDT_DPRINTF(GDT_D_DEBUG, ("iir_read()\n"));
                
#ifdef SDEV_PER_HBA
    int minor_no;
    struct gdt_softc *gdt;

    minor_no = minor(dev);
    gdt = gdt_minor2softc(minor_no);
    if (gdt == NULL)
        return (ENXIO);
#endif

    return (0);
}

/**
 * This is the control syscall interface.
 * It should be binary compatible with UnixWare,
 * if not totally syntatically so.
 */

static int
iir_ioctl(struct dev_ioctl_args *ap)
{
    GDT_DPRINTF(GDT_D_DEBUG, ("iir_ioctl() cmd 0x%lx\n",cmd));

#ifdef SDEV_PER_HBA
    int minor_no;
    struct gdt_softc *gdt;

    minor_no = minor(dev);
    gdt = gdt_minor2softc(minor_no);
    if (gdt == NULL)
        return (ENXIO);
#endif
    ++gdt_stat.io_count_act;
    if (gdt_stat.io_count_act > gdt_stat.io_count_max)
        gdt_stat.io_count_max = gdt_stat.io_count_act;

    switch (ap->a_cmd) {
      case GDT_IOCTL_GENERAL:
        {
            gdt_ucmd_t *ucmd;
            struct gdt_softc *gdt;

            ucmd = (gdt_ucmd_t *)ap->a_data;
            gdt = gdt_minor2softc(ucmd->io_node);
            if (gdt == NULL)
                return (ENXIO);
	    crit_enter();
            TAILQ_INSERT_TAIL(&gdt->sc_ucmd_queue, ucmd, links);
            ucmd->complete_flag = FALSE;
	    crit_exit();
            gdt_next(gdt);
            if (!ucmd->complete_flag)
                (void) tsleep((void *)ucmd, PCATCH, "iirucw", 0);
            break;
        }

      case GDT_IOCTL_DRVERS:
        *(int *)ap->a_data = 
            (IIR_DRIVER_VERSION << 8) | IIR_DRIVER_SUBVERSION;
        break;

      case GDT_IOCTL_CTRTYPE:
        {
            gdt_ctrt_t *p;
            struct gdt_softc *gdt; 
            
            p = (gdt_ctrt_t *)ap->a_data;
            gdt = gdt_minor2softc(p->io_node);
            if (gdt == NULL)
                return (ENXIO);
            p->oem_id = 0x8000;
            p->type = 0xfd;
            p->info = (gdt->sc_bus << 8) | (gdt->sc_slot << 3);
            p->ext_type = 0x6000 | gdt->sc_subdevice;
            p->device_id = gdt->sc_device;
            p->sub_device_id = gdt->sc_subdevice;
            break;
        }

      case GDT_IOCTL_OSVERS:
        {
            gdt_osv_t *p;

            p = (gdt_osv_t *)ap->a_data;
            p->oscode = 10;
            p->version = osrelease[0] - '0';
            if (osrelease[1] == '.')
                p->subversion = osrelease[2] - '0';
            else
                p->subversion = 0;
            if (osrelease[3] == '.')
                p->revision = osrelease[4] - '0';
            else
                p->revision = 0;
            strcpy(p->name, ostype);
            break;
        }

      case GDT_IOCTL_CTRCNT:
        *(int *)ap->a_data = gdt_cnt;
        break;

      case GDT_IOCTL_EVENT:
        {
            gdt_event_t *p;

            p = (gdt_event_t *)ap->a_data;
            if (p->erase == 0xff) {
                if (p->dvr.event_source == GDT_ES_TEST)
                    p->dvr.event_data.size = sizeof(p->dvr.event_data.eu.test);
                else if (p->dvr.event_source == GDT_ES_DRIVER)
                    p->dvr.event_data.size= sizeof(p->dvr.event_data.eu.driver);
                else if (p->dvr.event_source == GDT_ES_SYNC)
                    p->dvr.event_data.size = sizeof(p->dvr.event_data.eu.sync);
                else
                    p->dvr.event_data.size = sizeof(p->dvr.event_data.eu.async);
		crit_enter();
                gdt_store_event(p->dvr.event_source, p->dvr.event_idx,
                                &p->dvr.event_data);
		crit_exit();
            } else if (p->erase == 0xfe) {
		crit_enter();
                gdt_clear_events();
		crit_exit();
            } else if (p->erase == 0) {
                p->handle = gdt_read_event(p->handle, &p->dvr);
            } else {
                gdt_readapp_event((u_int8_t)p->erase, &p->dvr);
            }
            break;
        }
        
      case GDT_IOCTL_STATIST:
        {
            gdt_statist_t *p;
            
            p = (gdt_statist_t *)ap->a_data;
            bcopy(&gdt_stat, p, sizeof(gdt_statist_t));
            break;
        }

      default:
        break;
    }

    --gdt_stat.io_count_act;
    return (0);
}

static void
iir_drvinit(void *unused)
{
    GDT_DPRINTF(GDT_D_DEBUG, ("iir_drvinit()\n"));
                
    if (!iir_devsw_installed) {
        /* Add the I/O (data) channel */
        dev_ops_add(&iir_ops, 0, 0);
        iir_devsw_installed = 1;
    }
}

SYSINIT(iir_dev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE + CDEV_MAJOR, iir_drvinit, NULL)
