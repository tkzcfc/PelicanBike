# per-region JS split (sun / ambient) vs ours (dbg 2 direct, 3 ambient diffuse, 4 env spec)
import numpy as np, sys
from PIL import Image
def lo(p):
    d=np.fromfile(p,dtype=np.float32); w,h=d[:2].view(np.int32); a=d[2:].reshape(h,w,4)[::-1,:,:3]
    return np.stack([np.asarray(Image.fromarray(a[:,:,c].astype(np.float32)).resize((640,360),Image.BILINEAR)) for c in range(3)],-1)
def lj(p):
    d=np.fromfile(p,dtype=np.float32); w,h=d[:2].view(np.int32); return d[2:].reshape(h,w,3)[::-1]
for h in sys.argv[1:]:
  J=[lj(f'/tmp/jsS{h}{x}.bin') for x in ('','.sun','.amb')]; D=[lo(f'/tmp/our{h}_d{k}.bin') for k in (2,3,4)]
  for n,(y0,y1,x0,x1) in {'grass':(150,200,520,640),'rider':(150,260,250,330),'sand':(200,260,0,100),'road':(250,300,300,400)}.items():
    s=lambda a:a[y0:y1,x0:x1].reshape(-1,3).mean(0)
    oa=s(D[1])+s(D[2]); print(h,n,'sun js/ours',(s(J[1])/s(D[0])).round(2),'amb js',s(J[2]).round(3),'ours',oa.round(3),'ratio',(s(J[2])/oa).round(2))
