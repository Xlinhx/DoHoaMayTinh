"""Validate source-project Markdown links and gitignore behavior in a disposable repo.
Does not initialize Git in the project or claim that prose is semantically correct.
"""
import re, subprocess, tempfile
from pathlib import Path
from urllib.parse import unquote
ROOT=Path(__file__).resolve().parents[1]
pages=[ROOT/'README.md',ROOT/'THIRD_PARTY_NOTICES.md',*sorted((ROOT/'docs').glob('*.md')),ROOT/'mockups/README.md']
count=0
for page in pages:
    text=page.read_text(encoding='utf-8')
    fences=re.findall(r'^```.*$',text,re.M)
    assert len(fences)%2==0,('Unclosed code fence',page)
    prose=re.sub(r'^```[^\n]*\n.*?^```\s*$', '', text, flags=re.M|re.S)
    for link in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)',prose):
        target=link.split('#',1)[0]
        if not target or re.match(r'^[a-zA-Z][a-zA-Z0-9+.-]*:',target):continue
        path=(page.parent/unquote(target)).resolve()
        assert path.is_relative_to(ROOT) and path.exists(),('Broken/outside local link',page,link)
        count+=1
with tempfile.TemporaryDirectory(prefix='claw-docs-ignore-') as temp:
    p=Path(temp);(p/'.gitignore').write_bytes((ROOT/'.gitignore').read_bytes())
    subprocess.run(['git','init','--quiet',str(p)],check=True,capture_output=True)
    ignored=['build/release/claw_lab.exe','dist/claw-lab/assets/a.glb','third_party/glfw/CMakeLists.txt','output/claw-lab-windows.zip','output/runtime/reference/01-hero.png','mockups/a.blend1','tools/__pycache__/test.pyc','CMakeUserPresets.json']
    kept=['src/main.cpp','shaders/lit.frag','vendor/glad/src/gl.c','cmake/dependencies.lock.json','assets/models/claw-lab-r1.glb','assets/design/export-provenance.json','mockups/claw-lab-master.blend','mockups/renders/01-hero.png','docs/README.md','licenses/GLFW.txt','.clang-format']
    result=subprocess.run(['git','-C',str(p),'check-ignore','--no-index','--stdin','-z'],input=('\0'.join(ignored+kept)+'\0').encode(),capture_output=True)
    assert result.returncode==0,result.stderr
    actual=set(result.stdout.decode().rstrip('\0').split('\0'));assert actual==set(ignored),(actual-set(ignored),set(ignored)-actual)
print(f'DOCS PASS: {len(pages)} Markdown files; {count} existing local links; balanced fences; gitignore ignores {len(ignored)} generated paths and preserves {len(kept)} source/asset paths. Prose still requires source review.')
