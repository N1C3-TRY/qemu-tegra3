/*
 * ARM dummy L210, L220, PL310 cache controller.
 *
 * Copyright (c) 2010-2012 Calxeda
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or any later version, as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "sysbus.h"

/* L2C-310 r3p2 */
#define CACHE_ID 0x410000c8

typedef struct l2x0_state {
    SysBusDevice busdev;
    uint32_t cache_type;
    uint32_t ctrl;
    uint32_t aux_ctrl;
    uint32_t data_ctrl;
    uint32_t tag_ctrl;
    uint32_t filter_start;
    uint32_t filter_end;
} l2x0_state;

static const VMStateDescription vmstate_l2x0 = {
    .name = "l2x0",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(ctrl, l2x0_state),
        VMSTATE_UINT32(aux_ctrl, l2x0_state),
        VMSTATE_UINT32(data_ctrl, l2x0_state),
        VMSTATE_UINT32(tag_ctrl, l2x0_state),
        VMSTATE_UINT32(filter_start, l2x0_state),
        VMSTATE_UINT32(filter_end, l2x0_state),
        VMSTATE_END_OF_LIST()
    }
};

static uint32_t l2x0_priv_read(void *opaque, target_phys_addr_t offset)
{
    uint32_t cache_data;
    l2x0_state *s = (l2x0_state *)opaque;
    offset &= 0xfff;
    if (offset >= 0x730 && offset < 0x800) {
        return 0; /* cache ops complete */
    }
    switch (offset) {
    case 0:
        return CACHE_ID;
    case 0x4:
        /* aux_ctrl values affect cache_type values */
        cache_data = (s->aux_ctrl & (7 << 17)) >> 15;
        cache_data |= (s->aux_ctrl & (1 << 16)) >> 16;
        return s->cache_type |= (cache_data << 18) | (cache_data << 6);
    case 0x100:
        return s->ctrl;
    case 0x104:
        return s->aux_ctrl;
    case 0x108:
        return s->tag_ctrl;
    case 0x10C:
        return s->data_ctrl;
    case 0xC00:
        return s->filter_start;
    case 0xC04:
        return s->filter_end;
    case 0xF40:
        return 0;
    case 0xF60:
        return 0;
    case 0xF80:
        return 0;
    default:
        fprintf(stderr, "l2x0_priv_read: Bad offset %x\n", (int)offset);
        break;
    }
    return 0;
}

static void l2x0_priv_write(void *opaque, target_phys_addr_t offset,
                            uint32_t value)
{
    l2x0_state *s = (l2x0_state *)opaque;
    offset &= 0xfff;
    if (offset >= 0x730 && offset < 0x800) {
        /* ignore */
        return;
    }
    switch (offset) {
    case 0x100:
        s->ctrl = value & 1;
        break;
    case 0x104:
        s->aux_ctrl = value;
        break;
    case 0x108:
        s->tag_ctrl = value;
        break;
    case 0x10C:
        s->data_ctrl = value;
        break;
    case 0xC00:
        s->filter_start = value;
        break;
    case 0xC04:
        s->filter_end = value;
        break;
    case 0xF40:
        return;
    case 0xF60:
        return;
    case 0xF80:
        return;
    default:
        fprintf(stderr, "l2x0_priv_write: Bad offset %x\n", (int)offset);
        break;
    }
}

static void l2x0_priv_reset(DeviceState *dev)
{
    l2x0_state *s = DO_UPCAST(l2x0_state, busdev.qdev, dev);

    s->ctrl = 0;
    s->aux_ctrl = 0x02020000;
    s->tag_ctrl = 0;
    s->data_ctrl = 0;
    s->filter_start = 0;
    s->filter_end = 0;
}

static CPUReadMemoryFunc * const tegra_l2x0_readfn[] = {
   l2x0_priv_read,
   l2x0_priv_read,
   l2x0_priv_read
};

static CPUWriteMemoryFunc * const tegra_l2x0_writefn[] = {
   l2x0_priv_write,
   l2x0_priv_write,
   l2x0_priv_write
};

static int l2x0_priv_init(SysBusDevice *dev)
{
    int iomemtype;
    l2x0_state *s = FROM_SYSBUS(l2x0_state, dev);

    iomemtype = cpu_register_io_memory(tegra_l2x0_readfn,
                                       tegra_l2x0_writefn, s,
                                       DEVICE_NATIVE_ENDIAN);
    sysbus_init_mmio(dev, 0x1000, iomemtype);
    return 0;
}

static SysBusDeviceInfo l2x0_info = {
    .init = l2x0_priv_init,
    .qdev.name = "l2x0",
    .qdev.size = sizeof(l2x0_state),
    .qdev.vmsd = &vmstate_l2x0,
    .qdev.no_user = 1,
    .qdev.props = (Property[]) {
        DEFINE_PROP_UINT32("type", l2x0_state, cache_type, 0x1c100100),
        DEFINE_PROP_END_OF_LIST(),
    },
    .qdev.reset = l2x0_priv_reset,
};

static void l2x0_register_device(void)
{
    sysbus_register_withprop(&l2x0_info);
}

device_init(l2x0_register_device)
