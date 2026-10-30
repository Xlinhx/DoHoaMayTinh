"""Create exportable visual comparisons; source and runtime keep the same framing."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'output/runtime'
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',30)
small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',20)
def build(shots,name,size=1400):
    image=Image.new('RGB',(size*2,len(shots)*(size+66)), '#f8f6ef')
    draw=ImageDraw.Draw(image)
    for row,shot in enumerate(shots):
        y=row*(size+66)
        for col,(directory,label) in enumerate([('mockups/renders','Blender r1 · tham chiếu'),('output/runtime/reference','OpenGL 3.3 · bản chạy')]):
            path=ROOT/directory/(shot+'.png');pic=Image.open(path);assert pic.size==(1400,1400),(path,pic.size)
            image.paste(pic.resize((size,size),Image.Resampling.LANCZOS),(col*size,y+66))
            draw.text((col*size+22,y+10),label,fill='#173b3a',font=font if size==1400 else small)
            draw.text((col*size+22,y+42),shot,fill='#657875',font=small)
    image.save(out/name)
build(['01-hero'],'comparison-hero.png')
build(['02-front','03-side','05-top'],'comparison-views.png',700)
build(['07-claw-open','08-claw-grip','09-prize','16-result'],'comparison-details.png',700)
print('COMPARISON: hero, front/side/top, open/grip/prize/result; fixed manifest cameras, 1400x1400 input')
