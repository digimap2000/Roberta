#pragma once

#include <stddef.h>

// Where the main loop is, recorded in a watchdog scratch register so that
// after a watchdog reset the board can say what it was doing when it hung.
typedef enum {
    AXIS_DIAG_PHASE_BOOT = 1,
    AXIS_DIAG_PHASE_SERVICE,
    AXIS_DIAG_PHASE_REGISTERS,
    AXIS_DIAG_PHASE_CONSOLE,
    AXIS_DIAG_PHASE_NET,
    AXIS_DIAG_PHASE_IDLE,
} AxisDiagPhase;

// Captures why the last reset happened, then starts the hardware watchdog.
// Call once initialisation is complete; the loop must call axis_diag_feed.
void axis_diag_start(void);
void axis_diag_mark(AxisDiagPhase phase);
void axis_diag_feed(void);

// One line describing the last reset, e.g. "reset=watchdog phase=net".
void axis_diag_describe(char *buffer, size_t length);
