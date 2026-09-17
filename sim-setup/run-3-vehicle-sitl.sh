#!/usr/bin/env bash
# =====================================================================
# Launch N (default 3) PX4-Autopilot-NX SITL instances, gz_x500, headless.
# Each instance i gets MAV_SYS_ID = i+1 automatically (PX4 posix rcS), so
# the three present to STRATUM as THREE distinct vehicles — the D9 identity
# uniqueness the multi-vehicle design depends on, for free.
# Run INSIDE WSL after build-px4-nx-sitl.sh.
# =====================================================================
set -euo pipefail

PX4_DIR="${PX4_DIR:-$HOME/PX4-Autopilot-NX}"
N="${N:-3}"
MODEL="${MODEL:-gz_x500}"
AUTOSTART="${AUTOSTART:-4001}"          # 4001 = gz_x500 airframe
SPACING="${SPACING:-5}"                 # metres between spawn points
BIN="$PX4_DIR/build/px4_sitl_default/bin/px4"

[ -x "$BIN" ] || { echo "No SITL binary at $BIN — run build-px4-nx-sitl.sh first."; exit 1; }
cd "$PX4_DIR"

pids=()
cleanup() {
    echo; echo "==> stopping all instances + gz"
    kill "${pids[@]}" 2>/dev/null || true
    pkill -f 'px4_sitl_default/bin/px4' 2>/dev/null || true
    pkill -x gz 2>/dev/null || true
}
trap cleanup INT TERM EXIT

for i in $(seq 0 $((N-1))); do
    pose="0,$((i*SPACING))"             # spawn along the Y axis so they don't overlap
    gcs=$((18570+i))
    echo "==> instance $i   MAV_SYS_ID $((i+1))   GCS udp $gcs   pose ($pose)"
    if [ "$i" -eq 0 ]; then
        # instance 0 starts the gz server and spawns the first model
        HEADLESS=1 PX4_SYS_AUTOSTART=$AUTOSTART PX4_GZ_MODEL_POSE="$pose" PX4_SIM_MODEL=$MODEL \
            "$BIN" -i 0 -d > "/tmp/px4_sitl_0.log" 2>&1 &
        pids+=($!)
        echo "    (gz server starting; waiting 12s before adding vehicles)"
        sleep 12
    else
        # instances >0 detect the running gz server and add their model
        PX4_SYS_AUTOSTART=$AUTOSTART PX4_GZ_MODEL_POSE="$pose" PX4_SIM_MODEL=$MODEL \
            "$BIN" -i "$i" -d > "/tmp/px4_sitl_$i.log" 2>&1 &
        pids+=($!)
        sleep 4
    fi
done

echo
echo "==================================================================="
echo " $N instances up.  MAV_SYS_ID 1..$N.  Logs: /tmp/px4_sitl_*.log"
echo " STRATUM-QGC (Windows) should list $N vehicles via mirrored networking."
echo " If not all appear, add UDP Comm Links in STRATUM to ports:"
for i in $(seq 0 $((N-1))); do echo "     instance $i  ->  UDP $((18570+i))"; done
echo " Ctrl-C here stops everything."
echo "==================================================================="
wait
