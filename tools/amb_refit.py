# fit up/side ambient tints: JS amb ≈ (full-direct)_ours(tint=1) * mix(Ts,Tu,N.y)
import numpy as np
from PIL import Image
def lo(p):
    d=np.fromfile(p,dtype=np.float32); w,h=d[:2].view(np.int32); a=d[2:].reshape(h,w,4)[::-1,:,:3]
    return np.stack([np.asarray(Image.fromarray(a[:,:,c]).resize((640,360),Image.BILINEAR)) for c in range(3)],-1)
def lj(p):
    d=np.fromfile(p,dtype=np.float32); w,h=d[:2].view(np.int32); return d[2:].reshape(h,w,3)[::-1]
X=[];Y=[];U=[]
import sys
for sc in sys.argv[1:] or ['8c','17']:
  F=lo(f'/tmp/rf_{sc}_d0.bin'); D=lo(f'/tmp/rf_{sc}_d2.bin'); N=lo(f'/tmp/rf_{sc}_d7.bin')[...,0]
  J=lj(f'/tmp/jsH{sc}.amb.bin'); A=F-D
  m=(np.abs(F-D).sum(-1)>0.02)&(D.sum(-1)<F.sum(-1)*0.97)&(A.sum(-1)>0.05)
  # exclude sky: sky pixels have F==D (no direct split) -> handled; also require HUD-free rows
  m[:40]=False; m[-60:]=False
  X.append(A[m]);Y.append(J[m]);U.append(N[m])
X=np.concatenate(X);Y=np.concatenate(Y);U=np.concatenate(U)
for c in range(3):
  M=np.stack([X[:,c]*U, X[:,c]*(1-U)],1)
  t,_,_,_=np.linalg.lstsq(M,Y[:,c],rcond=None); print('ch',c,'Tu %.3f Ts %.3f'%tuple(t))
print('n',len(U),'up frac',(U>0.7).mean().round(2))
