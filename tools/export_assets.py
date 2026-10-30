"""Run in Blender: blender -b mockups/claw-lab-master.blend --python tools/export_assets.py.
Convert evaluated geometry once per shared source, retain hierarchy and Y-up glTF.
Never changes the master .blend or mockup source.
"""
import bpy, hashlib, json, math, struct
from mathutils import Vector, kdtree
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets'
(OUT / 'models').mkdir(parents=True, exist_ok=True)
(OUT / 'design').mkdir(parents=True, exist_ok=True)
raw = (ROOT / 'mockups/design-source.json').read_bytes()
D = json.loads(raw)
(OUT / 'design/design-source.json').write_bytes(raw)
controls = {}
for obj in bpy.data.objects:
    if obj.name == 'finger_0':
        controls = {'source': obj.name, 'radius': obj.data.bevel_depth, 'segments': [
            {'p0': list(a.co), 'p1': list(a.handle_right), 'p2': list(b.handle_left), 'p3': list(b.co)}
            for a, b in zip(obj.data.splines[0].bezier_points, list(obj.data.splines[0].bezier_points)[1:])]}
(OUT / 'design/curve-control-points.json').write_text(json.dumps(controls, indent=2))
cache = {}
for obj in list(bpy.data.objects):
    group = obj.get('group', '')
    if obj.type in {'LIGHT','CAMERA'} or group == 'swatch':
        bpy.data.objects.remove(obj, do_unlink=True)
        continue
    obj.hide_set(False)
    obj.hide_viewport = False
    obj.hide_render = False
    if obj.type in {'CURVE','FONT','MESH'}:
        key = (obj.data.as_pointer(), tuple((m.type, getattr(m,'width',0)) for m in obj.modifiers))
        if key not in cache:
            evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            mesh = bpy.data.meshes.new_from_object(evaluated, preserve_all_data_layers=True, depsgraph=bpy.context.evaluated_depsgraph_get())
            cache[key] = mesh
        mesh = cache[key]
        if obj.type != 'MESH':
            replacement = bpy.data.objects.new(obj.name + '_converted', mesh)
            bpy.context.collection.objects.link(replacement)
            replacement.parent = obj.parent
            replacement.matrix_local = obj.matrix_local.copy()
            for k,v in obj.items(): replacement[k] = v
            name = obj.name
            bpy.data.objects.remove(obj, do_unlink=True)
            replacement.name = name
        else:
            obj.modifiers.clear()
            obj.data = mesh
# Replace the Blender-only mix with a standard glTF alpha material using the same color.
for mid, spec in D['palette'].items():
    mat = bpy.data.materials.get(mid)
    if not mat: continue
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    if mid == 'glass':
        mat.node_tree.links.new(bsdf.outputs['BSDF'], mat.node_tree.nodes['Material Output'].inputs['Surface'])
        bsdf.inputs['Alpha'].default_value = spec['opacity']
        mat.surface_render_method = 'DITHERED'
# A small deterministic woven base-color texture, stored in the GLB; UVs come from Blender.
size=512
image=bpy.data.images.new('capsule_woven_srgb', width=size, height=size)
base=[int(D['palette']['prize']['hex'][i:i+2],16)/255 for i in (1,3,5)]
pixels=[]
for y in range(size):
    for x in range(size):
        v=0.985+0.012*math.sin(x*math.pi/2)*math.sin(y*math.pi/2)
        pixels.extend([c*v for c in base]+[1])
image.pixels=pixels
image.pack()
mat=bpy.data.materials['prize']
tex=mat.node_tree.nodes.new('ShaderNodeTexImage'); tex.image=image
mat.node_tree.links.new(tex.outputs['Color'],mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
bpy.ops.export_scene.gltf(filepath=str(OUT/'models/claw-lab-r1.glb'), export_format='GLB',
    export_yup=True, export_extras=True, export_cameras=False, export_lights=False,
    export_materials='EXPORT', export_animations=False, export_apply=False)
# Compare the actual exported accessors to the evaluated source geometry, in metres.
glb=(OUT/'models/claw-lab-r1.glb').read_bytes()
json_size=struct.unpack_from('<I',glb,12)[0]
gltf=json.loads(glb[20:20+json_size]);binary=glb[28+json_size:]
comparisons=[]
for mi, mesh in enumerate(gltf['meshes']):
    name=next(n['name'] for n in gltf['nodes'] if n.get('mesh')==mi)
    original=bpy.data.objects[name].data
    source=[Vector((v.co.x,v.co.z,-v.co.y)) for v in original.vertices]
    exported=[]
    for primitive in mesh['primitives']:
        accessor=gltf['accessors'][primitive['attributes']['POSITION']]
        view=gltf['bufferViews'][accessor['bufferView']]
        offset=view.get('byteOffset',0)+accessor.get('byteOffset',0)
        exported.extend(Vector(struct.unpack_from('<fff',binary,offset+i*view.get('byteStride',12))) for i in range(accessor['count']))
    def distance_to(points, targets):
        tree=kdtree.KDTree(len(targets))
        for i,p in enumerate(targets):tree.insert(p,i)
        tree.balance()
        return max(tree.find(p)[2] for p in points)
    error=max(distance_to(source,exported),distance_to(exported,source))
    assert error<0.000002,(name,error)
    comparisons.append({'mesh':mi,'source_object':name,'source_vertices':len(source),'export_vertices':len(exported),'max_bidirectional_vertex_error_m':error})
provenance={'design_sha256':hashlib.sha256(raw).hexdigest(),
    'blend_sha256':hashlib.sha256((ROOT/'mockups/claw-lab-master.blend').read_bytes()).hexdigest(),
    'glb_sha256':hashlib.sha256(glb).hexdigest(),'exporter':'Blender '+bpy.app.version_string,
    'axis_mapping':'(x,y,z) -> (x,z,-y), exactly once',
    'mesh_comparisons':comparisons,'max_vertex_error_m':max(c['max_bidirectional_vertex_error_m'] for c in comparisons)}
(OUT/'design/export-provenance.json').write_text(json.dumps(provenance,indent=2))
print('EXPORT_OK', hashlib.sha256(raw).hexdigest(), len(cache), 'shared evaluated meshes')
