"""Render actual shader options at an identical camera, compare resulting pixels."""
import json, subprocess
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
ROOT=Path(__file__).resolve().parents[1]
exe=ROOT/'dist/claw-lab/claw_lab.exe'
out=ROOT/'output/runtime/modes'
for mode,args in [('lit',[]),('no-glass',['--no-glass']),('no-shadow',['--no-shadow']),('unlit',['--unlit']),('no-texture',['--no-texture'])]:
    r=subprocess.run([str(exe),'--hidden','--shot','01-hero','--size','700x700','--output',str(out/mode),*args],cwd=ROOT,capture_output=True,text=True,timeout=60)
    assert r.returncode==0,(mode,r.stderr)
base=Image.open(out/'lit/01-hero.png').convert('RGB')
result={}
for mode in ['no-glass','no-shadow','unlit','no-texture']:
    diff=ImageChops.difference(base,Image.open(out/mode/'01-hero.png').convert('RGB'))
    mean=sum(ImageStat.Stat(diff).mean)/3
    assert diff.getbbox() and mean>.001,(mode,mean)
    result[mode]={'mean_absolute_pixel_difference_0_to_255':mean,'changed_bbox':diff.getbbox()}
(out/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
detail=out/'claw-detail'
r=subprocess.run([str(exe),'--hidden','--shot','07-claw-open','--size','700x700','--output',str(detail)],cwd=ROOT,capture_output=True,text=True,timeout=60)
assert r.returncode==0,r.stderr
pink=sum(1 for r,g,b in Image.open(detail/'07-claw-open.png').convert('RGB').get_flattened_data() if r-g>15 and 0<g-b<35 and b>140)
assert pink>1000,('claw detail must retain the same target prize visible in the Blender mockup',pink)
print('RENDER MODES PASS:',result)
