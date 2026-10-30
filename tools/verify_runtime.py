"""Exercise the installed executable, including Unicode/CWD and startup failures.
All temporary mutations happen to private copies inside workspace/build.
"""
import hashlib, json, os, shutil, struct, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
package=(Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'dist/claw-lab').resolve()
exe=package/'claw_lab.exe'
out=ROOT/'output/runtime/package'
out.mkdir(parents=True,exist_ok=True)
def run(args,cwd=ROOT,expected=0,contains=None):
    r=subprocess.run([str(exe),*map(str,args)],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
    assert r.returncode==expected,(args,r.returncode,r.stdout[-1000:],r.stderr)
    if contains:assert contains in r.stderr,(contains,r.stderr)
    return r
run(['--hidden','--verify','--size','1366x768','--output',out])
report=json.loads((out/'verification.json').read_text())
assert report['runtime_rounds_passed']==10
assert [r['score'] for r in report['rounds']]==[1,0]*5
for p in out.glob('live-*.png'):
    assert struct.unpack('>II',p.read_bytes()[16:24])==(1366,768),p
build=(ROOT/'build').resolve();build.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='kiểm-chứng-',dir=build) as temp:
    private=Path(temp).resolve();assert private.is_relative_to(build)
    copied=private/'Máy gắp thú';shutil.copytree(package,copied)
    old=exe;exe=copied/'claw_lab.exe'
    # Restrict PATH to OS directories; all non-system runtime code must be linked/packaged.
    saved_path=os.environ.get('PATH','');os.environ['PATH']=os.environ['SystemRoot']+'\\System32;'+os.environ['SystemRoot']
    try:
        run(['--hidden','--shot','02-front','--size','400x400','--output',private/'ảnh'],cwd=private)
    finally:os.environ['PATH']=saved_path
    original=(copied/'shaders/lit.frag').read_bytes()
    (copied/'shaders/lit.frag').write_text('#version 330 core\ninvalid shader syntax\n')
    run(['--hidden','--frames','1'],cwd=private,expected=1,contains='Shader compile')
    (copied/'shaders/lit.frag').write_text('#version 330 core\nin vec4 worldNormal; out vec4 c; void main(){c=worldNormal;}')
    run(['--hidden','--frames','1'],cwd=private,expected=1,contains='Shader link')
    (copied/'shaders/lit.frag').write_bytes(original)
    (copied/'shaders/lit.frag').unlink()
    run(['--hidden','--frames','1'],cwd=private,expected=1,contains='lit.frag')
    (copied/'shaders/lit.frag').write_bytes(original)
    (copied/'assets/models/claw-lab-r1.glb').unlink()
    run(['--hidden','--frames','1'],cwd=private,expected=1,contains='claw-lab-r1.glb')
    exe=old
    report['package_checks']=['10 rendered rounds: 5 hits / 5 misses','1366x768 PNG sizes','Unicode install path + different CWD + OS-only PATH','invalid shader compile -> nonzero with filename','shader link mismatch -> nonzero','missing shader -> nonzero with filename','missing asset -> nonzero with filename']
report['executable_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
(out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print('PACKAGE PASS:', '; '.join(report['package_checks']))
