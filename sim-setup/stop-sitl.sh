#!/usr/bin/env bash
# Kill any running PX4 SITL instances and the gz server. Run in WSL.
pkill -f 'px4_sitl_default/bin/px4' 2>/dev/null || true
pkill -x gz 2>/dev/null || true
pkill -f 'gz sim' 2>/dev/null || true
echo "stopped SITL + gz (if any were running)"
