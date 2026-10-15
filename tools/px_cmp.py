# region mean RGB diff between two screenshots: px_cmp.py a.png b.png [x0,y0,x1,y1 ...]
import sys, numpy as np
from PIL import Image
a=np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(int); b=np.asarray(Image.open(sys.argv[2]).convert('RGB')).astype(int)
for r in sys.argv[3:]:
    x0,y0,x1,y1=map(int,r.split(',')); print(r,'js',a[y0:y1,x0:x1].reshape(-1,3).mean(0).round(1),'ours',b[y0:y1,x0:x1].reshape(-1,3).mean(0).round(1))
d=np.abs(a-b).mean(); print('mean abs diff',round(d,2))
