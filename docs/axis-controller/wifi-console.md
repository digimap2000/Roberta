# WiFi Console Transport

This document defines how a higher-level controller reaches an axis controller
over WiFi. It is the current control path; the
[RS485 framed protocol](./rs485-framed-protocol.md) is parked.

## Why

Control is devolved: each axis controller (a Pico 2 W) sits at its motor and
joins the workshop WiFi. Nothing beyond power and the motor wiring has to run
back through the robot. A Mac (or later the pendant) connects to each axis
directly.

## Transport

- The axis joins a WPA2 network as a station and takes an address by DHCP.
- It listens on TCP port `7300`.
- The protocol is the USB console, unchanged: one command per line
  (`\r`, `\n` or `\r\n`), each response ends with the `axis> ` prompt.
- On connect the axis sends `axiscontroller ready` and a prompt.
- One controlling client at a time. A new connection replaces the old one, so
  an app can reconnect after a dropped link without waiting for a timeout.
- `reg readm <addr|name>...` reads several registers in one round trip, one
  response line per register, in request order. Hosts should use it for
  status polling; a WiFi round trip costs far more than parsing.

## Discovery

The axis advertises itself with mDNS/DNS-SD:

| Item | Value |
| --- | --- |
| Hostname | `roberta-<name>.local`, or `roberta-axis-<last 4 hex of board id>.local` before a name is set |
| Service | `_roberta-axis._tcp`, instance name equal to the hostname |
| TXT | `name=<name>`, `proto=console1` |

On a Mac, `dns-sd -B _roberta-axis._tcp` lists every axis on the network.

## Link Watchdog

The watchdog arms when a network client sends its first line, and re-arms on
every line after that. It trips when any of these happen while the axis is
moving or homing:

- no line received for the watchdog period (default `1000 ms`)
- the client disconnects or is replaced
- the WiFi link drops

Tripping issues a normal `STOP`. The drive stays enabled so the joint keeps
holding torque. A gravity-loaded joint would drop if it de-energised. The trip
is counted in `net status`.

Hosts keep the link alive by polling status. A client that only sends motion
commands must also send `ping` (reply `pong`) faster than the watchdog period.
`net watchdog <ms>` changes the period until reboot, and `0` disables it.

This is operational supervision for private testing, not a safety function. The
E-stop chain must stay in hardware.

## Network Commands

| Command | Effect |
| --- | --- |
| `net` / `net status` | State, hostname, SSID, IP, client, RSSI, watchdog period and trips |
| `net ssid <ssid>` | Stage the SSID (rest of line, spaces allowed) |
| `net pass <password>` | Stage the password (rest of line); omit for an open network |
| `net name <name>` | Stage the axis name (letters, digits, hyphens), e.g. `j1` |
| `net save` | Persist staged settings to flash and reconnect |
| `net forget` | Erase stored settings and drop off the network |
| `net watchdog <ms>` | Set the link watchdog period, `0` disables |
| `ping` | Reply `pong`; keeps the watchdog fed |

Settings live in the last 4 KB flash sector, so reflashing the firmware keeps
them. Use `net forget` to clear them.

## Timing Notes

- The CYW43 radio runs in performance power mode. The default power save adds
  100–300 ms of receive latency.
- Step generation still runs in the main loop. Network work happens in the
  CYW43 background interrupt and short critical sections, so expect a few tens
  of microseconds of step jitter. Move step generation to PIO or a timer before
  chasing high step rates.

## Synchronised Starts

Axes keep their own clocks. Nothing on the controller disciplines them.
Instead, the host measures each axis's offset just before a coordinated move
and expresses one shared start time in each axis's own clock.

1. **Estimate the offset.** The host reads `LOCAL_TICK_US` 8 times, noting its
   own steady clock before and after each read. It keeps the sample with the
   shortest round trip, assumes the tick was taken at that sample's midpoint,
   and records `offset = local_tick - host_us` (modulo 2^32). The error is at
   most half that round trip, about 3 ms worst case on WiFi and usually far
   less.
2. **Arm.** The host picks one start time a few hundred milliseconds ahead and
   converts it per axis. On each axis it writes `EXECUTE_AT_TICK_US`, the
   staged move, and the `CONTROL_WORD` move bit. The axis arms the move and
   sets status bit 14 while it waits.
3. **Fire.** The axis main loop starts the move when its tick reaches the
   target, within about 100 µs. It records the actual tick in
   `LAST_START_TICK_US`, which the host converts back to measure the spread.

A scheduled start is refused with error `106` if it is already in the past,
and with `107` if it is more than 5 s ahead, which catches stale or wrapped
estimates. A newer motion command, STOP, ESTOP, disabling the drive, or the
link watchdog all cancel a waiting start. A one-way UDP broadcast can't
measure link delay, so it would have been less accurate than this two-way
probe.

The host side is `roberta::axis::estimate_clock` in the core,
`roberta_axis_client_sync_clock` / `_move_by_steps_at` /
`_last_start_host_us` in the C API, and **Move Together** in Roberta Control.

Crystal drift between boards is tens of ppm, so an estimate made just before
a move stays good to microseconds over the lead time. Re-estimate before each
coordinated move rather than relying on an old one.

## Not Yet Done

- A binary or UDP fast path for streamed setpoints. Synchronised starts cover
  point-to-point moves, not continuous coordinated paths.
