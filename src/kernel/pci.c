/* DEBIL OS - minimal PCI scanner. Finds display controllers and reads BAR sizes. */
#include "pci.h"
#include "io.h"
#include "string.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static pci_gpu_t g_gpus[PCI_MAX_GPUS];
static uint32_t g_gpu_count = 0;
static uint32_t g_device_count = 0;

static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    uint32_t addr = 0x80000000u |
                    ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) |
                    ((uint32_t)func << 8) |
                    (off & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t value)
{
    uint32_t addr = 0x80000000u |
                    ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) |
                    ((uint32_t)func << 8) |
                    (off & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, value);
}

static uint32_t bar_size(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off)
{
    uint32_t original = pci_read32(bus, dev, func, off);
    if (original & 1u) return 0; /* I/O BAR */
    pci_write32(bus, dev, func, off, 0xFFFFFFFFu);
    uint32_t mask = pci_read32(bus, dev, func, off);
    pci_write32(bus, dev, func, off, original);
    if (mask == 0 || mask == 0xFFFFFFFFu) return 0;
    mask &= ~0xFu;
    return (~mask) + 1u;
}

void pci_init(void)
{
    g_gpu_count = 0;
    g_device_count = 0;

    for (uint32_t bus = 0; bus < 256 && g_gpu_count < PCI_MAX_GPUS; bus++) {
        for (uint32_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xFFFF);
            if (vendor == 0xFFFF || vendor == 0) continue;

            g_device_count++;

            uint32_t classreg = pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0x08);
            uint8_t class_code = (uint8_t)((classreg >> 24) & 0xFF);
            uint8_t subclass   = (uint8_t)((classreg >> 16) & 0xFF);

            if (class_code == 0x03 && g_gpu_count < PCI_MAX_GPUS) {
                pci_gpu_t *g = &g_gpus[g_gpu_count++];
                g->bus = (uint8_t)bus;
                g->device = (uint8_t)dev;
                g->function = 0;
                g->vendor = vendor;
                g->device_id = (uint16_t)((id >> 16) & 0xFFFF);
                g->class_code = class_code;
                g->subclass = subclass;
                g->bar0 = pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0x10) & ~0xFu;
                g->bar0_size = bar_size((uint8_t)bus, (uint8_t)dev, 0, 0x10);
                g->bar2 = pci_read32((uint8_t)bus, (uint8_t)dev, 0, 0x18) & ~0xFu;
                g->bar2_size = bar_size((uint8_t)bus, (uint8_t)dev, 0, 0x18);
            }
        }
    }
}

uint32_t pci_device_count(void) { return g_device_count; }
uint32_t pci_gpu_count(void)    { return g_gpu_count; }

const pci_gpu_t *pci_gpu_get(uint32_t index)
{
    if (index >= g_gpu_count) return 0;
    return &g_gpus[index];
}

const char *pci_vendor_name(uint16_t vendor)
{
    switch (vendor) {
    case 0x1234: return "QEMU/Bochs";
    case 0x8086: return "Intel";
    case 0x10DE: return "NVIDIA";
    case 0x1002: return "AMD/ATI";
    case 0x15AD: return "VMware";
    case 0x80EE: return "VirtualBox";
    case 0x1AF4: return "Red Hat/Virtio";
    case 0x1B36: return "Red Hat/QEMU";
    case 0x1013: return "Cirrus Logic";
    case 0x5333: return "S3 Graphics";
    case 0x1A03: return "ASPEED";
    case 0x102B: return "Matrox";
    default:     return "unknown vendor";
    }
}

const char *pci_class_name(uint8_t class_code, uint8_t subclass)
{
    switch (class_code) {
    case 0x00: return "legacy";
    case 0x01: return "storage";
    case 0x02: return "network";
    case 0x03: return subclass == 0x00 ? "VGA display" : "display controller";
    case 0x04: return "multimedia";
    case 0x05: return "memory";
    case 0x06: return "bridge";
    case 0x07: return "serial bus";
    case 0x08: return "system";
    case 0x09: return "input";
    case 0x0A: return "docking";
    case 0x0B: return "processor";
    case 0x0C: return "serial bus";
    case 0x0D: return "wireless";
    case 0x0E: return "intelligent I/O";
    case 0x0F: return "satellite";
    case 0x10: return "encryption";
    case 0x11: return "signal processing";
    default:   return "other";
    }
}
