#!/bin/bash
# serial time-aligned audit (deterministic): tools/audit2.sh "hours" "cams"
cd "$(dirname "$0")/.."; O=/workspace/pb2shots/audit
for h in $1; do for c in $2; do
  timeout 400 node tools/js_shot_t.mjs $O/js_${h}_$c.png $h $c 4 >/dev/null 2>&1
  python3 tools/shot.py $O/our_${h}_$c.png PB2_HOLD=4 PB2_CAM=$c PB2_HOUR=$h PB2_FREEZE=1 PB2_SPEED=0 PB2_DT=0.033 >/dev/null 2>&1
  echo "$h $c $(python3 tools/px_cmp.py $O/js_${h}_$c.png $O/our_${h}_$c.png | tail -1)"
done; done
