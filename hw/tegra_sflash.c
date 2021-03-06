/*
 * Copyright 2011 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * Tegra2 SPI flash (SFlash) controller emulation
 */

#include "sysbus.h"

#define DEBUG_SPI 1

#ifdef DEBUG_SPI
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "tegra_spi: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

/* status Clear-On-Write bits */
//#define SFLASH_STAT_COW 0x4C000000
#define SFLASH_STAT_COW 0x4C000000
/* status Read Only bits */
#define SFLASH_STAT_RO  0x83C00000

typedef struct {
    SysBusDevice busdev;
    uint32_t command;
    uint32_t status;
    uint32_t rx_cmp;
    uint32_t dma_ctl;
    qemu_irq irq;
} tegra_sflash_state;

static uint32_t tegra_sflash_read(void *opaque, target_phys_addr_t offset)
{
    tegra_sflash_state *s = (tegra_sflash_state *)opaque;
    DPRINTF("READ at %x status: 0x%x\n", offset, s->status);

    switch (offset) {
    case 0x00: /* SPI_COMMAND */
        return s->command;
    case 0x04: /* SPI_STATUS */
        return s->status;
    case 0x08: /* SPI_RX_CMP */
    	s->rx_cmp |= 0x00800000; // RX_EMPTY
    	s->rx_cmp |= 0x00200000; // TX_EMPTY
    	s->rx_cmp |= 0x40000000; // RDY
        return s->rx_cmp;
    case 0x0c: /* SPI_DMA_CTL */
        return s->dma_ctl;
    case 0x10: /* SPI_TX_FIFO */ // tegra2
        hw_error("tegra_sflash_read: Write only register\n");
    case 0x1c: /* SLINK_STATUS2_0 */
	return 0; //TODO
    case 0x20: /* SPI_RX_FIFO */ //tegra2
        return 0;
    case 0x100: /* SLINK_TX_FIFO_0 */
        hw_error("tegra_sflash_read: Write only register\n");
    case 0x180: /* SLINK_RX_FIFO_0 */
        return 0;
    default:
        hw_error("tegra_sflash_read: Bad offset %x\n", (int)offset);
    }

    return 0;
}

static void tegra_sflash_write(void *opaque, target_phys_addr_t offset,
                          uint32_t value)
{
    tegra_sflash_state *s = (tegra_sflash_state *)opaque;
    DPRINTF("WRITE at %x <= %x\n", offset, value);

    switch (offset) {
    case 0x00: /* SPI_COMMAND */
        s->command = value & 0x1c2dffff;
        if (value & 0x40000000) { /* Go */
            s->status |= 1<<30; /* Ready */
        }
        break;
    case 0x04: /* SPI_STATUS */
        s->status = (value & ~(SFLASH_STAT_RO | SFLASH_STAT_COW));
        s->status &= ~(value & SFLASH_STAT_COW);
        break;
    case 0x08: /* SPI_RX_CMP */
        s->rx_cmp = value & 0x1ffff;
        break;
    case 0x0c: /* SPI_DMA_CTL */
        s->dma_ctl = value;
        break;
    case 0x10: /* SPI_TX_FIFO */
        break;
    case 0x1c: /* SLINK_STATUS2_0 */
	break; //TODO
    case 0x20: /* SPI_RX_FIFO */
        hw_error("tegra_sflash_read: Read only register\n");
    case 0x100: /* SLINK_TX_FIFO_0 */
	break;
    case 0x180: /* SLINK_RX_FIFO_0 */
        hw_error("tegra_sflash_read: Read only register\n");
    default:
        hw_error("tegra_sflash_write: Bad offset %x\n", (int)offset);
    }
}

static CPUReadMemoryFunc * const tegra_sflash_readfn[] = {
   tegra_sflash_read,
   tegra_sflash_read,
   tegra_sflash_read
};

static CPUWriteMemoryFunc * const tegra_sflash_writefn[] = {
   tegra_sflash_write,
   tegra_sflash_write,
   tegra_sflash_write
};

static int tegra_sflash_init(SysBusDevice *dev)
{
    int iomemtype;
    tegra_sflash_state *s = FROM_SYSBUS(tegra_sflash_state, dev);

    iomemtype = cpu_register_io_memory(tegra_sflash_readfn,
                                       tegra_sflash_writefn, s,
                                       DEVICE_NATIVE_ENDIAN);
    sysbus_init_mmio(dev, 0x200, iomemtype);
    sysbus_init_irq(dev, &s->irq);

    return 0;
}

static void tegra_sflash_reset(DeviceState *d)
{
    tegra_sflash_state *s = container_of(d, tegra_sflash_state, busdev.qdev);

    s->command = 0x10000420;
    s->status = 0x02800000;
    s->rx_cmp = 0;
    s->dma_ctl = 0;
}

static const VMStateDescription tegra_sflash_vmstate = {
    .name = "tegra_sflash",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(command, tegra_sflash_state),
        VMSTATE_UINT32(status, tegra_sflash_state),
        VMSTATE_UINT32(rx_cmp, tegra_sflash_state),
        VMSTATE_UINT32(dma_ctl, tegra_sflash_state),
        VMSTATE_END_OF_LIST()
    }
};

static SysBusDeviceInfo tegra_sflash_info = {
    .init = tegra_sflash_init,
    .qdev.name  = "tegra_sflash",
    .qdev.size  = sizeof(tegra_sflash_state),
    .qdev.vmsd  = &tegra_sflash_vmstate,
    .qdev.reset = tegra_sflash_reset,
};

static void tegra_sflash_register(void)
{
    sysbus_register_withprop(&tegra_sflash_info);
}

device_init(tegra_sflash_register)
