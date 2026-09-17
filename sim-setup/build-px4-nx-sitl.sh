#!/usr/bin/env bash
# =====================================================================
# Build PX4-Autopilot-NX SITL inside WSL2 (Ubuntu 22.04/24.04).
# Run INSIDE the WSL shell.  This is the MVO harness build — it uses the
# NX FORK (custom Standoff mode: nav_state 9 / MAV_CMD_DO_STANDOFF 31010),
# NOT stock PX4, which has no Standoff mode and cannot test MVO.
# Idempotent: safe to re-run.
# =====================================================================
set -euo pipefail

# PX4-NX must live on the WSL-NATIVE filesystem (~), never on /mnt/c (OneDrive):
# building PX4 on the Windows mount is 10-50x slower and hits permission/case bugs.
PX4_DIR="${PX4_DIR:-$HOME/PX4-Autopilot-NX}"
# Preferred source: your Nexam fork, cloned WITH submodules. The Windows snapshot
# ("...-main") has no submodules and will not build; clone from git instead.
PX4_NX_REPO="${PX4_NX_REPO:-}"     # e.g. https://github.com/Nexam-Systems/PX4-Autopilot-NX.git

if [ -d "$PX4_DIR/.git" ]; then
    echo "==> [1/3] Updating $PX4_DIR (+ submodules)"
    git -C "$PX4_DIR" submodule update --init --recursive
elif [ -n "$PX4_NX_REPO" ]; then
    echo "==> [1/3] Cloning $PX4_NX_REPO -> $PX4_DIR (recursive)"
    git clone "$PX4_NX_REPO" --recursive "$PX4_DIR"
else
    echo "ERROR: PX4-NX source not found and PX4_NX_REPO not set."
    echo "  Give the harness your fork's git URL (it needs submodules):"
    echo "    PX4_NX_REPO=https://github.com/Nexam-Systems/PX4-Autopilot-NX.git \\"
    echo "        bash build-px4-nx-sitl.sh"
    echo "  (The connected Windows/OneDrive copy is a snapshot without submodules.)"
    exit 1
fi

echo "==> [2/3] PX4 toolchain installer (Gazebo gz + deps)"
bash "$PX4_DIR/Tools/setup/ubuntu.sh"

echo "==> [3/3] Compile SITL binary (no launch)"
cd "$PX4_DIR"
make px4_sitl_default

echo
echo "DONE. SITL binary: $PX4_DIR/build/px4_sitl_default/bin/px4"
echo "Next:  bash run-3-vehicle-sitl.sh"
