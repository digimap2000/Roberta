#!/usr/bin/env python3
"""Simulated WiFi axis controller for exercising Roberta Control without hardware.

Speaks the axis firmware's TCP console (port 7300 by default): the register
bank, `reg readm`, the motion aliases, `ping` and `net status`, plus the link
watchdog. On macOS it also advertises itself over Bonjour as
`_roberta-axis._tcp`, so the app discovers it like a real board.

    python3 tools/dev/fake_axis.py --name sim-j1
    python3 tools/dev/fake_axis.py --name sim-j2 --port 7301

Run several with different names and ports to try multi-axis work.
"""

import argparse
import asyncio
import ctypes
import pathlib
import random
import shutil
import subprocess
import tempfile
import time

PROMPT = "axis> "

# (address, name, writable)
REGISTERS = [
    (0x0000, "PROTOCOL_VERSION", False),
    (0x0001, "DEVICE_TYPE", False),
    (0x0002, "AXIS_ID", True),
    (0x0003, "CAPABILITY_WORD", False),
    (0x0010, "CONTROL_WORD", True),
    (0x0011, "STATUS_WORD", False),
    (0x0012, "FAULT_WORD", False),
    (0x0013, "MODE_WORD", True),
    (0x0014, "INPUT_WORD", False),
    (0x0015, "OUTPUT_WORD", False),
    (0x0016, "STAGED_COMMAND_COUNTER", True),
    (0x0017, "ACTIVE_COMMAND_COUNTER", False),
    (0x0018, "EXECUTE_AT_TICK_US", True),
    (0x0019, "SEGMENT_DURATION_US", True),
    (0x0020, "STAGED_TARGET_POSITION_STEPS", True),
    (0x0021, "ACTIVE_TARGET_POSITION_STEPS", False),
    (0x0022, "CURRENT_POSITION_STEPS", False),
    (0x0023, "STAGED_TARGET_VELOCITY_STEPS_S", True),
    (0x0024, "ACTIVE_TARGET_VELOCITY_STEPS_S", False),
    (0x0025, "CURRENT_VELOCITY_STEPS_S", False),
    (0x0026, "STAGED_TARGET_ACCEL_STEPS_S2", True),
    (0x0027, "ACTIVE_TARGET_ACCEL_STEPS_S2", False),
    (0x0028, "STAGED_HOME_VELOCITY_STEPS_S", True),
    (0x0029, "ACTIVE_HOME_VELOCITY_STEPS_S", False),
    (0x002A, "HOME_OFFSET_STEPS", True),
    (0x002B, "STAGED_RELATIVE_MOVE_STEPS", True),
    (0x0030, "FAN_LEVEL", True),
    (0x0031, "BRAKE_HOLD_ENABLE", True),
    (0x0032, "BRAKE_RELEASE_DELAY_MS", True),
    (0x0033, "BRAKE_APPLY_DELAY_MS", True),
    (0x0040, "STEPS_PER_MOTOR_REV", True),
    (0x0041, "MOTOR_REV_PER_JOINT_REV_NUMERATOR", True),
    (0x0042, "MOTOR_REV_PER_JOINT_REV_DENOMINATOR", True),
    (0x0043, "JOINT_DEGREES_PER_REV_Q12_20", True),
    (0x0044, "JOINT_BACKLASH_DEG_Q12_20", True),
    (0x0045, "SOFT_LIMIT_MIN_STEPS", True),
    (0x0046, "SOFT_LIMIT_MAX_STEPS", True),
    (0x0047, "JOINT_POSITION_DEG_Q12_20", False),
    (0x0048, "STAGED_JOINT_TARGET_DEG_Q12_20", True),
    (0x0049, "ACTIVE_JOINT_TARGET_DEG_Q12_20", False),
    (0x0050, "STEP_COUNT", False),
    (0x0051, "UPTIME_MS", False),
    (0x0052, "LOCAL_TICK_US", False),
    (0x0053, "LAST_ERROR_CODE", False),
    (0x0054, "LAST_SYNC_RX_TICK_US", False),
    (0x0055, "SYNC_WATCHDOG_TIMEOUT_MS", True),
    (0x0056, "SYNC_STATUS_WORD", False),
    (0x0057, "LAST_START_TICK_US", False),
    (0x0060, "LIMIT_MAX_VELOCITY_STEPS_S", True),
    (0x0061, "LIMIT_MAX_ACCEL_STEPS_S2", True),
    (0x0062, "LIMIT_MAX_DECEL_STEPS_S2", True),
    (0x0063, "LIMIT_MAX_JERK_STEPS_S3", True),
    (0x0064, "LIMIT_QUICK_STOP_DECEL_STEPS_S2", True),
    (0x0065, "MOTION_LIMITS_STATUS", False),
    (0x0066, "MOTION_SMOOTHING_MS", False),
]
LIMIT_REGISTERS = [0x0060, 0x0061, 0x0062, 0x0063, 0x0064]
# The firmware's uncommissioned defaults.
DEFAULT_LIMITS = [2000, 4000, 4000, 80000, 8000]
LIMITS_COMMISSIONED = 1 << 0
LIMITS_ADJUSTED = 1 << 1
BY_NAME = {name: (address, writable) for address, name, writable in REGISTERS}
BY_ADDRESS = {address: (name, writable) for address, name, writable in REGISTERS}

CONTROL_OUTPUT_ENABLE = 1 << 0
CONTROL_MOTOR_ENABLE = 1 << 1
CONTROL_MOVE_ABSOLUTE = 1 << 2
CONTROL_MOVE_RELATIVE = 1 << 3
CONTROL_HOME = 1 << 4
CONTROL_STOP = 1 << 5
CONTROL_ESTOP = 1 << 6
CONTROL_CLEAR_FAULT = 1 << 7
CONTROL_SET_ZERO = 1 << 8
CONTROL_SAVE_CONFIG = 1 << 10
CONTROL_RELOAD_CONFIG = 1 << 11
CONTROL_KILL = 1 << 12

FAULT_MOTION_WHILE_DISABLED = 1 << 1
FAULT_MOVE_BEFORE_HOMED = 1 << 2
FAULT_MOTION_ABORTED = 1 << 7
FAULT_KILLED = 1 << 8

FAULT_INVALID_COMMAND = 1 << 0

STATUS_START_SCHEDULED = 1 << 14
SCHEDULE_MAX_LEAD_US = 5_000_000

WATCHDOG_S = 1.0
HOME_SWITCH_STEPS = -400  # where the simulated home switch sits


# The firmware's motion governor, compiled from its source and driven
# through ctypes, so the fake moves exactly as a board does.
GOVERNOR_SOURCE = pathlib.Path(__file__).resolve().parents[3] / "firmware/axiscontroller/app/axis_motion.c"
FILTER_MAX = 2048
POSITION_MODE = 1
MAX_STEP_RATE = 20000.0


class MotionLimits(ctypes.Structure):
    _fields_ = [(name, ctypes.c_float) for name in
                ("max_velocity", "max_accel", "max_decel", "max_jerk", "quick_stop_decel")]


class MotionState(ctypes.Structure):
    _fields_ = [
        ("limits", MotionLimits),
        ("mode", ctypes.c_int),
        ("limits_pending", ctypes.c_bool),
        ("pending_limits", MotionLimits),
        ("position", ctypes.c_double),
        ("velocity", ctypes.c_float),
        ("accel", ctypes.c_float),
        ("target_position", ctypes.c_double),
        ("target_velocity", ctypes.c_float),
        ("cruise_velocity", ctypes.c_float),
        ("request_accel", ctypes.c_float),
        ("request_decel", ctypes.c_float),
        ("soft_limits_enabled", ctypes.c_bool),
        ("soft_min", ctypes.c_double),
        ("soft_max", ctypes.c_double),
        ("plan_position", ctypes.c_double),
        ("plan_velocity", ctypes.c_double),
        ("window", ctypes.c_int),
        ("head", ctypes.c_int),
        ("settled_updates", ctypes.c_int),
        ("sum", ctypes.c_double),
        ("fresh_sum", ctypes.c_double),
        ("history", ctypes.c_double * (FILTER_MAX + 1)),
    ]


def load_governor():
    library = pathlib.Path(tempfile.gettempdir()) / "roberta-fake-axis" / "libaxis_motion.so"
    if not library.exists() or library.stat().st_mtime < GOVERNOR_SOURCE.stat().st_mtime:
        library.parent.mkdir(exist_ok=True)
        subprocess.run(["cc", "-O2", "-shared", "-fPIC", "-o", str(library), str(GOVERNOR_SOURCE)], check=True)
    lib = ctypes.CDLL(str(library))
    lib.axis_motion_sanitise_limits.restype = ctypes.c_bool
    lib.axis_motion_is_idle.restype = ctypes.c_bool
    lib.axis_motion_init.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_double]
    lib.axis_motion_sanitise_limits.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.axis_motion_set_soft_limits.argtypes = [ctypes.c_void_p, ctypes.c_bool, ctypes.c_double, ctypes.c_double]
    lib.axis_motion_move_to.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_float, ctypes.c_float,
                                        ctypes.c_float]
    lib.axis_motion_run.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float]
    lib.axis_motion_stop.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.axis_motion_update.argtypes = [ctypes.c_void_p, ctypes.c_float]
    return lib


GOVERNOR = load_governor()


class Governor:
    def __init__(self, limits):
        self.state = MotionState()
        self.ref = ctypes.byref(self.state)
        usable = MotionLimits(*limits)
        GOVERNOR.axis_motion_sanitise_limits(ctypes.byref(usable), MAX_STEP_RATE)
        GOVERNOR.axis_motion_init(self.ref, ctypes.byref(usable), 0.0)

    def set_limits(self, limits):
        usable = MotionLimits(*limits)
        unchanged = GOVERNOR.axis_motion_sanitise_limits(ctypes.byref(usable), MAX_STEP_RATE)
        GOVERNOR.axis_motion_set_limits(self.ref, ctypes.byref(usable))
        return unchanged

    def set_soft_limits(self, enabled, low, high):
        GOVERNOR.axis_motion_set_soft_limits(self.ref, enabled, low, high)

    def move_to(self, target, velocity, accel, decel):
        GOVERNOR.axis_motion_move_to(self.ref, target, velocity, accel, decel)

    def run(self, velocity, accel, decel):
        GOVERNOR.axis_motion_run(self.ref, velocity, accel, decel)

    def stop(self, decel):
        GOVERNOR.axis_motion_stop(self.ref, decel)

    def halt_at(self, position):
        self.state.position = position
        GOVERNOR.axis_motion_halt(self.ref)

    def update(self):
        GOVERNOR.axis_motion_update(self.ref, 0.001)

    idle = property(lambda self: GOVERNOR.axis_motion_is_idle(self.ref))
    position = property(lambda self: self.state.position)
    velocity = property(lambda self: self.state.velocity)
    target = property(lambda self: self.state.target_position)
    mode = property(lambda self: self.state.mode)
    window = property(lambda self: self.state.window)
    max_decel = property(lambda self: self.state.limits.max_decel)
    quick_stop_decel = property(lambda self: self.state.limits.quick_stop_decel)


def u32(value):
    return value & 0xFFFFFFFF


def i32(value):
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


class Axis:
    def __init__(self):
        # Survives simulated reboots, like the board's flash.
        self.flash = None
        self.reset_state("power-on")

    def reset_state(self, reset_reason):
        """Fresh boot: drive off, position zero, like a real board after a reset."""
        self.reset_reason = reset_reason
        self.started = time.monotonic()
        # Each simulated board's tick starts somewhere random, like real boards.
        self.tick_offset = random.randrange(1 << 32)
        self.scheduled = None  # (handle, bits, target, relative)
        self.last_start_tick = 0
        self.stored = {address: 0 for address, _, _ in REGISTERS}
        self.stored.update({
            0x0000: 1,
            0x0001: 1,
            0x0003: 0x4F,
            0x0023: 400,
            0x0026: 800,
            0x0028: 150,
            0x0040: 200,
            0x0041: 1,
            0x0042: 1,
            0x0043: 360 << 20,
            0x0055: 100,
        })
        self.stored.update(zip(LIMIT_REGISTERS, DEFAULT_LIMITS))
        self.limits_status = 0
        self.levels = CONTROL_OUTPUT_ENABLE
        # Drive enable as it is; dropping it waits for the axis to rest.
        self.drive_on = False
        self.raw_steps = 0
        self.offset = 0
        self.homing = False
        self.homed = False
        self.steps = 0
        self.fault = 0
        self.last_error = 0
        self.leftover = 0.0
        self.governor = Governor(self.limits())
        self.load_config()

    @property
    def enabled(self):
        return bool(self.levels & CONTROL_OUTPUT_ENABLE and self.levels & CONTROL_MOTOR_ENABLE)

    @property
    def moving(self):
        return not self.governor.idle or round(self.governor.position) != self.raw_steps

    @property
    def position(self):
        return self.raw_steps + self.offset

    @property
    def target(self):
        if self.governor.mode == POSITION_MODE:
            return round(self.governor.target) + self.offset
        return self.position

    @property
    def velocity(self):
        return self.governor.velocity

    def limits(self):
        return [self.stored[address] for address in LIMIT_REGISTERS]

    def apply_limits(self):
        if self.governor.set_limits(self.limits()):
            self.limits_status &= ~LIMITS_ADJUSTED
        else:
            self.limits_status |= LIMITS_ADJUSTED

    def apply_soft_limits(self):
        low, high = i32(self.stored[0x0045]), i32(self.stored[0x0046])
        self.governor.set_soft_limits(low < high and not self.homing, low - self.offset, high - self.offset)

    def load_config(self):
        if self.flash:
            self.stored.update(self.flash)
            self.limits_status |= LIMITS_COMMISSIONED
        self.apply_limits()
        self.apply_soft_limits()

    def local_tick(self):
        return (int(time.monotonic() * 1e6) + self.tick_offset) & 0xFFFFFFFF

    def status_word(self):
        word = (CONTROL_OUTPUT_ENABLE if self.levels & CONTROL_OUTPUT_ENABLE else 0)
        word |= CONTROL_MOTOR_ENABLE if self.drive_on else 0
        word |= (1 << 2) if self.moving else 0
        word |= (1 << 3) if self.homing else 0
        word |= (1 << 4) if self.homed else 0
        word |= (1 << 5) if self.position == self.target and not self.moving else 0
        word |= (1 << 6) if self.fault else 0
        word |= (1 << 7) if self.raw_steps <= HOME_SWITCH_STEPS else 0
        word |= STATUS_START_SCHEDULED if self.scheduled else 0
        return word

    def read(self, address):
        live = {
            0x0010: self.levels,
            0x0011: self.status_word(),
            0x0012: self.fault,
            0x0021: self.target,
            0x0022: self.position,
            0x0025: abs(round(self.velocity)),
            0x0050: self.steps,
            0x0051: int((time.monotonic() - self.started) * 1000),
            0x0052: self.local_tick(),
            0x0053: self.last_error,
            0x0057: self.last_start_tick,
            0x0065: self.limits_status,
            0x0066: self.governor.window,
        }
        return u32(live.get(address, self.stored[address]))

    def write(self, address, value):
        if address == 0x0010:
            if value & CONTROL_KILL:
                self.kill()
                value &= ~CONTROL_MOTOR_ENABLE
            self.levels = value & 0x3
            if not self.enabled:
                self.cancel_scheduled()
                if self.moving:
                    self.homing = False
                    self.governor.stop(self.governor.quick_stop_decel)
                else:
                    self.drive_on = False
            elif not self.drive_on:
                self.drive_on = True
            self.action(value & ~0x3)
        elif address in LIMIT_REGISTERS:
            if value == 0:
                self.raise_fault(FAULT_INVALID_COMMAND, 1)
                return "invalid-value"
            self.stored[address] = u32(value)
            self.apply_limits()
        else:
            self.stored[address] = u32(value)
            if address in (0x0045, 0x0046):
                self.apply_soft_limits()
        return "ok"

    def raise_fault(self, bit, code):
        self.fault |= bit
        self.last_error = code

    def kill(self):
        """Drive off at once; the position is no longer trusted."""
        self.cancel_scheduled()
        self.levels &= ~CONTROL_MOTOR_ENABLE
        self.drive_on = False
        self.homing = self.homed = False
        self.governor.halt_at(self.raw_steps)
        self.apply_soft_limits()
        self.raise_fault(FAULT_KILLED, 109)

    def action(self, bits):
        if bits & CONTROL_CLEAR_FAULT:
            self.fault = 0
            self.last_error = 0
        if bits & CONTROL_STOP:
            self.cancel_scheduled()
            self.homing = False
            self.apply_soft_limits()
            self.governor.stop(min(max(self.stored[0x0026], 1), self.governor.max_decel))
        if bits & CONTROL_ESTOP:
            self.cancel_scheduled()
            self.homing = False
            self.apply_soft_limits()
            self.governor.stop(self.governor.quick_stop_decel)
            self.raise_fault(FAULT_MOTION_ABORTED, 108)
        if bits & (CONTROL_SAVE_CONFIG | CONTROL_RELOAD_CONFIG) and (self.moving or self.homing):
            self.raise_fault(FAULT_INVALID_COMMAND, 110)
            return
        if bits & CONTROL_RELOAD_CONFIG:
            self.load_config()
        if bits & CONTROL_SAVE_CONFIG:
            self.flash = {address: self.stored[address] for address in LIMIT_REGISTERS + [0x0045, 0x0046, 0x002A]}
            self.limits_status |= LIMITS_COMMISSIONED
        if bits & CONTROL_SET_ZERO:
            self.offset, self.homed = -self.raw_steps, True
            self.apply_soft_limits()
        motion = bits & (CONTROL_MOVE_ABSOLUTE | CONTROL_MOVE_RELATIVE | CONTROL_HOME)
        if not motion:
            return
        if not self.enabled:
            self.raise_fault(FAULT_MOTION_WHILE_DISABLED, 101)
            return
        if motion & CONTROL_MOVE_ABSOLUTE and not self.homed:
            self.raise_fault(FAULT_MOVE_BEFORE_HOMED, 102)
            return

        self.cancel_scheduled()
        target, relative = i32(self.stored[0x0020]), i32(self.stored[0x002B])
        execute_at = self.stored[0x0018]
        if execute_at == 0:
            self.start_motion(motion, target, relative)
            return

        lead_us = i32(execute_at - self.local_tick())
        if lead_us <= 0:
            self.raise_fault(FAULT_INVALID_COMMAND, 106)
        elif lead_us > SCHEDULE_MAX_LEAD_US:
            self.raise_fault(FAULT_INVALID_COMMAND, 107)
        else:
            handle = asyncio.get_running_loop().call_later(lead_us / 1e6, self.fire_scheduled)
            self.scheduled = (handle, motion, target, relative)

    def fire_scheduled(self):
        _, motion, target, relative = self.scheduled
        self.scheduled = None
        if self.enabled:
            self.start_motion(motion, target, relative)

    def cancel_scheduled(self):
        if self.scheduled:
            self.scheduled[0].cancel()
            self.scheduled = None

    def start_motion(self, motion, target, relative):
        self.last_start_tick = self.local_tick()
        velocity, accel = self.stored[0x0023], self.stored[0x0026]
        if motion & CONTROL_HOME:
            self.homing, self.homed = True, False
            self.apply_soft_limits()
            self.governor.run(-float(self.stored[0x0028] or 1), accel, accel)
            return
        self.homing = False
        self.apply_soft_limits()
        raw_target = target - self.offset if motion & CONTROL_MOVE_ABSOLUTE else self.raw_steps + relative
        self.governor.move_to(raw_target, velocity, accel, accel)

    def link_lost(self):
        """The link watchdog: quick stop, drive left on to hold position."""
        self.cancel_scheduled()
        self.homing = False
        self.apply_soft_limits()
        self.governor.stop(self.governor.quick_stop_decel)
        self.raise_fault(FAULT_MOTION_ABORTED, 108)

    def tick(self, dt):
        # The governor runs in 1 ms updates, as on the board.
        self.leftover += dt
        while self.leftover >= 0.001:
            self.leftover -= 0.001
            self.update()

    def update(self):
        if self.homing and self.raw_steps <= HOME_SWITCH_STEPS:
            self.offset = i32(self.stored[0x002A]) - self.raw_steps
            self.homed, self.homing = True, False
            self.apply_soft_limits()
            self.governor.stop(self.governor.quick_stop_decel)
        self.governor.update()
        if self.drive_on:
            wanted = round(self.governor.position)
            self.steps += abs(wanted - self.raw_steps)
            self.raw_steps = wanted
        if self.drive_on and not self.enabled and not self.moving:
            self.drive_on = False


class Console:
    def __init__(self, axis, args):
        self.axis = axis
        self.args = args
        self.crash = None

    def register_line(self, address):
        name, _ = BY_ADDRESS[address]
        value = self.axis.read(address)
        return f"{name} (0x{address:04x}) = 0x{value:08x} u={value} i={i32(value)}"

    def lookup(self, token):
        if token.upper() in BY_NAME:
            return BY_NAME[token.upper()][0]
        try:
            address = int(token, 0)
        except ValueError:
            return None
        return address if address in BY_ADDRESS else None

    def handle(self, line):
        words = line.split()
        if not words:
            return []
        command, rest = words[0], words[1:]
        axis = self.axis

        if command == "ping":
            return ["pong"]
        if command == "diag":
            if rest and rest[0] in ("panic", "hang"):
                self.crash = "panic phase=net caller=0x10012345 msg=\"simulated\"" if rest[0] == "panic" else "watchdog phase=net"
                return ["crashing"]
            uptime = int((time.monotonic() - axis.started) * 1000)
            return [f"diag reset={axis.reset_reason} uptime_ms={uptime}"]
        if command == "help":
            return ["commands: help status ping net reg read|readm|write|bitset|bitclear|dump",
                    "aliases: enable on|off outputs on|off zero home goto <steps> move <steps> vel <sps> "
                    "accel <sps2> stop qstop kill"]
        if command == "status":
            return [f"status pos={round(axis.position)} target={axis.target} vel={abs(round(axis.velocity))} "
                    f"homed={'yes' if axis.homed else 'no'} moving={'yes' if axis.moving else 'no'} "
                    f"faults=0x{axis.fault:08x} steps={axis.steps}"]
        if command == "net":
            return [f"net state=connected link=3 host={self.args.hostname}.local port={self.args.port} "
                    f"ssid=simulated ip=127.0.0.1 client=yes rssi=-42 watchdog_ms={int(WATCHDOG_S * 1000)} trips=0"]
        if command in ("stop", "qstop", "kill"):
            bit = {"stop": CONTROL_STOP, "qstop": CONTROL_ESTOP, "kill": CONTROL_KILL}[command]
            axis.write(0x0010, axis.levels | bit)
            return ["ok"]
        if command == "zero":
            axis.action(CONTROL_SET_ZERO)
            return ["ok"]
        if command == "home":
            axis.action(CONTROL_HOME)
            return ["ok"]
        if command in ("enable", "outputs") and rest:
            mask = CONTROL_MOTOR_ENABLE if command == "enable" else CONTROL_OUTPUT_ENABLE
            axis.write(0x0010, axis.levels | mask if rest[0] == "on" else axis.levels & ~mask)
            return ["ok"]
        if command in ("vel", "accel") and rest:
            axis.write(0x0023 if command == "vel" else 0x0026, int(rest[0], 0))
            return ["ok"]
        if command in ("goto", "move") and rest:
            staged = 0x0020 if command == "goto" else 0x002B
            axis.write(staged, int(rest[0], 0))
            axis.action(CONTROL_MOVE_ABSOLUTE if command == "goto" else CONTROL_MOVE_RELATIVE)
            return ["ok"]
        if command == "reg" and rest:
            return self.handle_reg(rest[0], rest[1:])
        return [f"unknown command: {command}"]

    def handle_reg(self, sub, args):
        axis = self.axis
        if sub == "dump":
            return [self.register_line(address) for address, _, _ in REGISTERS]
        if sub == "readm":
            lines = []
            for token in args:
                address = self.lookup(token)
                lines.append(self.register_line(address) if address is not None else f"not-found {token}")
            return lines
        address = self.lookup(args[0]) if args else None
        if address is None:
            return [f"usage: reg {sub} <addr|name>"]
        if sub == "read":
            return [self.register_line(address)]
        if sub in ("write", "bitset", "bitclear") and len(args) > 1:
            if not BY_ADDRESS[address][1]:
                return ["read-only"]
            value = u32(int(args[1], 0))
            current = axis.read(address)
            if sub == "bitset":
                value = current | value
            elif sub == "bitclear":
                value = current & ~value
            result = axis.write(address, value)
            return [result, self.register_line(address)] if result == "ok" else [result]
        return [f"unknown reg command: {sub}"]


async def serve(args):
    axis = Axis()
    console = Console(axis, args)
    active = {"writer": None, "last_rx": None}

    async def simulate():
        last = time.monotonic()
        while True:
            await asyncio.sleep(0.01)
            now = time.monotonic()
            axis.tick(now - last)
            last = now
            if active["last_rx"] and now - active["last_rx"] > WATCHDOG_S and (axis.moving or axis.homing):
                print("watchdog: client silent, motion stopped")
                axis.link_lost()
                active["last_rx"] = None

    async def client(reader, writer):
        if time.monotonic() < active.get("down_until", 0):
            writer.close()
            return
        if active["writer"] is not None:
            active["writer"].close()
        active["writer"], active["last_rx"] = writer, None
        peer = writer.get_extra_info("peername")
        print(f"client connected from {peer}")
        writer.write(f"axiscontroller ready\r\n{PROMPT}".encode())
        try:
            while line := await reader.readline():
                active["last_rx"] = time.monotonic()
                text = line.decode(errors="replace").strip("\r\n")
                response = console.handle(text)
                writer.write(("".join(f"{r}\r\n" for r in response) + PROMPT).encode())
                await writer.drain()
                if console.crash:
                    # Simulated reset: drop the link, stay away for a while,
                    # then come back as a freshly booted board.
                    print(f"simulating reset: {console.crash}")
                    axis.cancel_scheduled()
                    axis.reset_state(console.crash)
                    console.crash = None
                    active["down_until"] = time.monotonic() + 3
                    break
        except ConnectionError:
            pass
        finally:
            if active["writer"] is writer:
                if active["last_rx"] and (axis.moving or axis.homing):
                    print("watchdog: client disconnected, motion stopped")
                    axis.link_lost()
                active["writer"], active["last_rx"] = None, None
            writer.close()
            print(f"client {peer} gone")

    server = await asyncio.start_server(client, args.bind, args.port)
    print(f"fake axis {args.hostname} on {args.bind}:{args.port}")
    asyncio.create_task(simulate())
    async with server:
        await server.serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--name", default="sim", help="axis name; the host becomes roberta-<name>")
    parser.add_argument("--port", type=int, default=7300)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--no-bonjour", action="store_true", help="skip the dns-sd advertisement")
    args = parser.parse_args()
    args.hostname = f"roberta-{args.name}"

    advert = None
    if not args.no_bonjour and shutil.which("dns-sd"):
        # Proxy registration: advertises the service and a host record, so
        # "roberta-<name>.local" resolves to this machine like a real board.
        advert = subprocess.Popen(
            ["dns-sd", "-P", args.hostname, "_roberta-axis._tcp", "local", str(args.port),
             f"{args.hostname}.local", "127.0.0.1", f"name={args.name}", "proto=console1"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        asyncio.run(serve(args))
    except KeyboardInterrupt:
        pass
    finally:
        if advert:
            advert.terminate()


if __name__ == "__main__":
    main()
