"""Internal dimension drawings made from the same rendered images."""
import base64
import html
import json
import math
from pathlib import Path
ROOT = Path(__file__).resolve().parent
data = json.loads((ROOT / 'design-source.json').read_text(encoding='utf-8'))

def project(shot, point):
    def sub(a,b): return tuple(x-y for x,y in zip(a,b))
    def dot(a,b): return sum(x*y for x,y in zip(a,b))
    def norm(a):
        length=math.sqrt(dot(a,a))
        return tuple(x/length for x in a)
    def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
    forward=norm(sub(shot['look_at'],shot['camera']))
    right=norm(cross(forward,(0,0,1))) if abs(forward[2])<0.999 else (1,0,0)
    up=norm(cross(right,forward))
    delta=sub(point,shot['look_at'])
    return (800+dot(delta,right)*1400/shot['ortho'],800-dot(delta,up)*1400/shot['ortho'])


def svg_text(x,y,body,size=25,anchor='middle',extra=''):
    return f'<text x="{x:.2f}" y="{y:.2f}" text-anchor="{anchor}" font-size="{size}" {extra}>{html.escape(body)}</text>'


def line(a,b,color='teal',extra=''):
    return f'<line x1="{a[0]:.2f}" y1="{a[1]:.2f}" x2="{b[0]:.2f}" y2="{b[1]:.2f}" stroke="{data["palette"][color]["hex"]}" stroke-width="3" {extra}/>'


def dimension(a,b,at,label,vertical=False):
    if vertical:
        aa=(at,a[1]);bb=(at,b[1]);tx=at-24;ty=(a[1]+b[1])/2
        caption=svg_text(tx,ty,label,27,extra=f'transform="rotate(-90 {tx} {ty})"')
    else:
        aa=(a[0],at);bb=(b[0],at)
        caption=svg_text((a[0]+b[0])/2,at+38,label,27)
    return line(a,aa,extra='opacity=".55"')+line(b,bb,extra='opacity=".55"')+line(aa,bb,extra='marker-start="url(#arrow)" marker-end="url(#arrow)"')+caption


def annotated_svg(shot_id):
    shot=next(s for s in data['shots'] if s['id']==shot_id)
    png=base64.b64encode((ROOT/'renders'/f'{shot_id}.png').read_bytes()).decode()
    teal=data['palette']['teal']['hex']; orange=data['palette']['orange']['hex']
    w=data['cabinet']['width'];h=data['cabinet']['height'];d=data['cabinet']['body_depth'];front=d/2-data['cabinet']['overall_depth']
    parts=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1600 1600"><title>CLAW LAB / {html.escape(shot["title"])} / kích thước từ manifest</title><defs><marker id="arrow" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M 9 1 L 1 5 L 9 9" fill="none" stroke="{teal}" stroke-width="1.5"/></marker></defs><rect width="1600" height="1600" fill="#f8f6ef"/><image href="data:image/png;base64,{png}" x="100" y="100" width="1400" height="1400"/><g fill="{teal}" font-family="Segoe UI,sans-serif">']
    parts.append(svg_text(80,55,shot['title']+' / '+data['revision'],30,'start'))
    if shot_id=='02-front':
        parts.append(dimension(project(shot,(-w/2,0,0)),project(shot,(w/2,0,0)),1450,f'{w*1000:.0f} mm'))
        parts.append(dimension(project(shot,(-w/2,0,0)),project(shot,(-w/2,0,h)),370,f'{h*1000:.0f} mm',True))
        floor=project(shot,(w/2,0,data['cabinet']['floor_z']))
        parts.append(line(floor,(1420,floor[1])))
        parts.append(svg_text(1420,floor[1]-15,'Sàn 700 mm',25,'end'))
        rail=project(shot,(w/2,0,data['cabinet']['gantry_z']))
        parts.append(line(rail,(1420,rail[1])))
        parts.append(svg_text(1420,rail[1]-15,'Ray 1620 mm',25,'end'))
    elif shot_id=='03-side':
        parts.append(dimension(project(shot,(0,front,0)),project(shot,(0,d/2,0)),1450,f'{data["cabinet"]["overall_depth"]*1000:.0f} mm / có console'))
        parts.append(dimension(project(shot,(0,-d/2,h)),project(shot,(0,d/2,h)),145,f'{d*1000:.0f} mm / thân'))
        parts.append(dimension(project(shot,(0,front,0)),project(shot,(0,front,h)),300,f'{h*1000:.0f} mm',True))
        parts.append(svg_text(450,1400,'TRƯỚC',25))
        parts.append(svg_text(1050,1400,'SAU',25))
    else:
        limits_x=data['cabinet']['rail_limit_x']; limits_y=data['cabinet']['rail_limit_y']
        a=project(shot,(limits_x[0],limits_y[1],0));b=project(shot,(limits_x[1],limits_y[0],0))
        parts.append(f'<rect x="{a[0]}" y="{a[1]}" width="{b[0]-a[0]}" height="{b[1]-a[1]}" fill="none" stroke="{teal}" stroke-width="4" stroke-dasharray="14 10"/>')
        for i,prize in enumerate(data['prizes']):
            x,y=project(shot,(*prize['xy'],0))
            parts.append(svg_text(x,y+9,str(i+1).zfill(2),30,extra='font-weight="700"'))
        drop=project(shot,(*data['chute']['drop_xy'],0));target=project(shot,(*data['states']['aim']['xy'],0))
        parts.append(line(target,drop,'orange',extra='stroke-dasharray="10 8"'))
        parts.append(f'<circle cx="{drop[0]}" cy="{drop[1]}" r="12" fill="{orange}"/>')
        parts.append(svg_text(800,120,'Nét đứt: giới hạn tâm càng / số: InstanceId',27))
        parts.append(dimension(project(shot,(-w/2,0,0)),project(shot,(w/2,0,0)),1505,f'{w*1000:.0f} mm'))
    parts.append(svg_text(800,1565,'Đơn vị mm · hình trực giao · đổi camera không đổi asset',23))
    parts.append('</g></svg>')
    return ''.join(parts)


for shot_id in ('02-front','03-side','05-top'):
    (ROOT/f'{shot_id}-dimensions.svg').write_text(annotated_svg(shot_id),encoding='utf-8')

