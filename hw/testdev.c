#include <sys/mman.h>
#include "sysbus.h"
#include "hw.h"
#include "qdev.h"

typedef struct {
    SysBusDevice busdev;
    MemoryRegion iomem;
    qemu_irq irq;
    CharDriverState *chr;
} testdev_state;

static void testdev_serial_write(void *opaque, target_phys_addr_t offset,
                                 uint64_t value, unsigned size)
{
    char chr;

    memcpy(&chr, &value, 1);
    printf("%c", chr);
}

static char *iomem_buf;

static uint64_t testdev_read(void *opaque, target_phys_addr_t offset,
                             unsigned size)
{
    if (offset > 0x100) {
        uint64_t value;
        memcpy(&value, iomem_buf + offset, size);
        return value;
    } else {
        switch (offset >> 2) {
        case 0: /* read ramsize */
            return 0x10000; /* TODO: Fix! */
        default:
            hw_error("testdev_read: Bad offset %x\n", (int)offset);
            return 0;
        }
    }
}

static void testdev_write(void *opaque, target_phys_addr_t offset,
                          uint64_t value, unsigned size)
{
    if (offset > 0x100) {
        memcpy(iomem_buf + offset, &value, size);
    } else {
        switch (offset >> 2) {
        case 0: /* char write */
            testdev_serial_write(opaque, offset, value, size);
            break;
        case 1: /* unexpected error */
            printf("testdev_write: unexpected error\n");
            break;
        case 2: /* ping (ok, success, etc.) */
            printf("testdev_write: ping\n");
            break;
        case 42: /* exit (goodbye and thanks for all the fish */
            exit(value);
            break;
        default:
            hw_error("testdev_write: Bad offset %x\n", (int)offset);
        }
    }
}

static const MemoryRegionOps testdev_ops = {
    .read = testdev_read,
    .write = testdev_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int testdev_init(SysBusDevice *dev)
{
    testdev_state *s = FROM_SYSBUS(testdev_state, dev);

    memory_region_init_io(&s->iomem, &testdev_ops, s, "testdev", 0x10000);
    sysbus_init_mmio(dev, &s->iomem);
    sysbus_init_irq(dev, &s->irq);

    iomem_buf = g_malloc0(0x10000);
    return 0;
}

static void testdev_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);
    sdc->init = testdev_init;
}

static TypeInfo testdev_info = {
    .name          = "testdev",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(testdev_state),
    .class_init    = testdev_class_init,
};

static void testdev_register_type(void)
{
    type_register_static(&testdev_info);
}

type_init(testdev_register_type)
