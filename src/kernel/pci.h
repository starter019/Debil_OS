/* DEBIL OS - minimal PCI configuration-space scanner (display devices) */
#ifndef DEBIL_PCI_H
#define DEBIL_PCI_H

#include "types.h"

#define PCI_MAX_GPUS 4

typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint32_t bar0;
    uint32_t bar0_size;
    uint32_t bar2;
    uint32_t bar2_size;
} pci_gpu_t;

void pci_init(void);
uint32_t pci_device_count(void);
uint32_t pci_gpu_count(void);
const pci_gpu_t *pci_gpu_get(uint32_t index);
const char *pci_vendor_name(uint16_t vendor);
const char *pci_class_name(uint8_t class_code, uint8_t subclass);

#endif
