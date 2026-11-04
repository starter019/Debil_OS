/* DEBIL OS - CPU detection */
#include "cpu.h"
#include "timer.h"
#include "string.h"
#include "io.h"

static char g_vendor[13];
static char g_brand[49];
static uint32_t g_cores = 1;
static uint32_t g_mhz = 0;
static uint32_t g_feat_edx = 0;
static uint32_t g_feat_ecx = 0;

static void cpuid(uint32_t leaf, uint32_t sub,
                  uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid"
                     : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(sub));
}

void cpu_init(void)
{
    uint32_t a, b, c, d;

    cpuid(0, 0, &a, &b, &c, &d);
    memcpy(g_vendor + 0, &b, 4);
    memcpy(g_vendor + 4, &d, 4);
    memcpy(g_vendor + 8, &c, 4);
    g_vendor[12] = 0;

    uint32_t max_leaf = a;
    if (max_leaf >= 1) {
        cpuid(1, 0, &a, &b, &c, &d);
        g_feat_ecx = c;
        g_feat_edx = d;
        g_cores = (b >> 16) & 0xFF;
        if (g_cores == 0) g_cores = 1;
    }

    g_brand[0] = 0;
    cpuid(0x80000000u, 0, &a, &b, &c, &d);
    if (a >= 0x80000004u) {
        uint32_t *out = (uint32_t *)g_brand;
        cpuid(0x80000002u, 0, &out[0], &out[1], &out[2], &out[3]);
        cpuid(0x80000003u, 0, &out[4], &out[5], &out[6], &out[7]);
        cpuid(0x80000004u, 0, &out[8], &out[9], &out[10], &out[11]);
        g_brand[48] = 0;
        /* Trim leading spaces. */
        char *p = g_brand;
        while (*p == ' ') p++;
        if (p != g_brand) memmove(g_brand, p, strlen(p) + 1);
        str_trim(g_brand);
    }

    /* Rough TSC frequency: cycles during a 50 ms PIT delay. */
    uint64_t t0 = rdtsc();
    timer_sleep_ms(50);
    uint64_t t1 = rdtsc();
    uint32_t delta = (uint32_t)(t1 - t0);
    g_mhz = (delta + 25000u) / 50000u;
}

const char *cpu_vendor(void) { return g_vendor[0] ? g_vendor : "unknown"; }
const char *cpu_brand(void)  { return g_brand[0] ? g_brand : "unknown x86 CPU"; }
uint32_t cpu_cores(void)     { return g_cores; }
uint32_t cpu_mhz(void)       { return g_mhz; }
uint32_t cpu_features_edx(void) { return g_feat_edx; }
uint32_t cpu_features_ecx(void) { return g_feat_ecx; }
