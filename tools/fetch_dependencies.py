"""Fetch pinned source releases, keeping their licenses. No global installation."""
import hashlib, io, json, urllib.request, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
DEPS = {
    'glfw': ('glfw/glfw', '3.4'),
    'glm': ('g-truc/glm', '1.0.1'),
    'tinygltf': ('syoyo/tinygltf', 'v2.9.5'),
    'catch2': ('catchorg/Catch2', 'v3.8.1'),
    'imgui': ('ocornut/imgui', 'v1.91.9b'),
}
lock_path = ROOT / 'cmake/dependencies.lock.json'
lock = json.loads(lock_path.read_text()) if lock_path.exists() else {}
for name, (repo, tag) in DEPS.items():
    dst = ROOT / 'third_party' / name
    if dst.exists():
        continue
    url = f'https://codeload.github.com/{repo}/zip/refs/tags/{tag}'
    raw = urllib.request.urlopen(url, timeout=90).read()
    digest = hashlib.sha256(raw).hexdigest()
    if name in lock and lock[name]['sha256'] != digest:
        raise RuntimeError(f'Archive integrity mismatch: {name}')
    archive = zipfile.ZipFile(io.BytesIO(raw))
    prefix = archive.namelist()[0].split('/')[0] + '/'
    for member in archive.infolist():
        rel = member.filename[len(prefix):]
        if not rel or member.is_dir(): continue
        out = dst / rel
        if not out.resolve().is_relative_to(dst.resolve()): raise RuntimeError('Unsafe archive')
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(archive.read(member))
    lock[name] = {'repository': repo, 'tag': tag, 'url': url, 'sha256': digest}
    lock_path.write_text(json.dumps(lock, indent=2) + '\n')
    print(f'{name} {tag} {digest}', flush=True)
