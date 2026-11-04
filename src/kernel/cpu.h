/* DEBIL OS - CPUID identification and a rough TSC frequency measurement */
#ifndef DEBIL_CPU_H
#define DEBIL_CPU_H

#include "types.h"

void cpu_init(void);
const char *cpu_vendor(void);
const char *cpu_brand(void);
uint32_t cpu_cores(void);
uint32_t cpu_mhz(void);
uint32_t cpu_features_edx(void);
uint32_t cpu_features_ecx(void);

#endif
