/*
 * Virtio Test Device
 *
 * Copyright (C) 2013 Red Hat, Inc.
 * Copyright (C) 2013 Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */
#include "hw/virtio/virtio-bus.h"

#define VIRTIO_ID_TESTDEV 0xffff

#define TYPE_VIRTIO_TESTDEV "virtio-testdev"
#define VIRTIO_TESTDEV(obj) \
        OBJECT_CHECK(VirtIOTestdev, (obj), TYPE_VIRTIO_TESTDEV)

#define TESTDEV_MAJOR_VER 1
#define TESTDEV_MINOR_VER 1

/*
 * Size of the register bank in bytes. The max is determined
 * by the device's config space. For virtio-mmio devices we
 * only have 256 bytes, so that's our max, but we don't need
 * that much anyway. 64 bytes gives us token, nargs, nrets,
 * and an additional 13 4-byte registers for input/output.
 * That'll do.
 */
#define CONFIG_SIZE 64

enum {
    VERSION = 1,
    CLEAR,
    EXIT,
};

#define TOKEN_OFFSET            0x0
#define NARGS_OFFSET            0x4
#define NRETS_OFFSET            0x8
#define ARG_OFFSET(n)           (0xc + (n) * 4)
#define __RET_OFFSET(nargs, n)  (ARG_OFFSET(nargs) + (n) * 4)
#define RET_OFFSET(d, n)        __RET_OFFSET(config_readl(d, NARGS_OFFSET), n)

typedef struct VirtIOTestdev {
    VirtIODevice parent_obj;
    uint8_t config[CONFIG_SIZE];
} VirtIOTestdev;

static uint32_t config_readl(VirtIOTestdev *dev, unsigned offset)
{
    uint32_t *config = (uint32_t *)&dev->config[0];

    if (offset > (CONFIG_SIZE - 4)) {
        return 0;
    }

    return le32_to_cpu(config[offset / 4]);
}

static void config_writel(VirtIOTestdev *dev, unsigned offset, uint32_t val)
{
    uint32_t *config = (uint32_t *)&dev->config[0];

    if (offset <= (CONFIG_SIZE - 4)) {
        config[offset / 4] = cpu_to_le32(val);
    }
}

static void virtio_testdev_get_config(VirtIODevice *vdev, uint8_t *config_data)
{
    VirtIOTestdev *dev = VIRTIO_TESTDEV(vdev);
    memcpy(config_data, &dev->config[0], CONFIG_SIZE);
}

static void virtio_testdev_set_config(VirtIODevice *vdev,
                                      const uint8_t *config_data)
{
    VirtIOTestdev *dev = VIRTIO_TESTDEV(vdev);
    uint32_t token;

    memcpy(&dev->config[0], config_data, CONFIG_SIZE);

    token = config_readl(dev, TOKEN_OFFSET);

    /*
     * The token register must always remain zero in order to
     * avoid re-executing operations while new operation
     * arguments are being filled in.
     */
    config_writel(dev, TOKEN_OFFSET, 0);

    switch (token) {
    case 0:
        /*
         * No token yet, so we were just filling in arguments, and
         * now there's nothing left to do.
         */
        return;
    case VERSION:
        config_writel(dev, RET_OFFSET(dev, 0),
            (TESTDEV_MAJOR_VER << 16) | TESTDEV_MINOR_VER);
        break;
    case EXIT:
        exit((config_readl(dev, ARG_OFFSET(0)) << 1) | 1);
    case CLEAR:
    default:
        /* The CLEAR op and unknown ops reset all registers */
        memset(&dev->config[0], 0, CONFIG_SIZE);
    }
}

static uint32_t virtio_testdev_get_features(VirtIODevice *vdev, uint32_t f)
{
    return f;
}

static void virtio_testdev_realize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    virtio_init(vdev, "virtio-testdev", VIRTIO_ID_TESTDEV, CONFIG_SIZE);
}

static void virtio_testdev_unrealize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    virtio_cleanup(vdev);
}

static void virtio_testdev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    dc->unrealize = virtio_testdev_unrealize;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    vdc->realize = virtio_testdev_realize;
    vdc->get_config = virtio_testdev_get_config;
    vdc->set_config = virtio_testdev_set_config;
    vdc->get_features = virtio_testdev_get_features;
}

static const TypeInfo virtio_testdev_info = {
    .name = TYPE_VIRTIO_TESTDEV,
    .parent = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtIOTestdev),
    .class_init = virtio_testdev_class_init,
};

static void virtio_register_types(void)
{
    type_register_static(&virtio_testdev_info);
}

type_init(virtio_register_types)
