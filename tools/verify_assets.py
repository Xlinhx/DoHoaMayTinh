"""Verify packaged GLB binding and shared identity; no Blender required."""
import hashlib, json, struct, sys
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1]
design_bytes=(root/'assets/design/design-source.json').read_bytes()
design=json.loads(design_bytes)
binding=json.loads((root/'assets/design/export-provenance.json').read_text())
glb=(root/'assets/models/claw-lab-r1.glb').read_bytes()
assert hashlib.sha256(glb).hexdigest()==binding['glb_sha256']
assert hashlib.sha256(design_bytes).hexdigest()==binding['design_sha256']
if (root/'mockups/design-source.json').exists():
    assert design_bytes==(root/'mockups/design-source.json').read_bytes()
    assert binding['blend_sha256']==hashlib.sha256((root/'mockups/claw-lab-master.blend').read_bytes()).hexdigest()
assert len(binding['mesh_comparisons'])==102
assert binding['max_vertex_error_m']<.000002
size=struct.unpack_from('<I',glb,12)[0];g=json.loads(glb[20:20+size]);nodes={n['name']:n for n in g['nodes']}
assert len(g['meshes'])==102
for suffix in ['volume','seam']+[f'stitch_{i:02d}' for i in range(16)]:
    assert len({nodes[f'prize_{i:02d}_{suffix}']['mesh'] for i in range(1,9)})==1,suffix
assert len({nodes[f'finger_{i}']['mesh'] for i in range(3)})==1
assert len([n for n in g['nodes'] if n['name'] in [p['id'] for p in design['prizes']]])==8
assert all(not n['name'].startswith('sample_') for n in g['nodes'])
assert all(m.get('pbrMetallicRoughness',{}).get('baseColorTexture',{}).get('index',-1)>=0 for m in g['materials'] if m['name']=='prize')
print(f'ASSET PASS: 102 evaluated meshes; bidirectional vertex error <= {binding["max_vertex_error_m"]:.9g} m; 8 shared prizes; 3 shared fingers; hashes match source and GLB')
