/* i915_dma.c -- DMA support for the I915 -*- linux-c -*-
 */
/*-
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * $FreeBSD: src/sys/dev/drm2/i915/i915_dma.c,v 1.1 2012/05/22 11:07:44 kib Exp $
 */

#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "intel_drv.h"
#include "intel_ringbuffer.h"

extern struct drm_i915_private *i915_mch_dev;

extern void i915_pineview_get_mem_freq(struct drm_device *dev);
extern void i915_ironlake_get_mem_freq(struct drm_device *dev);
static int i915_driver_unload_int(struct drm_device *dev, bool locked);

static void i915_write_hws_pga(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 addr;

	addr = dev_priv->status_page_dmah->busaddr;
	if (INTEL_INFO(dev)->gen >= 4)
		addr |= (dev_priv->status_page_dmah->busaddr >> 28) & 0xf0;
	I915_WRITE(HWS_PGA, addr);
}

/**
 * Sets up the hardware status page for devices that need a physical address
 * in the register.
 */
static int i915_init_phys_hws(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = LP_RING(dev_priv);

	/*
	 * Program Hardware Status Page
	 * XXXKIB Keep 4GB limit for allocation for now.  This method
	 * of allocation is used on <= 965 hardware, that has several
	 * erratas regarding the use of physical memory > 4 GB.
	 */
	DRM_UNLOCK(dev);
	dev_priv->status_page_dmah =
		drm_pci_alloc(dev, PAGE_SIZE, PAGE_SIZE, 0xffffffff);
	DRM_LOCK(dev);
	if (!dev_priv->status_page_dmah) {
		DRM_ERROR("Can not allocate hardware status page\n");
		return -ENOMEM;
	}
	ring->status_page.page_addr = dev_priv->hw_status_page =
	    dev_priv->status_page_dmah->vaddr;
	dev_priv->dma_status_page = dev_priv->status_page_dmah->busaddr;

	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);

	i915_write_hws_pga(dev);
	DRM_DEBUG("Enabled hardware status page, phys %jx\n",
	    (uintmax_t)dev_priv->dma_status_page);
	return 0;
}

/**
 * Frees the hardware status page, whether it's a physical address or a virtual
 * address set up by the X Server.
 */
static void i915_free_hws(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = LP_RING(dev_priv);

	if (dev_priv->status_page_dmah) {
		drm_pci_free(dev, dev_priv->status_page_dmah);
		dev_priv->status_page_dmah = NULL;
	}

	if (dev_priv->status_gfx_addr) {
		dev_priv->status_gfx_addr = 0;
		ring->status_page.gfx_addr = 0;
		drm_core_ioremapfree(&dev_priv->hws_map, dev);
	}

	/* Need to rewrite hardware status page */
	I915_WRITE(HWS_PGA, 0x1ffff000);
}

void i915_kernel_lost_context(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = LP_RING(dev_priv);

	/*
	 * We should never lose context on the ring with modesetting
	 * as we don't expose it to userspace
	 */
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	ring->head = I915_READ_HEAD(ring) & HEAD_ADDR;
	ring->tail = I915_READ_TAIL(ring) & TAIL_ADDR;
	ring->space = ring->head - (ring->tail + 8);
	if (ring->space < 0)
		ring->space += ring->size;

#if 1
	KIB_NOTYET();
#else
	if (!dev->primary->master)
		return;
#endif

	if (ring->head == ring->tail && dev_priv->sarea_priv)
		dev_priv->sarea_priv->perf_boxes |= I915_BOX_RING_EMPTY;
}

static int i915_dma_cleanup(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;


	/* Make sure interrupts are disabled here because the uninstall ioctl
	 * may not have been called from userspace and after dev_private
	 * is freed, it's too late.
	 */
	if (dev->irq_enabled)
		drm_irq_uninstall(dev);

	for (i = 0; i < I915_NUM_RINGS; i++)
		intel_cleanup_ring_buffer(&dev_priv->ring[i]);

	/* Clear the HWS virtual address at teardown */
	if (I915_NEED_GFX_HWS(dev))
		i915_free_hws(dev);

	return 0;
}

static int i915_initialize(struct drm_device * dev, drm_i915_init_t * init)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	dev_priv->sarea = drm_getsarea(dev);
	if (!dev_priv->sarea) {
		DRM_ERROR("can not find sarea!\n");
		i915_dma_cleanup(dev);
		return -EINVAL;
	}

	dev_priv->sarea_priv = (drm_i915_sarea_t *)
	    ((u8 *) dev_priv->sarea->virtual + init->sarea_priv_offset);

	if (init->ring_size != 0) {
		if (LP_RING(dev_priv)->obj != NULL) {
			i915_dma_cleanup(dev);
			DRM_ERROR("Client tried to initialize ringbuffer in "
				  "GEM mode\n");
			return -EINVAL;
		}

		ret = intel_render_ring_init_dri(dev,
						 init->ring_start,
						 init->ring_size);
		if (ret) {
			i915_dma_cleanup(dev);
			return ret;
		}
	}

	dev_priv->cpp = init->cpp;
	dev_priv->back_offset = init->back_offset;
	dev_priv->front_offset = init->front_offset;
	dev_priv->current_page = 0;
	dev_priv->sarea_priv->pf_current_page = 0;

	/* Allow hardware batchbuffers unless told otherwise.
	 */
	dev_priv->allow_batchbuffer = 1;

	return 0;
}

static int i915_dma_resume(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	struct intel_ring_buffer *ring = LP_RING(dev_priv);

	DRM_DEBUG("\n");

	if (ring->map.handle == NULL) {
		DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
		return -ENOMEM;
	}

	/* Program Hardware Status Page */
	if (!ring->status_page.page_addr) {
		DRM_ERROR("Can not find hardware status page\n");
		return -EINVAL;
	}
	DRM_DEBUG("hw status page @ %p\n", ring->status_page.page_addr);
	if (ring->status_page.gfx_addr != 0)
		intel_ring_setup_status_page(ring);
	else
		i915_write_hws_pga(dev);

	DRM_DEBUG("Enabled hardware status page\n");

	return 0;
}

static int i915_dma_init(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_init_t *init = data;
	int retcode = 0;

	switch (init->func) {
	case I915_INIT_DMA:
		retcode = i915_initialize(dev, init);
		break;
	case I915_CLEANUP_DMA:
		retcode = i915_dma_cleanup(dev);
		break;
	case I915_RESUME_DMA:
		retcode = i915_dma_resume(dev);
		break;
	default:
		retcode = -EINVAL;
		break;
	}

	return retcode;
}

/* Implement basically the same security restrictions as hardware does
 * for MI_BATCH_NON_SECURE.  These can be made stricter at any time.
 *
 * Most of the calculations below involve calculating the size of a
 * particular instruction.  It's important to get the size right as
 * that tells us where the next instruction to check is.  Any illegal
 * instruction detected will be given a size of zero, which is a
 * signal to abort the rest of the buffer.
 */
static int do_validate_cmd(int cmd)
{
	switch (((cmd >> 29) & 0x7)) {
	case 0x0:
		switch ((cmd >> 23) & 0x3f) {
		case 0x0:
			return 1;	/* MI_NOOP */
		case 0x4:
			return 1;	/* MI_FLUSH */
		default:
			return 0;	/* disallow everything else */
		}
		break;
	case 0x1:
		return 0;	/* reserved */
	case 0x2:
		return (cmd & 0xff) + 2;	/* 2d commands */
	case 0x3:
		if (((cmd >> 24) & 0x1f) <= 0x18)
			return 1;

		switch ((cmd >> 24) & 0x1f) {
		case 0x1c:
			return 1;
		case 0x1d:
			switch ((cmd >> 16) & 0xff) {
			case 0x3:
				return (cmd & 0x1f) + 2;
			case 0x4:
				return (cmd & 0xf) + 2;
			default:
				return (cmd & 0xffff) + 2;
			}
		case 0x1e:
			if (cmd & (1 << 23))
				return (cmd & 0xffff) + 1;
			else
				return 1;
		case 0x1f:
			if ((cmd & (1 << 23)) == 0)	/* inline vertices */
				return (cmd & 0x1ffff) + 2;
			else if (cmd & (1 << 17))	/* indirect random */
				if ((cmd & 0xffff) == 0)
					return 0;	/* unknown length, too hard */
				else
					return (((cmd & 0xffff) + 1) / 2) + 1;
			else
				return 2;	/* indirect sequential */
		default:
			return 0;
		}
	default:
		return 0;
	}

	return 0;
}

static int validate_cmd(int cmd)
{
	int ret = do_validate_cmd(cmd);

/*	printk("validate_cmd( %x ): %d\n", cmd, ret); */

	return ret;
}

static int i915_emit_cmds(struct drm_device *dev, int __user *buffer,
			  int dwords)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i, ret;

	if ((dwords+1) * sizeof(int) >= LP_RING(dev_priv)->size - 8)
		return -EINVAL;

	ret = BEGIN_LP_RING((dwords+1)&~1);
	if (ret)
		return ret;

	for (i = 0; i < dwords;) {
		int cmd, sz;

		if (DRM_COPY_FROM_USER_UNCHECKED(&cmd, &buffer[i], sizeof(cmd)))
			return -EINVAL;

		if ((sz = validate_cmd(cmd)) == 0 || i + sz > dwords)
			return -EINVAL;

		OUT_RING(cmd);

		while (++i, --sz) {
			if (DRM_COPY_FROM_USER_UNCHECKED(&cmd, &buffer[i],
							 sizeof(cmd))) {
				return -EINVAL;
			}
			OUT_RING(cmd);
		}
	}

	if (dwords & 1)
		OUT_RING(0);

	ADVANCE_LP_RING();

	return 0;
}

int i915_emit_box(struct drm_device * dev,
		  struct drm_clip_rect *boxes,
		  int i, int DR1, int DR4)
{
	struct drm_clip_rect box;

	if (DRM_COPY_FROM_USER_UNCHECKED(&box, &boxes[i], sizeof(box))) {
		return -EFAULT;
	}

	return (i915_emit_box_p(dev, &box, DR1, DR4));
}

int
i915_emit_box_p(struct drm_device *dev, struct drm_clip_rect *box,
    int DR1, int DR4)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (box->y2 <= box->y1 || box->x2 <= box->x1 || box->y2 <= 0 ||
	    box->x2 <= 0) {
		DRM_ERROR("Bad box %d,%d..%d,%d\n",
			  box->x1, box->y1, box->x2, box->y2);
		return -EINVAL;
	}

	if (INTEL_INFO(dev)->gen >= 4) {
		ret = BEGIN_LP_RING(4);
		if (ret != 0)
			return (ret);

		OUT_RING(GFX_OP_DRAWRECT_INFO_I965);
		OUT_RING((box->x1 & 0xffff) | (box->y1 << 16));
		OUT_RING(((box->x2 - 1) & 0xffff) | ((box->y2 - 1) << 16));
		OUT_RING(DR4);
	} else {
		ret = BEGIN_LP_RING(6);
		if (ret != 0)
			return (ret);

		OUT_RING(GFX_OP_DRAWRECT_INFO);
		OUT_RING(DR1);
		OUT_RING((box->x1 & 0xffff) | (box->y1 << 16));
		OUT_RING(((box->x2 - 1) & 0xffff) | ((box->y2 - 1) << 16));
		OUT_RING(DR4);
		OUT_RING(0);
	}
	ADVANCE_LP_RING();

	return 0;
}

/* XXX: Emitting the counter should really be moved to part of the IRQ
 * emit. For now, do it in both places:
 */

static void i915_emit_breadcrumb(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (++dev_priv->counter > 0x7FFFFFFFUL)
		dev_priv->counter = 0;
	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->last_enqueue = dev_priv->counter;

	if (BEGIN_LP_RING(4) == 0) {
		OUT_RING(MI_STORE_DWORD_INDEX);
		OUT_RING(I915_BREADCRUMB_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
		OUT_RING(dev_priv->counter);
		OUT_RING(0);
		ADVANCE_LP_RING();
	}
}

static int i915_dispatch_cmdbuffer(struct drm_device * dev,
    drm_i915_cmdbuffer_t * cmd, struct drm_clip_rect *cliprects, void *cmdbuf)
{
	int nbox = cmd->num_cliprects;
	int i = 0, count, ret;

	if (cmd->sz & 0x3) {
		DRM_ERROR("alignment\n");
		return -EINVAL;
	}

	i915_kernel_lost_context(dev);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			ret = i915_emit_box_p(dev, &cmd->cliprects[i],
			    cmd->DR1, cmd->DR4);
			if (ret)
				return ret;
		}

		ret = i915_emit_cmds(dev, cmdbuf, cmd->sz / 4);
		if (ret)
			return ret;
	}

	i915_emit_breadcrumb(dev);
	return 0;
}

static int
i915_dispatch_batchbuffer(struct drm_device * dev,
    drm_i915_batchbuffer_t * batch, struct drm_clip_rect *cliprects)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int nbox = batch->num_cliprects;
	int i, count, ret;

	if ((batch->start | batch->used) & 0x7) {
		DRM_ERROR("alignment\n");
		return -EINVAL;
	}

	i915_kernel_lost_context(dev);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			int ret = i915_emit_box_p(dev, &cliprects[i],
			    batch->DR1, batch->DR4);
			if (ret)
				return ret;
		}

		if (!IS_I830(dev) && !IS_845G(dev)) {
			ret = BEGIN_LP_RING(2);
			if (ret != 0)
				return (ret);

			if (INTEL_INFO(dev)->gen >= 4) {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6) |
				    MI_BATCH_NON_SECURE_I965);
				OUT_RING(batch->start);
			} else {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
				OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			}
		} else {
			ret = BEGIN_LP_RING(4);
			if (ret != 0)
				return (ret);

			OUT_RING(MI_BATCH_BUFFER);
			OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			OUT_RING(batch->start + batch->used - 4);
			OUT_RING(0);
		}
		ADVANCE_LP_RING();
	}

	i915_emit_breadcrumb(dev);

	return 0;
}

static int i915_dispatch_flip(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (!dev_priv->sarea_priv)
		return -EINVAL;

	DRM_DEBUG("%s: page=%d pfCurrentPage=%d\n",
		  __func__,
		  dev_priv->current_page,
		  dev_priv->sarea_priv->pf_current_page);

	i915_kernel_lost_context(dev);

	ret = BEGIN_LP_RING(10);
	if (ret)
		return ret;
	OUT_RING(MI_FLUSH | MI_READ_FLUSH);
	OUT_RING(0);

	OUT_RING(CMD_OP_DISPLAYBUFFER_INFO | ASYNC_FLIP);
	OUT_RING(0);
	if (dev_priv->current_page == 0) {
		OUT_RING(dev_priv->back_offset);
		dev_priv->current_page = 1;
	} else {
		OUT_RING(dev_priv->front_offset);
		dev_priv->current_page = 0;
	}
	OUT_RING(0);

	OUT_RING(MI_WAIT_FOR_EVENT | MI_WAIT_FOR_PLANE_A_FLIP);
	OUT_RING(0);

	ADVANCE_LP_RING();

	if (++dev_priv->counter > 0x7FFFFFFFUL)
		dev_priv->counter = 0;
	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->last_enqueue = dev_priv->counter;

	if (BEGIN_LP_RING(4) == 0) {
		OUT_RING(MI_STORE_DWORD_INDEX);
		OUT_RING(I915_BREADCRUMB_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
		OUT_RING(dev_priv->counter);
		OUT_RING(0);
		ADVANCE_LP_RING();
	}

	dev_priv->sarea_priv->pf_current_page = dev_priv->current_page;
	return 0;
}

static int
i915_quiescent(struct drm_device *dev)
{
	struct intel_ring_buffer *ring = LP_RING(dev->dev_private);

	i915_kernel_lost_context(dev);
	return (intel_wait_ring_idle(ring));
}

static int
i915_flush_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	int ret;

	RING_LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_LOCK(dev);
	ret = i915_quiescent(dev);
	DRM_UNLOCK(dev);

	return (ret);
}

static int i915_batchbuffer(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	drm_i915_sarea_t *sarea_priv;
	drm_i915_batchbuffer_t *batch = data;
	struct drm_clip_rect *cliprects;
	size_t cliplen;
	int ret;

	if (!dev_priv->allow_batchbuffer) {
		DRM_ERROR("Batchbuffer ioctl disabled\n");
		return -EINVAL;
	}
	DRM_UNLOCK(dev);

	DRM_DEBUG("i915 batchbuffer, start %x used %d cliprects %d\n",
		  batch->start, batch->used, batch->num_cliprects);

	cliplen = batch->num_cliprects * sizeof(struct drm_clip_rect);
	if (batch->num_cliprects < 0)
		return -EFAULT;
	if (batch->num_cliprects != 0) {
		cliprects = kmalloc(batch->num_cliprects *
		    sizeof(struct drm_clip_rect), DRM_MEM_DMA,
		    M_WAITOK | M_ZERO);

		ret = -copyin(batch->cliprects, cliprects,
		    batch->num_cliprects * sizeof(struct drm_clip_rect));
		if (ret != 0) {
			DRM_LOCK(dev);
			goto fail_free;
		}
	} else
		cliprects = NULL;

	DRM_LOCK(dev);
	RING_LOCK_TEST_WITH_RETURN(dev, file_priv);
	ret = i915_dispatch_batchbuffer(dev, batch, cliprects);

	sarea_priv = (drm_i915_sarea_t *)dev_priv->sarea_priv;
	if (sarea_priv)
		sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

fail_free:
	drm_free(cliprects, DRM_MEM_DMA);
	return ret;
}

static int i915_cmdbuffer(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	drm_i915_sarea_t *sarea_priv;
	drm_i915_cmdbuffer_t *cmdbuf = data;
	struct drm_clip_rect *cliprects = NULL;
	void *batch_data;
	int ret;

	DRM_DEBUG("i915 cmdbuffer, buf %p sz %d cliprects %d\n",
		  cmdbuf->buf, cmdbuf->sz, cmdbuf->num_cliprects);

	if (cmdbuf->num_cliprects < 0)
		return -EINVAL;

	DRM_UNLOCK(dev);

	batch_data = kmalloc(cmdbuf->sz, DRM_MEM_DMA, M_WAITOK);

	ret = -copyin(cmdbuf->buf, batch_data, cmdbuf->sz);
	if (ret != 0) {
		DRM_LOCK(dev);
		goto fail_batch_free;
	}

	if (cmdbuf->num_cliprects) {
		cliprects = kmalloc(cmdbuf->num_cliprects *
		    sizeof(struct drm_clip_rect), DRM_MEM_DMA,
		    M_WAITOK | M_ZERO);
		ret = -copyin(cmdbuf->cliprects, cliprects,
		    cmdbuf->num_cliprects * sizeof(struct drm_clip_rect));
		if (ret != 0) {
			DRM_LOCK(dev);
			goto fail_clip_free;
		}
	}

	DRM_LOCK(dev);
	RING_LOCK_TEST_WITH_RETURN(dev, file_priv);
	ret = i915_dispatch_cmdbuffer(dev, cmdbuf, cliprects, batch_data);
	if (ret) {
		DRM_ERROR("i915_dispatch_cmdbuffer failed\n");
		goto fail_clip_free;
	}

	sarea_priv = (drm_i915_sarea_t *)dev_priv->sarea_priv;
	if (sarea_priv)
		sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

fail_clip_free:
	drm_free(cliprects, DRM_MEM_DMA);
fail_batch_free:
	drm_free(batch_data, DRM_MEM_DMA);
	return ret;
}

static int i915_flip_bufs(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	int ret;

	DRM_DEBUG("%s\n", __func__);

	RING_LOCK_TEST_WITH_RETURN(dev, file_priv);

	ret = i915_dispatch_flip(dev);

	return ret;
}

static int i915_getparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_getparam_t *param = data;
	int value;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_PARAM_IRQ_ACTIVE:
		value = dev->irq_enabled ? 1 : 0;
		break;
	case I915_PARAM_ALLOW_BATCHBUFFER:
		value = dev_priv->allow_batchbuffer ? 1 : 0;
		break;
	case I915_PARAM_LAST_DISPATCH:
		value = READ_BREADCRUMB(dev_priv);
		break;
	case I915_PARAM_CHIPSET_ID:
		value = dev->pci_device;
		break;
	case I915_PARAM_HAS_GEM:
		value = 1;
		break;
	case I915_PARAM_NUM_FENCES_AVAIL:
		value = dev_priv->num_fence_regs - dev_priv->fence_reg_start;
		break;
	case I915_PARAM_HAS_OVERLAY:
		value = dev_priv->overlay ? 1 : 0;
		break;
	case I915_PARAM_HAS_PAGEFLIPPING:
		value = 1;
		break;
	case I915_PARAM_HAS_EXECBUF2:
		value = 1;
		break;
	case I915_PARAM_HAS_BSD:
		value = HAS_BSD(dev);
		break;
	case I915_PARAM_HAS_BLT:
		value = HAS_BLT(dev);
		break;
	case I915_PARAM_HAS_RELAXED_FENCING:
		value = 1;
		break;
	case I915_PARAM_HAS_COHERENT_RINGS:
		value = 1;
		break;
	case I915_PARAM_HAS_EXEC_CONSTANTS:
		value = INTEL_INFO(dev)->gen >= 4;
		break;
	case I915_PARAM_HAS_RELAXED_DELTA:
		value = 1;
		break;
	case I915_PARAM_HAS_GEN7_SOL_RESET:
		value = 1;
		break;
	case I915_PARAM_HAS_LLC:
		value = HAS_LLC(dev);
		break;
	default:
		DRM_DEBUG_DRIVER("Unknown parameter %d\n",
				 param->param);
		return -EINVAL;
	}

	if (DRM_COPY_TO_USER(param->value, &value, sizeof(int))) {
		DRM_ERROR("DRM_COPY_TO_USER failed\n");
		return -EFAULT;
	}

	return 0;
}

static int i915_setparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_setparam_t *param = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_SETPARAM_USE_MI_BATCHBUFFER_START:
		break;
	case I915_SETPARAM_TEX_LRU_LOG_GRANULARITY:
		dev_priv->tex_lru_log_granularity = param->value;
		break;
	case I915_SETPARAM_ALLOW_BATCHBUFFER:
		dev_priv->allow_batchbuffer = param->value;
		break;
	case I915_SETPARAM_NUM_USED_FENCES:
		if (param->value > dev_priv->num_fence_regs ||
		    param->value < 0)
			return -EINVAL;
		/* Userspace can use first N regs */
		dev_priv->fence_reg_start = param->value;
		break;
	default:
		DRM_DEBUG("unknown parameter %d\n", param->param);
		return -EINVAL;
	}

	return 0;
}

static int i915_set_status_page(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_hws_addr_t *hws = data;
	struct intel_ring_buffer *ring = LP_RING(dev_priv);

	if (!I915_NEED_GFX_HWS(dev))
		return -EINVAL;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	DRM_DEBUG("set status page addr 0x%08x\n", (u32)hws->addr);
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		DRM_ERROR("tried to set status page when mode setting active\n");
		return 0;
	}

	ring->status_page.gfx_addr = dev_priv->status_gfx_addr =
	    hws->addr & (0x1ffff<<12);

	dev_priv->hws_map.offset = dev->agp->base + hws->addr;
	dev_priv->hws_map.size = 4*1024;
	dev_priv->hws_map.type = 0;
	dev_priv->hws_map.flags = 0;
	dev_priv->hws_map.mtrr = 0;

	drm_core_ioremap_wc(&dev_priv->hws_map, dev);
	if (dev_priv->hws_map.virtual == NULL) {
		i915_dma_cleanup(dev);
		ring->status_page.gfx_addr = dev_priv->status_gfx_addr = 0;
		DRM_ERROR("can not ioremap virtual address for"
				" G33 hw status page\n");
		return -ENOMEM;
	}
	ring->status_page.page_addr = dev_priv->hw_status_page =
	    dev_priv->hws_map.virtual;

	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
	I915_WRITE(HWS_PGA, dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws HWS_PGA with gfx mem 0x%x\n",
			dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws at %p\n", dev_priv->hw_status_page);
	return 0;
}

static bool
intel_enable_ppgtt(struct drm_device *dev)
{
	if (i915_enable_ppgtt >= 0)
		return i915_enable_ppgtt;

	/* Disable ppgtt on SNB if VT-d is on. */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_enabled)
		return false;

	return true;
}

static int
i915_load_gem_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long prealloc_size, gtt_size, mappable_size;
	int ret;

	prealloc_size = dev_priv->mm.gtt->stolen_size;
	gtt_size = dev_priv->mm.gtt->gtt_total_entries << PAGE_SHIFT;
	mappable_size = dev_priv->mm.gtt->gtt_mappable_entries << PAGE_SHIFT;

	/* Basic memrange allocator for stolen space */
	drm_mm_init(&dev_priv->mm.stolen, 0, prealloc_size);

	DRM_LOCK(dev);
	if (intel_enable_ppgtt(dev) && HAS_ALIASING_PPGTT(dev)) {
		/* PPGTT pdes are stolen from global gtt ptes, so shrink the
		 * aperture accordingly when using aliasing ppgtt. */
		gtt_size -= I915_PPGTT_PD_ENTRIES*PAGE_SIZE;
		/* For paranoia keep the guard page in between. */
		gtt_size -= PAGE_SIZE;

		i915_gem_do_init(dev, 0, mappable_size, gtt_size);

		ret = i915_gem_init_aliasing_ppgtt(dev);
		if (ret) {
			DRM_UNLOCK(dev);
			return ret;
		}
	} else {
		/* Let GEM Manage all of the aperture.
		 *
		 * However, leave one page at the end still bound to the scratch
		 * page.  There are a number of places where the hardware
		 * apparently prefetches past the end of the object, and we've
		 * seen multiple hangs with the GPU head pointer stuck in a
		 * batchbuffer bound at the last page of the aperture.  One page
		 * should be enough to keep any prefetching inside of the
		 * aperture.
		 */
		i915_gem_do_init(dev, 0, mappable_size, gtt_size - PAGE_SIZE);
	}

	ret = i915_gem_init_hw(dev);
	DRM_UNLOCK(dev);
	if (ret != 0) {
		i915_gem_cleanup_aliasing_ppgtt(dev);
		return (ret);
	}

#if 0
	/* Try to set up FBC with a reasonable compressed buffer size */
	if (I915_HAS_FBC(dev) && i915_powersave) {
		int cfb_size;

		/* Leave 1M for line length buffer & misc. */

		/* Try to get a 32M buffer... */
		if (prealloc_size > (36*1024*1024))
			cfb_size = 32*1024*1024;
		else /* fall back to 7/8 of the stolen space */
			cfb_size = prealloc_size * 7 / 8;
		i915_setup_compression(dev, cfb_size);
	}
#endif

	/* Allow hardware batchbuffers unless told otherwise. */
	dev_priv->allow_batchbuffer = 1;
	return 0;
}

static int
i915_load_modeset_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	ret = intel_parse_bios(dev);
	if (ret)
		DRM_INFO("failed to find VBIOS tables\n");

#if 0
	intel_register_dsm_handler();
#endif

	/* IIR "flip pending" bit means done if this bit is set */
	if (IS_GEN3(dev) && (I915_READ(ECOSKPD) & ECO_FLIP_DONE))
		dev_priv->flip_pending_is_done = true;

	intel_modeset_init(dev);

	ret = i915_load_gem_init(dev);
	if (ret != 0)
		goto cleanup_gem;

	intel_modeset_gem_init(dev);

	ret = drm_irq_install(dev);
	if (ret)
		goto cleanup_gem;

	dev->vblank_disable_allowed = 1;

	ret = intel_fbdev_init(dev);
	if (ret)
		goto cleanup_gem;

	drm_kms_helper_poll_init(dev);

	/* We're off and running w/KMS */
	dev_priv->mm.suspended = 0;

	return (0);

cleanup_gem:
	DRM_LOCK(dev);
	i915_gem_cleanup_ringbuffer(dev);
	DRM_UNLOCK(dev);
	i915_gem_cleanup_aliasing_ppgtt(dev);
	return (ret);
}

static int
i915_get_bridge_dev(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;

	dev_priv = dev->dev_private;

	dev_priv->bridge_dev = intel_gtt_get_bridge_device();
	if (dev_priv->bridge_dev == NULL) {
		DRM_ERROR("bridge device not found\n");
		return (-1);
	}
	return (0);
}

#define MCHBAR_I915 0x44
#define MCHBAR_I965 0x48
#define MCHBAR_SIZE (4*4096)

#define DEVEN_REG 0x54
#define   DEVEN_MCHBAR_EN (1 << 28)

/* Allocate space for the MCH regs if needed, return nonzero on error */
static int
intel_alloc_mchbar_resource(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;
	device_t vga;
	int reg;
	u32 temp_lo, temp_hi;
	u64 mchbar_addr, temp;

	dev_priv = dev->dev_private;
	reg = INTEL_INFO(dev)->gen >= 4 ? MCHBAR_I965 : MCHBAR_I915;

	if (INTEL_INFO(dev)->gen >= 4)
		temp_hi = pci_read_config(dev_priv->bridge_dev, reg + 4, 4);
	else
		temp_hi = 0;
	temp_lo = pci_read_config(dev_priv->bridge_dev, reg, 4);
	mchbar_addr = ((u64)temp_hi << 32) | temp_lo;

	/* If ACPI doesn't have it, assume we need to allocate it ourselves */
#ifdef XXX_CONFIG_PNP
	if (mchbar_addr &&
	    pnp_range_reserved(mchbar_addr, mchbar_addr + MCHBAR_SIZE))
		return 0;
#endif

	/* Get some space for it */
	vga = device_get_parent(dev->dev);
	dev_priv->mch_res_rid = 0x100;
	dev_priv->mch_res = BUS_ALLOC_RESOURCE(device_get_parent(vga),
	    dev->dev, SYS_RES_MEMORY, &dev_priv->mch_res_rid, 0, ~0UL,
	    MCHBAR_SIZE, RF_ACTIVE | RF_SHAREABLE, -1);
	if (dev_priv->mch_res == NULL) {
		DRM_ERROR("failed mchbar resource alloc\n");
		return (-ENOMEM);
	}

	if (INTEL_INFO(dev)->gen >= 4) {
		temp = rman_get_start(dev_priv->mch_res);
		temp >>= 32;
		pci_write_config(dev_priv->bridge_dev, reg + 4, temp, 4);
	}
	pci_write_config(dev_priv->bridge_dev, reg,
	    rman_get_start(dev_priv->mch_res) & UINT32_MAX, 4);
	return (0);
}

static void
intel_setup_mchbar(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;
	int mchbar_reg;
	u32 temp;
	bool enabled;

	dev_priv = dev->dev_private;
	mchbar_reg = INTEL_INFO(dev)->gen >= 4 ? MCHBAR_I965 : MCHBAR_I915;

	dev_priv->mchbar_need_disable = false;

	if (IS_I915G(dev) || IS_I915GM(dev)) {
		temp = pci_read_config(dev_priv->bridge_dev, DEVEN_REG, 4);
		enabled = (temp & DEVEN_MCHBAR_EN) != 0;
	} else {
		temp = pci_read_config(dev_priv->bridge_dev, mchbar_reg, 4);
		enabled = temp & 1;
	}

	/* If it's already enabled, don't have to do anything */
	if (enabled) {
		DRM_DEBUG("mchbar already enabled\n");
		return;
	}

	if (intel_alloc_mchbar_resource(dev))
		return;

	dev_priv->mchbar_need_disable = true;

	/* Space is allocated or reserved, so enable it. */
	if (IS_I915G(dev) || IS_I915GM(dev)) {
		pci_write_config(dev_priv->bridge_dev, DEVEN_REG,
		    temp | DEVEN_MCHBAR_EN, 4);
	} else {
		temp = pci_read_config(dev_priv->bridge_dev, mchbar_reg, 4);
		pci_write_config(dev_priv->bridge_dev, mchbar_reg, temp | 1, 4);
	}
}

static void
intel_teardown_mchbar(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;
	device_t vga;
	int mchbar_reg;
	u32 temp;

	dev_priv = dev->dev_private;
	mchbar_reg = INTEL_INFO(dev)->gen >= 4 ? MCHBAR_I965 : MCHBAR_I915;

	if (dev_priv->mchbar_need_disable) {
		if (IS_I915G(dev) || IS_I915GM(dev)) {
			temp = pci_read_config(dev_priv->bridge_dev,
			    DEVEN_REG, 4);
			temp &= ~DEVEN_MCHBAR_EN;
			pci_write_config(dev_priv->bridge_dev, DEVEN_REG,
			    temp, 4);
		} else {
			temp = pci_read_config(dev_priv->bridge_dev,
			    mchbar_reg, 4);
			temp &= ~1;
			pci_write_config(dev_priv->bridge_dev, mchbar_reg,
			    temp, 4);
		}
	}

	if (dev_priv->mch_res != NULL) {
		vga = device_get_parent(dev->dev);
		BUS_DEACTIVATE_RESOURCE(device_get_parent(vga), dev->dev,
		    SYS_RES_MEMORY, dev_priv->mch_res_rid, dev_priv->mch_res);
		BUS_RELEASE_RESOURCE(device_get_parent(vga), dev->dev,
		    SYS_RES_MEMORY, dev_priv->mch_res_rid, dev_priv->mch_res);
		dev_priv->mch_res = NULL;
	}
}

int
i915_driver_load(struct drm_device *dev, unsigned long flags)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long base, size;
	int mmio_bar, ret;

	ret = 0;

	/* i915 has 4 more counters */
	dev->counters += 4;
	dev->types[6] = _DRM_STAT_IRQ;
	dev->types[7] = _DRM_STAT_PRIMARY;
	dev->types[8] = _DRM_STAT_SECONDARY;
	dev->types[9] = _DRM_STAT_DMA;

	dev_priv = kmalloc(sizeof(drm_i915_private_t), DRM_MEM_DRIVER,
	    M_ZERO | M_WAITOK);
	if (dev_priv == NULL)
		return -ENOMEM;

	dev->dev_private = (void *)dev_priv;
	dev_priv->dev = dev;
	dev_priv->info = i915_get_device_id(dev->pci_device);

	if (i915_get_bridge_dev(dev)) {
		drm_free(dev_priv, DRM_MEM_DRIVER);
		return (-EIO);
	}
	dev_priv->mm.gtt = intel_gtt_get();

	/* Add register map (needed for suspend/resume) */
	mmio_bar = IS_GEN2(dev) ? 1 : 0;
	base = drm_get_resource_start(dev, mmio_bar);
	size = drm_get_resource_len(dev, mmio_bar);

	ret = drm_addmap(dev, base, size, _DRM_REGISTERS,
	    _DRM_KERNEL | _DRM_DRIVER, &dev_priv->mmio_map);

	dev_priv->tq = taskqueue_create("915", M_WAITOK,
	    taskqueue_thread_enqueue, &dev_priv->tq);
	taskqueue_start_threads(&dev_priv->tq, 1, 0, -1, "i915 taskq");
	lockinit(&dev_priv->gt_lock, "915gt", 0, LK_CANRECURSE);
	lockinit(&dev_priv->error_lock, "915err", 0, LK_CANRECURSE);
	lockinit(&dev_priv->error_completion_lock, "915cmp", 0, LK_CANRECURSE);
	lockinit(&dev_priv->rps_lock, "915rps", 0, LK_CANRECURSE);

	dev_priv->has_gem = 1;
	intel_irq_init(dev);

	intel_setup_mchbar(dev);
	intel_setup_gmbus(dev);
	intel_opregion_setup(dev);

	intel_setup_bios(dev);

	i915_gem_load(dev);

	/* Init HWS */
	if (!I915_NEED_GFX_HWS(dev)) {
		ret = i915_init_phys_hws(dev);
		if (ret != 0) {
			drm_rmmap(dev, dev_priv->mmio_map);
			drm_free(dev_priv, DRM_MEM_DRIVER);
			return ret;
		}
	}

	if (IS_PINEVIEW(dev))
		i915_pineview_get_mem_freq(dev);
	else if (IS_GEN5(dev))
		i915_ironlake_get_mem_freq(dev);

	lockinit(&dev_priv->irq_lock, "userirq", 0, LK_CANRECURSE);

	if (IS_IVYBRIDGE(dev))
		dev_priv->num_pipe = 3;
	else if (IS_MOBILE(dev) || !IS_GEN2(dev))
		dev_priv->num_pipe = 2;
	else
		dev_priv->num_pipe = 1;

	ret = drm_vblank_init(dev, dev_priv->num_pipe);
	if (ret)
		goto out_gem_unload;

	/* Start out suspended */
	dev_priv->mm.suspended = 1;

	intel_detect_pch(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		DRM_UNLOCK(dev);
		ret = i915_load_modeset_init(dev);
		DRM_LOCK(dev);
		if (ret < 0) {
			DRM_ERROR("failed to init modeset\n");
			goto out_gem_unload;
		}
	}

	intel_opregion_init(dev);

	callout_init_mp(&dev_priv->hangcheck_timer);
	callout_reset(&dev_priv->hangcheck_timer, DRM_I915_HANGCHECK_PERIOD,
	    i915_hangcheck_elapsed, dev);

	if (IS_GEN5(dev)) {
		lockmgr(&mchdev_lock, LK_EXCLUSIVE);
		i915_mch_dev = dev_priv;
		dev_priv->mchdev_lock = &mchdev_lock;
		lockmgr(&mchdev_lock, LK_RELEASE);
	}

	return (0);

out_gem_unload:
	/* XXXKIB */
	(void) i915_driver_unload_int(dev, true);
	return (ret);
}

static int
i915_driver_unload_int(struct drm_device *dev, bool locked)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (!locked)
		DRM_LOCK(dev);
	ret = i915_gpu_idle(dev, true);
	if (ret)
		DRM_ERROR("failed to idle hardware: %d\n", ret);
	if (!locked)
		DRM_UNLOCK(dev);

	i915_free_hws(dev);

	intel_teardown_mchbar(dev);

	if (locked)
		DRM_UNLOCK(dev);
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		intel_fbdev_fini(dev);
		intel_modeset_cleanup(dev);
	}

	/* Free error state after interrupts are fully disabled. */
	callout_stop(&dev_priv->hangcheck_timer);

	i915_destroy_error_state(dev);

	intel_opregion_fini(dev);

	if (locked)
		DRM_LOCK(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		if (!locked)
			DRM_LOCK(dev);
		i915_gem_free_all_phys_object(dev);
		i915_gem_cleanup_ringbuffer(dev);
		if (!locked)
			DRM_UNLOCK(dev);
		i915_gem_cleanup_aliasing_ppgtt(dev);
#if 1
		KIB_NOTYET();
#else
		if (I915_HAS_FBC(dev) && i915_powersave)
			i915_cleanup_compression(dev);
#endif
		drm_mm_takedown(&dev_priv->mm.stolen);

		intel_cleanup_overlay(dev);

		if (!I915_NEED_GFX_HWS(dev))
			i915_free_hws(dev);
	}

	i915_gem_unload(dev);

	lockuninit(&dev_priv->irq_lock);

	if (dev_priv->tq != NULL)
		taskqueue_free(dev_priv->tq);

	bus_generic_detach(dev->dev);
	drm_rmmap(dev, dev_priv->mmio_map);
	intel_teardown_gmbus(dev);

	lockuninit(&dev_priv->error_lock);
	lockuninit(&dev_priv->error_completion_lock);
	lockuninit(&dev_priv->rps_lock);
	drm_free(dev->dev_private, DRM_MEM_DRIVER);

	return (0);
}

int
i915_driver_unload(struct drm_device *dev)
{

	return (i915_driver_unload_int(dev, true));
}

int
i915_driver_open(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_i915_file_private *i915_file_priv;

	i915_file_priv = kmalloc(sizeof(*i915_file_priv), DRM_MEM_FILES,
	    M_WAITOK | M_ZERO);

	spin_init(&i915_file_priv->mm.lock);
	INIT_LIST_HEAD(&i915_file_priv->mm.request_list);
	file_priv->driver_priv = i915_file_priv;

	return (0);
}

void
i915_driver_lastclose(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (!dev_priv || drm_core_check_feature(dev, DRIVER_MODESET)) {
#if 1
		KIB_NOTYET();
#else
		drm_fb_helper_restore();
		vga_switcheroo_process_delayed_switch();
#endif
		return;
	}
	i915_gem_lastclose(dev);
	i915_dma_cleanup(dev);
}

void i915_driver_preclose(struct drm_device * dev, struct drm_file *file_priv)
{

	i915_gem_release(dev, file_priv);
}

void i915_driver_postclose(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_i915_file_private *i915_file_priv = file_priv->driver_priv;

	spin_uninit(&i915_file_priv->mm.lock);
	drm_free(i915_file_priv, DRM_MEM_FILES);
}

struct drm_ioctl_desc i915_ioctls[] = {
	DRM_IOCTL_DEF(DRM_I915_INIT, i915_dma_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_FLUSH, i915_flush_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_FLIP, i915_flip_bufs, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_BATCHBUFFER, i915_batchbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_IRQ_EMIT, i915_irq_emit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_IRQ_WAIT, i915_irq_wait, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_GETPARAM, i915_getparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_SETPARAM, i915_setparam, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_ALLOC, drm_noop, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_FREE, drm_noop, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_INIT_HEAP, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_CMDBUFFER, i915_cmdbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_DESTROY_HEAP,  drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY ),
	DRM_IOCTL_DEF(DRM_I915_SET_VBLANK_PIPE,  i915_vblank_pipe_set, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY ),
	DRM_IOCTL_DEF(DRM_I915_GET_VBLANK_PIPE,  i915_vblank_pipe_get, DRM_AUTH ),
	DRM_IOCTL_DEF(DRM_I915_VBLANK_SWAP, i915_vblank_swap, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_HWS_ADDR, i915_set_status_page, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_GEM_INIT, i915_gem_init_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_GEM_EXECBUFFER, i915_gem_execbuffer, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_EXECBUFFER2, i915_gem_execbuffer2, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_PIN, i915_gem_pin_ioctl, DRM_AUTH|DRM_ROOT_ONLY|DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_UNPIN, i915_gem_unpin_ioctl, DRM_AUTH|DRM_ROOT_ONLY|DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_BUSY, i915_gem_busy_ioctl, DRM_AUTH|DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_THROTTLE, i915_gem_throttle_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_GEM_ENTERVT, i915_gem_entervt_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_GEM_LEAVEVT, i915_gem_leavevt_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_GEM_CREATE, i915_gem_create_ioctl, 0),
	DRM_IOCTL_DEF(DRM_I915_GEM_PREAD, i915_gem_pread_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_PWRITE, i915_gem_pwrite_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_MMAP, i915_gem_mmap_ioctl, 0),
	DRM_IOCTL_DEF(DRM_I915_GEM_MMAP_GTT, i915_gem_mmap_gtt_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_SET_DOMAIN, i915_gem_set_domain_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_SW_FINISH, i915_gem_sw_finish_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_SET_TILING, i915_gem_set_tiling, 0),
	DRM_IOCTL_DEF(DRM_I915_GEM_GET_TILING, i915_gem_get_tiling, 0),
	DRM_IOCTL_DEF(DRM_I915_GEM_GET_APERTURE, i915_gem_get_aperture_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GET_PIPE_FROM_CRTC_ID, intel_get_pipe_from_crtc_id, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GEM_MADVISE, i915_gem_madvise_ioctl, DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_OVERLAY_PUT_IMAGE, intel_overlay_put_image, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_OVERLAY_ATTRS, intel_overlay_attrs, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_SET_SPRITE_COLORKEY, intel_sprite_set_colorkey, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
	DRM_IOCTL_DEF(DRM_I915_GET_SPRITE_COLORKEY, intel_sprite_get_colorkey, DRM_MASTER|DRM_CONTROL_ALLOW|DRM_UNLOCKED),
};

struct drm_driver i915_driver_info = {
	.driver_features =   DRIVER_USE_AGP | DRIVER_REQUIRE_AGP |
	    DRIVER_USE_MTRR | DRIVER_HAVE_IRQ | DRIVER_LOCKLESS_IRQ |
	    DRIVER_GEM /*| DRIVER_MODESET*/,

	.buf_priv_size	= sizeof(drm_i915_private_t),
	.load		= i915_driver_load,
	.open		= i915_driver_open,
	.unload		= i915_driver_unload,
	.preclose	= i915_driver_preclose,
	.lastclose	= i915_driver_lastclose,
	.postclose	= i915_driver_postclose,
	.device_is_agp	= i915_driver_device_is_agp,
	.gem_init_object = i915_gem_init_object,
	.gem_free_object = i915_gem_free_object,
	.gem_pager_ops	= &i915_gem_pager_ops,
	.dumb_create	= i915_gem_dumb_create,
	.dumb_map_offset = i915_gem_mmap_gtt,
	.dumb_destroy	= i915_gem_dumb_destroy,
	.sysctl_init	= i915_sysctl_init,
	.sysctl_cleanup	= i915_sysctl_cleanup,

	.ioctls		= i915_ioctls,
	.max_ioctl	= DRM_ARRAY_SIZE(i915_ioctls),

	.name		= DRIVER_NAME,
	.desc		= DRIVER_DESC,
	.date		= DRIVER_DATE,
	.major		= DRIVER_MAJOR,
	.minor		= DRIVER_MINOR,
	.patchlevel	= DRIVER_PATCHLEVEL,
};

/**
 * Determine if the device really is AGP or not.
 *
 * All Intel graphics chipsets are treated as AGP, even if they are really
 * built-in.
 *
 * \param dev   The device to be tested.
 *
 * \returns
 * A value of 1 is always retured to indictate every i9x5 is AGP.
 */
int i915_driver_device_is_agp(struct drm_device * dev)
{
	return 1;
}
