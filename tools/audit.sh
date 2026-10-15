#!/bin/bash
# time-aligned side-by-side audit (sim held at S.t=4 s in both): tools/audit.sh js|our [hours...]
# -> /workspace/pb2shots/audit/{js,our}_<h>_<cam>.png ; JS runs 3 browsers in parallel
cd "$(dirname "$0")/.."; O=/workspace/pb2shots/audit; mode=$1; shift; H=${@:-8 12 17.35 18.5 21.5}
for h in $H; do for c in orbit chase side cine pov; do
  if [ "$mode" = js ]; then
    while [ $(jobs -r | wc -l) -ge 3 ]; do sleep 2; done
    (timeout 400 node tools/js_shot_t.mjs $O/js_${h}_$c.png $h $c 4 >/dev/null 2>&1; echo "js $h $c") &
  else python3 tools/shot.py $O/our_${h}_$c.png PB2_HOLD=4 PB2_CAM=$c PB2_HOUR=$h PB2_FREEZE=1 PB2_SPEED=0 PB2_DT=0.033 >/dev/null 2>&1; echo "our $h $c"; fi
done; done; wait
