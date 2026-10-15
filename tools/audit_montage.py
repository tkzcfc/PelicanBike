# combine js/our audit shots: one image per hour, rows = cams, left JS / right ours (640x360 each)
import sys
from PIL import Image, ImageDraw
O='/workspace/pb2shots/audit'; cams=['orbit','chase','side','cine','pov']
for h in sys.argv[1:] or ['8','12','17.35','18.5','21.5']:
  M=Image.new('RGB',(1280,360*len(cams)))
  for i,c in enumerate(cams):
    for j,k in enumerate(['js','our']):
      try: im=Image.open(f'{O}/{k}_{h}_{c}.png').convert('RGB').resize((640,360))
      except Exception: continue
      M.paste(im,(640*j,360*i)); ImageDraw.Draw(M).text((640*j+8,360*i+8),f'{k} {h} {c}',fill=(255,0,255))
  M.save(f'{O}/cmp_{h}.png')
