# compare our PB2_DUMPHDR dump vs js_hdr.mjs dump: region luminance stats + false-colour side-by-side
import sys, numpy as np
from PIL import Image
def load_ours(p):
    d = np.fromfile(p, dtype=np.float32); w, h = d[:2].view(np.int32); a = d[2:].reshape(h, w, 4)[::-1, :, :3]
    return np.asarray(Image.fromarray(np.clip(a, 0, 60).astype(np.float32)[:, :, 0]).resize((640, 360))), a
def load_js(p):
    d = np.fromfile(p, dtype=np.float32); w, h = d[:2].view(np.int32); a = d[2:].reshape(h, w, 3)[::-1]
    return a
o_full = load_ours(sys.argv[1])[1]; j = load_js(sys.argv[2])
o = np.stack([np.asarray(Image.fromarray(o_full[:, :, c].astype(np.float32)).resize((640, 360), Image.BILINEAR)) for c in range(3)], -1)
L = lambda a: a @ np.array([0.299, 0.587, 0.114])
R = {'sky_left': (40, 110, 0, 120), 'sky_mid': (20, 80, 200, 440), 'sea_left': (130, 190, 0, 120), 'sea_mid': (120, 160, 150, 280), 'sand_left': (200, 260, 0, 100),
     'road': (250, 300, 300, 400), 'grass': (150, 200, 520, 640), 'rider': (150, 260, 250, 330)}
if len(sys.argv) > 4: R = eval(sys.argv[4])
for n, (y0, y1, x0, x1) in R.items():
    a, b = o[y0:y1, x0:x1], j[y0:y1, x0:x1]
    print(f"{n:10s} ours L{L(a).mean():7.3f} p99 {np.percentile(L(a),99):7.2f} >1.05 {(L(a)>1.05).mean():.3f} rgb {a.mean((0,1)).round(3)} | js L{L(b).mean():7.3f} p99 {np.percentile(L(b),99):7.2f} >1.05 {(L(b)>1.05).mean():.3f} rgb {b.mean((0,1)).round(3)}")
tm = lambda a: (np.clip(a / (1 + a), 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)
c = np.concatenate([tm(o), tm(j)], 0); Image.fromarray(c).save(sys.argv[3])
