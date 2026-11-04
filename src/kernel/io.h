/* DEBIL OS - x86 port and control register helpers */
#ifndef DEBIL_IO_H
#define DEBIL_IO_H

#include "types.h"

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

static inline void cpu_cli(void) { __asm__ volatile("cli"); }
static inline void cpu_sti(void) { __asm__ volatile("sti"); }
static inline void cpu_hlt(void) { __asm__ volatile("hlt"); }

static inline void cpu_wait_interrupt(void)
{
    __asm__ volatile("sti; hlt");
}

static inline uint32_t read_cr0(void)
{
    uint32_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v;
}
static inline uint32_t read_cr2(void)
{
    uint32_t v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v;
}
static inline uint32_t read_cr3(void)
{
    uint32_t v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v;
}
static inline uint32_t read_cr4(void)
{
    uint32_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v;
}

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#endif
