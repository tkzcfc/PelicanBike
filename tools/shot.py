#!/usr/bin/env python3
# usage: shot.py out.png [ENV=VAL ...]  — runs PelicanBike on Xvfb :12, grabs the X screen after READY
import os, subprocess, sys, time
from PIL import ImageGrab
out = sys.argv[1]
env = dict(os.environ, DISPLAY=os.environ.get('SHOT_DISPLAY', ':12'), PB2_SHOT='1', PB2_SHOT_FRAME=os.environ.get('PB2_SHOT_FRAME', '60'))
for kv in sys.argv[2:]:
    k, v = kv.split('=', 1); env[k] = v
d = os.environ.get('PB_BIN', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'bin', 'PelicanBike'))
p = subprocess.Popen(['./PelicanBike'], cwd=d, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
log = []
for line in p.stdout:
    log.append(line)
    if 'READY' in line:
        time.sleep(1.0)
        ImageGrab.grab(xdisplay=os.environ.get('SHOT_DISPLAY', ':12')).crop((0, 0, int(os.environ.get("SHOT_W","1280")), int(os.environ.get("SHOT_H","720")))).save(out)
        print('saved', out)
        break
p.wait(timeout=120)
print(''.join(l for l in log if 'pb2' in l or 'rror' in l)[-1500:])
