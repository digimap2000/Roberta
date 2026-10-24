#include "axis_diag.h"

#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "pico/platform.h"

#include <stdint.h>
#include <stdio.h>

// Watchdog scratch 0-3 are free for applications; the SDK uses 4-7.
enum {
    AXIS_DIAG_WATCHDOG_MS = 2000,
    AXIS_DIAG_PHASE_MAGIC = 0xa7150000u,
    AXIS_DIAG_PHASE_MASK = 0x0000ffffu,
    AXIS_DIAG_FAULT_MAGIC = 0xfa17ed00u,
    AXIS_DIAG_PANIC_MAGIC = 0x9a41c000u,
    SCRATCH_PHASE = 0,
    SCRATCH_FAULT_PC = 1,
    SCRATCH_FAULT_LR = 2,
    SCRATCH_FAULT_MARK = 3,
};

typedef enum {
    RESET_POWER_ON,
    RESET_WATCHDOG,
    RESET_HARDFAULT,
    RESET_PANIC,
} ResetKind;

static ResetKind g_reset_kind;
static uint32_t g_reset_phase;
static uint32_t g_fault_pc;
static uint32_t g_fault_lr;

static const char *phase_name(uint32_t phase)
{
    switch (phase) {
    case AXIS_DIAG_PHASE_BOOT:
        return "boot";
    case AXIS_DIAG_PHASE_SERVICE:
        return "motion";
    case AXIS_DIAG_PHASE_REGISTERS:
        return "registers";
    case AXIS_DIAG_PHASE_CONSOLE:
        return "usb-console";
    case AXIS_DIAG_PHASE_NET:
        return "net";
    case AXIS_DIAG_PHASE_IDLE:
        return "idle";
    default:
        return "unknown";
    }
}

// Called from the hard-fault handler with the stacked exception frame:
// r0, r1, r2, r3, r12, lr, pc, xpsr.
void axis_diag_on_hardfault(const uint32_t *frame)
{
    watchdog_hw->scratch[SCRATCH_FAULT_PC] = frame[6];
    watchdog_hw->scratch[SCRATCH_FAULT_LR] = frame[5];
    watchdog_hw->scratch[SCRATCH_FAULT_MARK] = AXIS_DIAG_FAULT_MAGIC;
    watchdog_reboot(0, 0, 0);
    while (true) {
    }
}

// panic() (PICO_PANIC_FUNCTION) lands here with the message in r0 and the
// caller's return address on top of the stack. lwIP assertions arrive this
// way too, with their message as the format string.
void axis_diag_record_panic(const char *message, uint32_t caller)
{
    watchdog_hw->scratch[SCRATCH_FAULT_PC] = (uint32_t)message;
    watchdog_hw->scratch[SCRATCH_FAULT_LR] = caller;
    watchdog_hw->scratch[SCRATCH_FAULT_MARK] = AXIS_DIAG_PANIC_MAGIC;
    watchdog_reboot(0, 0, 0);
    while (true) {
    }
}

void __attribute__((naked)) axis_diag_panic(void)
{
    __asm volatile(
        "ldr r1, [sp]\n"
        "b axis_diag_record_panic\n");
}

static bool is_flash_string(uint32_t address)
{
    return address >= XIP_BASE && address < XIP_BASE + PICO_FLASH_SIZE_BYTES;
}

// Overrides the SDK's default handler, which would hang with the outputs in
// whatever state they were in.
void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b axis_diag_on_hardfault\n");
}

void axis_diag_start(void)
{
    const uint32_t phase_word = watchdog_hw->scratch[SCRATCH_PHASE];

    g_reset_phase = (phase_word & ~AXIS_DIAG_PHASE_MASK) == AXIS_DIAG_PHASE_MAGIC
        ? phase_word & AXIS_DIAG_PHASE_MASK
        : 0;

    if (watchdog_hw->scratch[SCRATCH_FAULT_MARK] == AXIS_DIAG_FAULT_MAGIC) {
        g_reset_kind = RESET_HARDFAULT;
        g_fault_pc = watchdog_hw->scratch[SCRATCH_FAULT_PC];
        g_fault_lr = watchdog_hw->scratch[SCRATCH_FAULT_LR];
    } else if (watchdog_hw->scratch[SCRATCH_FAULT_MARK] == AXIS_DIAG_PANIC_MAGIC) {
        g_reset_kind = RESET_PANIC;
        g_fault_pc = watchdog_hw->scratch[SCRATCH_FAULT_PC];
        g_fault_lr = watchdog_hw->scratch[SCRATCH_FAULT_LR];
    } else if (watchdog_enable_caused_reboot()) {
        g_reset_kind = RESET_WATCHDOG;
    } else {
        g_reset_kind = RESET_POWER_ON;
        g_reset_phase = 0;
    }

    watchdog_hw->scratch[SCRATCH_FAULT_MARK] = 0;
    axis_diag_mark(AXIS_DIAG_PHASE_BOOT);
    watchdog_enable(AXIS_DIAG_WATCHDOG_MS, true);

    if (g_reset_kind != RESET_POWER_ON) {
        char line[192];
        axis_diag_describe(line, sizeof(line));
        printf("previous run ended abnormally: %s\r\n", line);
    }
}

void axis_diag_mark(AxisDiagPhase phase)
{
    watchdog_hw->scratch[SCRATCH_PHASE] = AXIS_DIAG_PHASE_MAGIC | (uint32_t)phase;
}

void axis_diag_feed(void)
{
    watchdog_update();
}

void axis_diag_describe(char *buffer, size_t length)
{
    switch (g_reset_kind) {
    case RESET_POWER_ON:
        snprintf(buffer, length, "reset=power-on");
        break;
    case RESET_WATCHDOG:
        snprintf(buffer, length, "reset=watchdog phase=%s", phase_name(g_reset_phase));
        break;
    case RESET_HARDFAULT:
        snprintf(buffer, length, "reset=hardfault phase=%s pc=0x%08lx lr=0x%08lx",
                 phase_name(g_reset_phase), (unsigned long)g_fault_pc, (unsigned long)g_fault_lr);
        break;
    case RESET_PANIC:
        // The message lives in flash, so it is still readable after the reset.
        snprintf(buffer, length, "reset=panic phase=%s caller=0x%08lx msg=\"%s\"",
                 phase_name(g_reset_phase), (unsigned long)g_fault_lr,
                 is_flash_string(g_fault_pc) ? (const char *)g_fault_pc : "?");
        break;
    }
}
