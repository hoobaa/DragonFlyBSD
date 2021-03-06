/*-
 * Copyright (c) 2002-2004 M. Warner Losh.
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
 * $FreeBSD: src/sys/dev/pccbb/pccbb_isa.c,v 1.4 2005/01/11 05:33:18 imp Exp $
 */

/*
 * Driver for ISA to PCMCIA bridges compliant with the Intel ExCA
 * specification.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <bus/isa/isavar.h>

#include <bus/pccard/pccardreg.h>
#include <bus/pccard/pccardvar.h>

#include <dev/pccard/exca/excareg.h>
#include <dev/pccard/exca/excavar.h>

#include <dev/pccard/pccbb/pccbbreg.h>
#include <dev/pccard/pccbb/pccbbvar.h>

#include "power_if.h"
#include "card_if.h"

/*****************************************************************************
 * Configurable parameters.
 *****************************************************************************/

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, pcic, CTLFLAG_RD, 0, "PCIC parameters");

static int isa_intr_mask = EXCA_INT_MASK_ALLOWED;
TUNABLE_INT("hw.cbb.intr_mask", &isa_intr_mask);
SYSCTL_INT(_hw_pcic, OID_AUTO, intr_mask, CTLFLAG_RD, &isa_intr_mask, 0,
    "Mask of allowable interrupts for this laptop.  The default is generally\n\
correct, but some laptops do not route all the IRQ pins to the bridge to\n\
save wires.  Sometimes you need a more restrictive mask because some of the\n\
hardware in your laptop may not have a driver so its IRQ might not be\n\
allocated.");

/*****************************************************************************
 * End of configurable parameters.
 *****************************************************************************/

#define	DPRINTF(x) do { if (cbb_debug) kprintf x; } while (0)
#define	DEVPRINTF(x) do { if (cbb_debug) device_printf x; } while (0)

static struct isa_pnp_id pcic_ids[] = {
	{EXCA_PNP_ACTIONTEC,		NULL},		/* AEI0218 */
	{EXCA_PNP_IBM3765,		NULL},		/* IBM3765 */
	{EXCA_PNP_82365,		NULL},		/* PNP0E00 */
	{EXCA_PNP_CL_PD6720,		NULL},		/* PNP0E01 */
	{EXCA_PNP_VLSI_82C146,		NULL},		/* PNP0E02 */
	{EXCA_PNP_82365_CARDBUS,	NULL},		/* PNP0E03 */
	{EXCA_PNP_SCM_SWAPBOX,		NULL},		/* SCM0469 */
	{0}
};

/************************************************************************/
/* Probe/Attach								*/
/************************************************************************/

#if 0
	struct resource *res;
	int rid;
	int i;

	/* A little bogus, but go ahead and get the irq for CSC events */
	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (res == NULL) {
		/*
		 * No IRQ specified, find one.  This can be due to the PnP
		 * data not specifying any IRQ, or the default kernel not
		 * assinging an IRQ.
		 */
		for (i = 0; i < 16; i++) {
			if (((1 << i) & isa_intr_mask) == 0)
				continue;
			res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, i, i,
			    1, RF_ACTIVE);
			if (res != NULL)
				break;
		}
		if (res == NULL)
			return (ENXIO);
		bus_release_resource(dev, SYS_RES_IRQ, rid, res);	
		bus_set_resource(dev, SYS_RES_IRQ, 0, i, 1);
	} else {
		bus_release_resource(dev, SYS_RES_IRQ, rid, res);
	}
	if (res == NULL) {
		device_printf(dev, "Cannot allocate mem\n");
		return (ENOMEM);
	}
#endif

static int
cbb_isa_activate(device_t dev)
{
	return (ENOMEM);
}

static void
cbb_isa_deactivate(device_t dev)
{
}

static int
cbb_isa_probe(device_t dev)
{
	int error;
	struct cbb_softc *sc = device_get_softc(dev);

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, pcic_ids);
	if (error != 0 && error != ENOENT)
		return (error);

	error = cbb_isa_activate(dev);
	if (error != 0)
		return (error);

	/* Check to make sure that we have actual hardware */
	error = exca_probe_slots(dev, &sc->exca[0], sc->bst, sc->bsh);
	cbb_isa_deactivate(dev);
	return (error);
}

static int
cbb_isa_attach(device_t dev)
{
	return (ENOMEM);
}

static device_method_t cbb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			cbb_isa_probe),
	DEVMETHOD(device_attach,		cbb_isa_attach),
	DEVMETHOD(device_detach,		cbb_detach),
	DEVMETHOD(device_shutdown,		cbb_shutdown),
	DEVMETHOD(device_suspend,		cbb_suspend),
	DEVMETHOD(device_resume,		cbb_resume),

	/* bus methods */
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		cbb_read_ivar),
	DEVMETHOD(bus_write_ivar,		cbb_write_ivar),
	DEVMETHOD(bus_alloc_resource,		cbb_alloc_resource),
	DEVMETHOD(bus_release_resource,		cbb_release_resource),
	DEVMETHOD(bus_activate_resource,	cbb_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	cbb_deactivate_resource),
	DEVMETHOD(bus_driver_added,		cbb_driver_added),
	DEVMETHOD(bus_child_detached,		cbb_child_detached),
	DEVMETHOD(bus_setup_intr,		cbb_setup_intr),
	DEVMETHOD(bus_teardown_intr,		cbb_teardown_intr),
	DEVMETHOD(bus_child_present,		cbb_child_present),

	/* 16-bit card interface */
	DEVMETHOD(card_set_res_flags,		cbb_pcic_set_res_flags),
	DEVMETHOD(card_set_memory_offset,	cbb_pcic_set_memory_offset),

	/* power interface */
	DEVMETHOD(power_enable_socket,		cbb_power_enable_socket),
	DEVMETHOD(power_disable_socket,		cbb_power_disable_socket),

	DEVMETHOD_END
};

static driver_t cbb_isa_driver = {
	"cbb",
	cbb_methods,
	sizeof(struct cbb_softc)
};

DRIVER_MODULE(cbb, isa, cbb_isa_driver, cbb_devclass, NULL, NULL);
MODULE_DEPEND(cbb, exca, 1, 1, 1);
