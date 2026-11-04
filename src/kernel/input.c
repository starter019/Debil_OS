/* DEBIL OS - PS/2 set-1 keyboard and serial keyboard input */
#include "input.h"
#include "io.h"
#include "serial.h"
#include "timer.h"

#define KEY_QUEUE_SIZE 256

static volatile int g_queue[KEY_QUEUE_SIZE];
static volatile uint32_t g_qhead = 0;
static volatile uint32_t g_qtail = 0;

static int g_shift = 0;
static int g_ctrl = 0;
static int g_caps = 0;
static int g_extended = 0;

static const char keymap[128] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char shiftmap[128] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void queue_push(int key)
{
    uint32_t next = (g_qhead + 1) % KEY_QUEUE_SIZE;
    if (next != g_qtail) {
        g_queue[g_qhead] = key;
        g_qhead = next;
    }
}

static int queue_pop(void)
{
    if (g_qtail == g_qhead) return 0;
    int key = g_queue[g_qtail];
    g_qtail = (g_qtail + 1) % KEY_QUEUE_SIZE;
    return key;
}

void input_init(void)
{
    g_qhead = g_qtail = 0;
    g_shift = g_ctrl = g_caps = g_extended = 0;
    /* Drain anything the BIOS left in the controller. */
    for (int i = 0; i < 16; i++) {
        if (!(inb(0x64) & 0x01)) break;
        (void)inb(0x60);
    }
}

static void handle_scancode(uint8_t sc)
{
    if (sc == 0xE0) {
        g_extended = 1;
        return;
    }

    if (g_extended) {
        g_extended = 0;
        if (sc & 0x80) return; /* key release */
        switch (sc) {
        case 0x48: queue_push(KEY_UP); break;
        case 0x50: queue_push(KEY_DOWN); break;
        case 0x4B: queue_push(KEY_LEFT); break;
        case 0x4D: queue_push(KEY_RIGHT); break;
        case 0x53: queue_push(KEY_DELETE); break;
        case 0x47: queue_push(KEY_HOME); break;
        case 0x4F: queue_push(KEY_END); break;
        case 0x49: queue_push(KEY_PAGEUP); break;
        case 0x51: queue_push(KEY_PAGEDOWN); break;
        case 0x1C: queue_push(KEY_ENTER); break;
        default: break;
        }
        return;
    }

    if (sc == 0x2A || sc == 0x36) { g_shift = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { g_shift = 0; return; }
    if (sc == 0x1D) { g_ctrl = 1; return; }
    if (sc == 0x9D) { g_ctrl = 0; return; }
    if (sc == 0x3A) { g_caps = !g_caps; return; }

    if (sc & 0x80) return;

    if (sc == 0x01) { queue_push(KEY_ESC); return; }
    if (sc == 0x0E) { queue_push(KEY_BACKSPACE); return; }
    if (sc == 0x0F) { queue_push(KEY_TAB); return; }
    if (sc == 0x1C) { queue_push(KEY_ENTER); return; }
    if (sc == 0x3B) { queue_push(KEY_F1); return; }
    if (sc == 0x3C) { queue_push(KEY_F2); return; }
    if (sc == 0x3D) { queue_push(KEY_F3); return; }
    if (sc == 0x3E) { queue_push(KEY_F4); return; }

    char ch = g_shift ? shiftmap[sc] : keymap[sc];
    if (!ch) return;

    if (ch >= 'a' && ch <= 'z' && g_caps) ch = (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z' && g_caps) ch = (char)(ch - 'A' + 'a');

    if (g_ctrl) {
        if (ch == 'c' || ch == 'C') { queue_push(KEY_CTRLC); return; }
        if (ch == 'l' || ch == 'L') { queue_push(0x0C); return; }
        return;
    }

    queue_push((int)(unsigned char)ch);
}

void input_keyboard_irq(void)
{
    uint8_t sc = inb(0x60);
    handle_scancode(sc);
}

/* Parse ANSI escape sequences coming from a serial terminal. */
static int g_ser_state = 0;

static int serial_poll(void)
{
    while (serial_available()) {
        unsigned char c = (unsigned char)serial_read();

        switch (g_ser_state) {
        case 0:
            if (c == 0x1B) { g_ser_state = 1; break; }
            if (c == '\r' || c == '\n') return KEY_ENTER;
            if (c == 0x7F || c == 0x08) return KEY_BACKSPACE;
            if (c == 0x03) return KEY_CTRLC;
            if (c == '\t') return KEY_TAB;
            if (c >= 32 && c < 127) return (int)c;
            break;
        case 1:
            if (c == '[' || c == 'O') { g_ser_state = 2; break; }
            g_ser_state = 0;
            return KEY_ESC;
        case 2:
            g_ser_state = 0;
            switch (c) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            case '3': g_ser_state = 3; break;
            case '5': g_ser_state = 4; break;
            case '6': g_ser_state = 5; break;
            default: break;
            }
            break;
        case 3:
            g_ser_state = 0;
            if (c == '~') return KEY_DELETE;
            break;
        case 4:
            g_ser_state = 0;
            if (c == '~') return KEY_PAGEUP;
            break;
        case 5:
            g_ser_state = 0;
            if (c == '~') return KEY_PAGEDOWN;
            break;
        }
    }
    return 0;
}

int input_poll(void)
{
    int key = queue_pop();
    if (key) return key;
    return serial_poll();
}

int input_available(void)
{
    if (g_qtail != g_qhead) return 1;
    if (serial_available()) return 1;
    return 0;
}

int input_wait(void)
{
    for (;;) {
        int key = input_poll();
        if (key) return key;
        cpu_idle();
    }
}
