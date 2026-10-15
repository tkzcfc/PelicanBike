#!/bin/bash
# drive the desktop build on Xvfb :12 with xdotool; logs to /tmp/mouse.log, shots to /workspace/pb2shots/m_*.png
cd /workspace/PelicanBike_rename/PelicanBike/build/bin/PelicanBike
DISPLAY=:12 PB2_DT=0.033 PB2_HOUR=17.35 ./PelicanBike > /tmp/mouse.log 2>&1 &
PID=$!
for i in $(seq 60); do grep -q "FPS" /tmp/mouse.log && break; sleep 1; done; sleep 2
export DISPLAY=:12
g() { python3 -c "from PIL import ImageGrab; ImageGrab.grab(xdisplay=':12').crop((0,0,1280,720)).save('/workspace/pb2shots/m_$1.png')"; }
xdotool mousemove 646 37 click 1; sleep 2; g chip_side
xdotool mousemove 640 300 mousedown 1; for i in $(seq 10); do xdotool mousemove_relative -- 25 6; sleep 0.15; done; xdotool mouseup 1; sleep 2; g drag
for i in 1 2 3 4 5; do xdotool click 4; sleep 0.1; done; sleep 2; g zoomin
xdotool mousemove 1106 37 click 1; sleep 2; xdotool mousemove 1000 216 click 1; sleep 2; g settings
xdotool mousemove 1106 37 click 1; sleep 1
xdotool mousemove 260 300 click 1; sleep 1; xdotool mousemove 30 37 click 1; sleep 0.5; xdotool mousemove 582 37 click 1; sleep 3; xdotool mousemove 640 400 click 1; sleep 1; xdotool mousemove 610 330 click 1; sleep 1; xdotool mousemove 900 330 click 1; sleep 1; g after
kill $PID
grep -E "zoom|clickAt|hit|sea|cam ->" /tmp/mouse.log
