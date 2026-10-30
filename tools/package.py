"""Archive, verify every byte after serialization, then launch the extracted package."""
import hashlib, json, subprocess, tempfile, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'dist/claw-lab';archive=ROOT/'output/claw-lab-windows.zip'
files=sorted(p for p in source.rglob('*') if p.is_file())
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for path in files:z.write(path,Path('claw-lab')/path.relative_to(source))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for path in files:assert z.read(str(Path('claw-lab')/path.relative_to(source)).replace('\\','/'))==path.read_bytes()
    build=(ROOT/'build').resolve()
    with tempfile.TemporaryDirectory(prefix='zip-kiểm-tra-',dir=build) as temp:
        extracted=Path(temp).resolve();assert extracted.is_relative_to(build)
        z.extractall(extracted)
        exe=extracted/'claw-lab/claw_lab.exe'
        result=subprocess.run([str(exe),'--hidden','--verify','--size','400x400','--output',str(extracted/'run')],cwd=extracted,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90)
        assert result.returncode==0,result.stderr
        report=json.loads((extracted/'run/verification.json').read_text())
        assert report['runtime_rounds_passed']==10
print('ZIP PASS:',len(files),'files, byte-for-byte validated; extracted Unicode-path package ran 10 rounds;',archive.stat().st_size,'bytes; SHA256',hashlib.sha256(archive.read_bytes()).hexdigest())
