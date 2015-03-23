/*
 *  GICv2m extension for MSI/MSI-x support with a GICv2-based system
 *
 * Copyright (C) 2015 Linaro, All rights reserved.
 *
 * Author: Christoffer Dall <christoffer.dall@linaro.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hw/sysbus.h"

#define TYPE_GICV2M "gicv2m"
#define GICV2M(obj) OBJECT_CHECK(GICv2mState, (obj), TYPE_GICV2M)

#define GICV2M_NUM_SPI_MAX 128

#define V2M_MSI_TYPER           0x008
#define V2M_MSI_SETSPI_NS       0x040
#define V2M_MSI_IIDR            0xFCC
#define V2M_IIDR0               0xFD0

typedef struct GICv2mState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq spi[GICV2M_NUM_SPI_MAX];

    uint32_t base_spi;
    uint32_t num_spi;
} GICv2mState;

static void gicv2m_set_irq(void *opaque, int irq, int level)
{
    GICv2mState *s = (GICv2mState *)opaque;

    qemu_set_irq(s->spi[irq], level);
}

static uint64_t gicv2m_read(void *opaque, hwaddr offset,
                            unsigned size)
{
    GICv2mState *s = (GICv2mState *)opaque;
    uint32_t val;

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR, "gicv2m_read: bad size %u\n", size);
        return 0;
    }

    switch (offset) {
    case V2M_MSI_TYPER:
        val = (s->base_spi + 32) << 16;
        val |= s->num_spi;
        return val;
    case V2M_MSI_IIDR:
        /* TODO: Return implementer ID etc. here */
        return 0;
    default:
        if (offset > V2M_IIDR0 && offset <= 0xFFC) {
            return 0;
        }

        qemu_log_mask(LOG_GUEST_ERROR,
                      "gicv2m_read: Bad offset %x\n", (int)offset);
        return 0;
    }
}

static void gicv2m_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    GICv2mState *s = (GICv2mState *)opaque;

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR, "gicv2m_write: bad size %u\n", size);
        return;
    }

    switch (offset) {
    case V2M_MSI_SETSPI_NS: {
        int spi;

        spi = (value & 0x3ff) - (s->base_spi + 32);
        if (spi < s->num_spi) {
            gicv2m_set_irq(s, spi, 1);
        }
        return;
    }
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "gicv2m_write: Bad offset %x\n", (int)offset);
        return;
    }
}

static const MemoryRegionOps gicv2m_ops = {
    .read = gicv2m_read,
    .write = gicv2m_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void gicv2m_realize(DeviceState *dev, Error **errp)
{
    GICv2mState *s = GICV2M(dev);
    int i;

    if (s->num_spi > GICV2M_NUM_SPI_MAX) {
        error_setg(errp,
                   "requested %u SPIs exceeds GICv2m frame maximum %d",
                   s->num_spi, GICV2M_NUM_SPI_MAX);
        return;
    }

    if (s->base_spi + 32 > 1020 - s->num_spi) {
        error_setg(errp,
                   "requested base SPI %u+%u exceeds max. number 1020",
                   s->base_spi + 32, s->num_spi);
        return;
    }

    for (i = 0; i < s->num_spi; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->spi[i]);
    }
}

static void gicv2m_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    GICv2mState *s = GICV2M(obj);

    memory_region_init_io(&s->iomem, OBJECT(s), &gicv2m_ops, s, "gicv2m", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static Property gicv2m_properties[] = {
    DEFINE_PROP_UINT32("base-spi", GICv2mState, base_spi, 0),
    DEFINE_PROP_UINT32("num-spi", GICv2mState, num_spi, 64),
    DEFINE_PROP_END_OF_LIST(),
};

static void gicv2m_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->props = gicv2m_properties;
    dc->realize = gicv2m_realize;
    //dc->vmsd = &vmstate_gicv2m;
}

static const TypeInfo gicv2m_info = {
    .name          = TYPE_GICV2M,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(GICv2mState),
    .instance_init = gicv2m_init,
    .class_init    = gicv2m_class_init,
};

static void gicv2m_register_types(void)
{
    type_register_static(&gicv2m_info);
}

type_init(gicv2m_register_types)
