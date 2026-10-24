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
import random
import shutil
import subprocess
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
]
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

FAULT_MOTION_WHILE_DISABLED = 1 << 1
FAULT_MOVE_BEFORE_HOMED = 1 << 2

FAULT_INVALID_COMMAND = 1 << 0

STATUS_START_SCHEDULED = 1 << 14
SCHEDULE_MAX_LEAD_US = 5_000_000

WATCHDOG_S = 1.0
HOME_SWITCH_STEPS = -400  # where the simulated home switch sits


def u32(value):
    return value & 0xFFFFFFFF


def i32(value):
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


class Axis:
    def __init__(self):
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
        self.levels = CONTROL_OUTPUT_ENABLE
        self.position = 0.0
        self.velocity = 0.0
        self.target = 0
        self.moving = False
        self.homing = False
        self.homed = False
        self.steps = 0
        self.fault = 0
        self.last_error = 0

    @property
    def enabled(self):
        return self.levels & CONTROL_OUTPUT_ENABLE and self.levels & CONTROL_MOTOR_ENABLE

    def local_tick(self):
        return (int(time.monotonic() * 1e6) + self.tick_offset) & 0xFFFFFFFF

    def status_word(self):
        word = self.levels & 0x3
        word |= (1 << 2) if self.moving else 0
        word |= (1 << 3) if self.homing else 0
        word |= (1 << 4) if self.homed else 0
        word |= (1 << 5) if round(self.position) == self.target and not self.moving else 0
        word |= (1 << 6) if self.fault else 0
        word |= (1 << 7) if self.position <= HOME_SWITCH_STEPS else 0
        word |= STATUS_START_SCHEDULED if self.scheduled else 0
        return word

    def read(self, address):
        live = {
            0x0010: self.levels,
            0x0011: self.status_word(),
            0x0012: self.fault,
            0x0021: self.target,
            0x0022: round(self.position),
            0x0025: abs(round(self.velocity)),
            0x0050: self.steps,
            0x0051: int((time.monotonic() - self.started) * 1000),
            0x0052: self.local_tick(),
            0x0053: self.last_error,
            0x0057: self.last_start_tick,
        }
        return u32(live.get(address, self.stored[address]))

    def write(self, address, value):
        if address == 0x0010:
            self.levels = value & 0x3
            if not self.enabled:
                self.stop()
                self.cancel_scheduled()
            self.action(value & ~0x3)
        else:
            self.stored[address] = u32(value)

    def raise_fault(self, bit, code):
        self.fault |= bit
        self.last_error = code

    def action(self, bits):
        if bits & CONTROL_CLEAR_FAULT:
            self.fault = 0
            self.last_error = 0
        if bits & (CONTROL_STOP | CONTROL_ESTOP):
            self.stop()
            self.cancel_scheduled()
            if bits & CONTROL_ESTOP:
                self.levels &= ~CONTROL_MOTOR_ENABLE
        if bits & CONTROL_SET_ZERO:
            self.position, self.target, self.homed = 0.0, 0, True
            self.stop()
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
        if motion & CONTROL_MOVE_ABSOLUTE:
            self.target = target
            self.moving = True
        elif motion & CONTROL_MOVE_RELATIVE:
            self.target = round(self.position) + relative
            self.moving = True
        else:
            self.homing, self.homed, self.moving = True, False, False

    def stop(self):
        self.moving = self.homing = False
        self.velocity = 0.0
        self.target = round(self.position)

    def tick(self, dt):
        if not self.enabled:
            return
        max_v = max(self.stored[0x0023], 1)
        accel = max(self.stored[0x0026], 1)
        if self.homing:
            self.velocity = max(self.velocity - accel * dt, -float(self.stored[0x0028] or 1))
            if self.position <= HOME_SWITCH_STEPS:
                self.position = float(i32(self.stored[0x002A]))
                self.target = round(self.position)
                self.homed = True
                self.stop()
                return
        elif self.moving:
            remaining = self.target - self.position
            direction = 1 if remaining > 0 else -1
            stopping = self.velocity * self.velocity / (2 * accel)
            if abs(remaining) <= stopping:
                self.velocity -= direction * accel * dt
            else:
                self.velocity += direction * accel * dt
            self.velocity = max(-max_v, min(max_v, self.velocity))
        else:
            return
        before = round(self.position)
        self.position += self.velocity * dt
        if self.moving and (self.target - self.position) * (self.target - before) <= 0:
            self.position = float(self.target)
            self.stop()
        self.steps += abs(round(self.position) - before)


class Console:
    def __init__(self, axis, args):
        self.axis = axis
        self.args = args

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
        if command == "help":
            return ["commands: help status ping net reg read|readm|write|bitset|bitclear|dump",
                    "aliases: enable on|off outputs on|off zero home goto <steps> move <steps> stop"]
        if command == "status":
            return [f"status pos={round(axis.position)} target={axis.target} vel={abs(round(axis.velocity))} "
                    f"homed={'yes' if axis.homed else 'no'} moving={'yes' if axis.moving else 'no'} "
                    f"faults=0x{axis.fault:08x} steps={axis.steps}"]
        if command == "net":
            return [f"net state=connected link=3 host={self.args.hostname}.local port={self.args.port} "
                    f"ssid=simulated ip=127.0.0.1 client=yes rssi=-42 watchdog_ms={int(WATCHDOG_S * 1000)} trips=0"]
        if command == "stop":
            axis.action(CONTROL_STOP)
            return ["OK"]
        if command == "zero":
            axis.action(CONTROL_SET_ZERO)
            return ["OK"]
        if command == "home":
            axis.action(CONTROL_HOME)
            return ["OK"]
        if command in ("enable", "outputs") and rest:
            mask = CONTROL_MOTOR_ENABLE if command == "enable" else CONTROL_OUTPUT_ENABLE
            axis.write(0x0010, axis.levels | mask if rest[0] == "on" else axis.levels & ~mask)
            return ["OK"]
        if command in ("goto", "move") and rest:
            staged = 0x0020 if command == "goto" else 0x002B
            axis.write(staged, int(rest[0], 0))
            axis.action(CONTROL_MOVE_ABSOLUTE if command == "goto" else CONTROL_MOVE_RELATIVE)
            return ["OK"]
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
            axis.write(address, value)
            return ["OK", self.register_line(address)]
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
                axis.stop()
                active["last_rx"] = None

    async def client(reader, writer):
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
        except ConnectionError:
            pass
        finally:
            if active["writer"] is writer:
                if active["last_rx"] and (axis.moving or axis.homing):
                    print("watchdog: client disconnected, motion stopped")
                    axis.stop()
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
