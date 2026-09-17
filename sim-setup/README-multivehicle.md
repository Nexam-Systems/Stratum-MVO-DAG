# Multi-Vehicle SITL Harness — STRATUM MVO (S0)

Stand up **three simulated Daggers at once** so STRATUM's Multi-Vehicle
Orchestration can be built and tested with no airframe, no radio, no airspace.
This is the **S0** deliverable of the MVO roadmap (NXM-SW-PLAN-STRATUM-MVO-001):
prove three vehicles fly independently before any orchestration code trusts that
they do.

> This harness is separate from the single-vehicle `README.md` in this folder.
> That one clones **stock PX4** and is fine for single-view / AOP work. MVO is
> different: it needs the custom **Standoff** flight mode (`nav_state 9`,
> `MAV_CMD_DO_STANDOFF` = 31010), which exists **only in PX4-Autopilot-NX**.
> Stock PX4 will reject 31010 and knows no "Standoff" mode. So this harness
> builds from the **NX fork**. (Verified: `standoff.cpp` compiles into the
> NAVIGATOR module, and the SITL board enables NAVIGATOR + COMMANDER — the mode
> is in the sim build, not just the flight-controller build.)

## How it works, in one picture

```
   WSL2 (Ubuntu)                                   Windows
   ------------------------------------            -----------------------
   PX4-NX instance -i 0  MAV_SYS_ID 1  ─┐
   PX4-NX instance -i 1  MAV_SYS_ID 2  ─┼─ MAVLink/UDP ─►  STRATUM-QGC
   PX4-NX instance -i 2  MAV_SYS_ID 3  ─┘  (mirrored net)   MultiVehicleManager
        │  one gz server, three x500 models                 lists 3 vehicles
        └─ physics in Gazebo (gz), headless
```

Each PX4 instance is the **real flight firmware** compiled to run as a program;
Gazebo supplies the physics. PX4's own startup assigns `MAV_SYS_ID = instance+1`,
so instances `0/1/2` become vehicles `1/2/3` — the **D9** identity uniqueness the
whole design rests on, handled for free. Two airframes sharing the default
`MAV_SYS_ID=1` is exactly the failure that makes the second vehicle silently
vanish; the instance index prevents it.

## Prerequisites (one-time)

1. WSL2 + Ubuntu, and **mirrored networking** already applied (`.wslconfig` in
   this folder → `C:\Users\<you>\.wslconfig`, then `wsl --shutdown`). That is
   what lets three MAVLink streams reach STRATUM on Windows automatically.
2. **Build on the WSL-native filesystem** (`~`), never on `/mnt/c` (OneDrive).
   Building PX4 on the Windows mount is 10–50× slower and hits case/permission
   bugs. The scripts default to `~/PX4-Autopilot-NX`.

## Build (once, ~20–40 min first time)

Inside WSL, from this folder copied into `~`:

```bash
PX4_NX_REPO=https://github.com/Nexam-Systems/PX4-Autopilot-NX.git \
    bash build-px4-nx-sitl.sh
```

`PX4_NX_REPO` must be your fork's git URL because it needs **submodules** — the
connected Windows snapshot (`PX4-Autopilot-NX-main`) has none and will not build.
If your fork is not yet on GitHub, push it there (same clean-fork discipline as
STRATUM), or clone it recursively into `~/PX4-Autopilot-NX` yourself and re-run.

## Run three vehicles

```bash
bash run-3-vehicle-sitl.sh          # N=3 by default; N=5 bash run-3-vehicle-sitl.sh for five
```

Then launch STRATUM-QGC on Windows. With mirrored networking it auto-discovers
all three on UDP 14550. If any are missing, add UDP Comm Links to ports
**18570 / 18571 / 18572** (one per instance). `bash stop-sitl.sh` kills everything.

## S0 exit gate — what "done" looks like

S0 is demonstrated, not asserted, when:

1. STRATUM lists **three vehicles at once**, each with a distinct ID.
2. Each is independently commandable through the **existing single-vehicle**
   path (the Set Standoff panel, one vehicle at a time) — no orchestration code
   involved yet.
3. Each enters `nav_state 9` (Standoff) and settles to Hold on its commanded
   bearing, **confirmed in the log**, not by eye.

Passing this retires R6 (the custom modes were statically reviewed but never
compile-verified in a real run) with an actual three-instance run. Only then
does S1 — orchestration commanding all three from code — have solid ground.

## One thing to verify on first run

The gz **multi-vehicle attach** (instance 0 starts the gz server; instances 1–2
add their model to it via `PX4_GZ_MODEL_POSE`) follows PX4's documented pattern,
but I could not confirm the exact env flags against your tree because the
`Tools/simulation/gz` submodule is absent in the connected snapshot. If
instances 1–2 spawn their own empty world instead of joining instance 0's,
that flag is the one line to adjust in `run-3-vehicle-sitl.sh`. The proven
fallback is PX4's own `Tools/simulation/sitl_multiple_run.sh` (gazebo-classic).

## Troubleshooting

- **Only one vehicle appears** → almost always `MAV_SYS_ID` collision (not this
  harness — it sets them apart) or a missing Comm Link; add UDP 18570/71/72.
- **Build fails on missing headers** → submodules not initialised; re-run with
  `PX4_NX_REPO` set so the clone is `--recursive`.
- **Gazebo stutters / no GPU** → already headless (`HEADLESS=1`); STRATUM is the
  only display you need.
