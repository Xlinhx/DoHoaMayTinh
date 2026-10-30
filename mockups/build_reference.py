"""Deterministic reference-art authoring, not the C++ game implementation.

Run with Blender: blender -b --factory-startup -t 8 --python build_reference.py -- --only 01-hero
No external assets or add-ons are needed. design-source.json owns all shared values.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
DESIGN = json.loads((ROOT / 'design-source.json').read_text(encoding='utf-8'))
PARSER = argparse.ArgumentParser()
PARSER.add_argument('--only', default='')
PARSER.add_argument('--size', type=int, default=1400)
PARSER.add_argument('--samples', type=int, default=48)
ARGS = PARSER.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
RENDERS = ROOT / 'renders'
RENDERS.mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
SCENE = bpy.context.scene
bpy.context.preferences.filepaths.save_version = 0
SCENE.render.engine = 'BLENDER_EEVEE'
SCENE.eevee.taa_render_samples = ARGS.samples
SCENE.render.resolution_x = ARGS.size
SCENE.render.resolution_y = ARGS.size
SCENE.render.resolution_percentage = 100
SCENE.render.image_settings.file_format = 'PNG'
SCENE.render.film_transparent = False
SCENE.render.image_settings.color_mode = 'RGB'
SCENE.view_settings.view_transform = 'AgX'
SCENE.world.color = (0.55, 0.55, 0.55)
SCENE.world.use_nodes = True
SCENE.world.node_tree.nodes['Background'].inputs['Color'].default_value = (0.72, 0.78, 0.75, 1)
SCENE.world.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.45
MATS = {}
PARTS = {}
PRIZES = {}
JOINTS = []


def linear(v):
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def color(hex_value):
    return tuple(linear(int(hex_value[i:i+2], 16) / 255) for i in (1, 3, 5)) + (1,)


for mat_id, spec in DESIGN['palette'].items():
    mat = bpy.data.materials.new(mat_id)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = color(spec['hex'])
    bsdf.inputs['Roughness'].default_value = spec['roughness']
    bsdf.inputs['Metallic'].default_value = spec['metallic']
    if mat_id == 'led':
        bsdf.inputs['Emission Color'].default_value = color(spec['hex'])
        bsdf.inputs['Emission Strength'].default_value = 2.5
    if mat_id == 'glass':
        transparent = mat.node_tree.nodes.new('ShaderNodeBsdfTransparent')
        mix = mat.node_tree.nodes.new('ShaderNodeMixShader')
        mix.inputs[0].default_value = spec['opacity']
        mat.node_tree.links.new(transparent.outputs[0], mix.inputs[1])
        mat.node_tree.links.new(bsdf.outputs[0], mix.inputs[2])
        mat.node_tree.links.new(mix.outputs[0], mat.node_tree.nodes['Material Output'].inputs['Surface'])
        mat.surface_render_method = 'DITHERED'
    if mat_id == 'prize':
        noise = mat.node_tree.nodes.new('ShaderNodeTexNoise')
        noise.inputs['Scale'].default_value = 135
        bump = mat.node_tree.nodes.new('ShaderNodeBump')
        bump.inputs['Strength'].default_value = 0.20
        bump.inputs['Distance'].default_value = 0.0007
        mat.node_tree.links.new(noise.outputs['Fac'], bump.inputs['Height'])
        mat.node_tree.links.new(bump.outputs[0], bsdf.inputs['Normal'])
    MATS[mat_id] = mat


def register(obj, name, material, group='body', parent=None):
    obj.name = name
    obj['asset_part_id'] = name
    obj['group'] = group
    if material:
        obj.data.materials.append(MATS[material])
    if parent:
        obj.parent = parent
    PARTS[name] = obj
    return obj


def cube(name, loc, dims, material, bevel=0.008, group='body', parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = register(bpy.context.object, name, material, group, parent)
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        modifier = obj.modifiers.new('edge_radius', 'BEVEL')
        modifier.width = bevel
        modifier.segments = 3
        obj.modifiers.new('weighted_normals', 'WEIGHTED_NORMAL')
    return obj


def cylinder(name, loc, radius, depth, material, group='body', parent=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=radius, depth=depth, location=loc)
    obj = register(bpy.context.object, name, material, group, parent)
    bevel = obj.modifiers.new('edge_radius', 'BEVEL')
    bevel.width = min(0.004, depth * 0.2)
    bevel.segments = 3
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def sphere(name, loc, dims, material, group='body', parent=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=40, ring_count=24, radius=0.5, location=loc)
    obj = register(bpy.context.object, name, material, group, parent)
    obj.scale = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def curve(name, points, radius, material, group='body', parent=None, closed=False, bezier=False):
    data = bpy.data.curves.new(name, 'CURVE')
    data.dimensions = '3D'
    data.resolution_u = 12
    data.bevel_depth = radius
    data.bevel_resolution = 3
    spline = data.splines.new('BEZIER' if bezier else 'POLY')
    if bezier:
        spline.bezier_points.add(len(points) - 1)
        for bp, p in zip(spline.bezier_points, points):
            bp.co = p
            bp.handle_left_type = 'AUTO'
            bp.handle_right_type = 'AUTO'
    else:
        spline.points.add(len(points) - 1)
        for bp, p in zip(spline.points, points):
            bp.co = (*p, 1)
    spline.use_cyclic_u = closed
    obj = bpy.data.objects.new(name, data)
    SCENE.collection.objects.link(obj)
    return register(obj, name, material, group, parent)


def empty(name, loc=(0, 0, 0), parent=None, group='head'):
    obj = bpy.data.objects.new(name, None)
    SCENE.collection.objects.link(obj)
    obj.location = loc
    if parent:
        obj.parent = parent
    obj['group'] = group
    PARTS[name] = obj
    return obj


def text(name, body, loc, size, material='cream', group='body'):
    data = bpy.data.curves.new(name, 'FONT')
    data.body = body
    data.align_x = 'CENTER'
    data.align_y = 'CENTER'
    data.size = size
    data.extrude = 0.0005
    obj = bpy.data.objects.new(name, data)
    SCENE.collection.objects.link(obj)
    register(obj, name, material, group)
    obj.location = loc
    obj.rotation_euler = (math.pi / 2, 0, 0)
    return obj


# Cabinet: a shell, a physical floor opening and an actual receiving pocket.
cube('base_plinth', (0, 0, 0.065), (0.90, 0.76, 0.09), 'dark', 0.025)
cube('base_back', (0, 0.354, 0.365), (0.86, 0.045, 0.55), 'teal', 0.012)
cube('base_left', (-0.424, 0, 0.365), (0.045, 0.71, 0.55), 'teal', 0.012)
cube('base_right', (0.424, 0, 0.365), (0.045, 0.71, 0.55), 'teal', 0.012)
cube('front_right', (0.145, -0.357, 0.365), (0.55, 0.045, 0.55), 'teal', 0.01)
cube('front_left_pillar', (-0.408, -0.357, 0.365), (0.035, 0.045, 0.55), 'teal', 0.006)
cube('front_window_upper', (-0.265, -0.357, 0.535), (0.255, 0.045, 0.21), 'teal', 0.006)
cube('front_window_lower', (-0.265, -0.357, 0.175), (0.255, 0.045, 0.16), 'teal', 0.006)
cube('tray_floor', (-0.255, -0.30, 0.217), (0.245, 0.25, 0.022), 'dark', 0.01)
cube('tray_backstop', (-0.255, -0.165, 0.32), (0.245, 0.025, 0.21), 'dark', 0.006)
cube('tray_leftwall', (-0.379, -0.28, 0.32), (0.02, 0.23, 0.21), 'dark', 0.004)
cube('tray_rightwall', (-0.131, -0.28, 0.32), (0.02, 0.23, 0.21), 'dark', 0.004)
cube('tray_lip', (-0.255, -0.425, 0.23), (0.27, 0.035, 0.055), 'orange', 0.009)
text('tray_label', 'PICK UP', (-0.255, -0.384, 0.475), 0.033)
for x in (-0.34, 0.34):
    for y in (-0.27, 0.27):
        cylinder('foot_%s_%s' % (x, y), (x, y, 0.018), 0.04, 0.036, 'dark')

floor_z = DESIGN['cabinet']['floor_z']
# The four non-overlapping pieces surround x[-.37,-.14], y[-.30,-.08].
cube('floor_main', (0.13, 0, floor_z - 0.018), (0.54, 0.66, 0.036), 'cream', 0.005)
cube('floor_back_left', (-0.27, 0.125, floor_z - 0.018), (0.26, 0.41, 0.036), 'cream', 0.005)
cube('floor_left_rim', (-0.385, -0.19, floor_z - 0.018), (0.03, 0.22, 0.036), 'cream', 0.003)
cube('floor_front_rim', (-0.27, -0.315, floor_z - 0.018), (0.26, 0.03, 0.036), 'cream', 0.003)
for name, loc, dims in [
    ('chute_left', (-0.38, -0.19, 0.725), (0.018, 0.24, 0.06)),
    ('chute_right', (-0.13, -0.19, 0.725), (0.018, 0.24, 0.06)),
    ('chute_back', (-0.255, -0.065, 0.725), (0.25, 0.018, 0.06)),
    ('chute_front', (-0.255, -0.315, 0.725), (0.25, 0.018, 0.06))]:
    cube(name, loc, dims, 'orange', 0.004)

for x in (-0.423, 0.423):
    for y in (-0.35, 0.35):
        cube('post_%s_%s' % (x, y), (x, y, 1.185), (0.045, 0.045, 1.05), 'cream', 0.009)
cube('bottom_frame_front', (0, -0.355, 0.662), (0.90, 0.05, 0.07), 'cream', 0.012)
cube('bottom_frame_back', (0, 0.355, 0.662), (0.90, 0.05, 0.07), 'cream', 0.012)
for x in (-0.425, 0.425):
    cube('bottom_frame_side_%s' % x, (x, 0, 0.662), (0.05, 0.66, 0.07), 'cream', 0.012)
cube('roof_crown', (0, 0, 1.745), (0.90, 0.76, 0.11), 'cream', 0.028, 'roof')
cube('roof_teal_insert', (0, -0.384, 1.745), (0.69, 0.012, 0.071), 'teal', 0.011, 'roof')
text('roof_wordmark', 'CLAW LAB', (0, -0.393, 1.746), 0.047, 'cream', 'roof')
cube('roof_underside', (0, 0, 1.683), (0.81, 0.67, 0.018), 'teal', 0.006, 'roof')
for x in (-0.34, 0.34):
    cube('led_strip_%s' % x, (x, 0, 1.668), (0.014, 0.54, 0.009), 'led', 0.003, 'roof')

cube('glass_front', (0, -0.354, 1.18), (0.80, 0.003, 0.97), 'glass', 0.001, 'glass')
cube('glass_back', (0, 0.354, 1.18), (0.80, 0.003, 0.97), 'glass', 0.001, 'glass')
cube('glass_right', (0.426, 0, 1.18), (0.003, 0.66, 0.97), 'glass', 0.001, 'glass')
cube('glass_left', (-0.426, 0, 1.18), (0.003, 0.66, 0.97), 'glass', 0.001, 'glass')
for x in (-0.394, 0.394):
    cube('glass_front_edge_%s' % x, (x, -0.356, 1.18), (0.002, 0.002, 0.93), 'steel', 0)

cube('console', (0, -0.425, 0.625), (0.88, 0.21, 0.085), 'cream', 0.018)
cube('console_teal_plate', (0.12, -0.427, 0.672), (0.48, 0.15, 0.013), 'teal', 0.012)
cylinder('joystick_socket', (0.04, -0.434, 0.686), 0.028, 0.016, 'dark')
cylinder('joystick_stem', (0.04, -0.434, 0.717), 0.009, 0.055, 'steel')
sphere('joystick_grip', (0.04, -0.434, 0.753), (0.043, 0.043, 0.043), 'orange')
cylinder('grab_button_rim', (0.26, -0.434, 0.687), 0.032, 0.015, 'dark')
cylinder('grab_button', (0.26, -0.434, 0.699), 0.025, 0.018, 'orange')
cube('coin_slot_plate', (0.265, -0.386, 0.444), (0.12, 0.009, 0.15), 'dark', 0.01)
cube('coin_slot', (0.265, -0.393, 0.475), (0.065, 0.005, 0.009), 'steel', 0.003)
text('coin_label', '01', (0.265, -0.397, 0.412), 0.034, 'cream')
for i in range(5):
    cube('rear_vent_%s' % i, (0.22, 0.378, 0.25 + i * 0.036), (0.24, 0.003, 0.009), 'dark', 0.001)

# Gantry: two stationary Y rails; the X bridge translates in Y; trolley in X.
for x in (-0.325, 0.325):
    rail = cylinder('fixed_y_rail_%s' % x, (x, 0, 1.62), 0.009, 0.61, 'steel', 'gantry')
    rail.rotation_euler.x = math.pi / 2
    for y in (-0.30, 0.30):
        cube('rail_mount_%s_%s' % (x, y), (x, y, 1.645), (0.045, 0.045, 0.065), 'teal', 0.004, 'gantry')
BRIDGE = empty('bridge_y', group='gantry')
cube('bridge_beam', (0, 0, 1.61), (0.71, 0.038, 0.045), 'teal', 0.007, 'gantry', BRIDGE)
rail = cylinder('moving_x_rail', (0, -0.032, 1.595), 0.008, 0.61, 'steel', 'gantry', BRIDGE)
rail.rotation_euler.y = math.pi / 2
TROLLEY = empty('trolley_x', parent=BRIDGE, group='gantry')
cube('trolley_body', (0, -0.015, 1.57), (0.10, 0.095, 0.065), 'orange', 0.012, 'gantry', TROLLEY)
for x in (-0.04, 0.04):
    cylinder('trolley_wheel_%s' % x, (x, 0.035, 1.62), 0.017, 0.016, 'dark', 'gantry', TROLLEY)
HEAD = empty('claw_head', parent=TROLLEY)
cylinder('claw_housing', (0, 0, 0), 0.047, 0.055, 'teal', 'head', HEAD)
cylinder('claw_housing_ring', (0, 0, -0.021), 0.049, 0.012, 'orange', 'head', HEAD)
sphere('claw_cap', (0, 0, 0.025), (0.085, 0.085, 0.035), 'steel', 'head', HEAD)
CABLE = cylinder('lift_cable', (0, 0, 1.53), 0.004, 0.1, 'dark', 'gantry', TROLLEY)
finger_source = None
for index in range(3):
    azimuth = empty('finger_azimuth_%s' % index, parent=HEAD)
    azimuth.rotation_euler.z = math.radians(index * 120 + 30)
    pivot = empty('finger_joint_%s' % index, (0.035, 0, -0.025), azimuth)
    JOINTS.append(pivot)
    if finger_source is None:
        finger_source = curve('finger_0', [(0, 0, 0), (0.07, 0, -0.125), (0.02, 0, -0.255)], 0.0075, 'steel', 'head', pivot, bezier=True)
    else:
        obj = bpy.data.objects.new('finger_%s' % index, finger_source.data)
        SCENE.collection.objects.link(obj)
        register(obj, obj.name, None, 'head', pivot)
    joint = cylinder('joint_pin_%s' % index, (0, 0, 0), 0.013, 0.019, 'orange', 'head', pivot)
    joint.rotation_euler.x = math.pi / 2
    sphere('finger_pad_%s' % index, (0.02, 0, -0.255), (0.021, 0.021, 0.025), 'dark', 'head', pivot)

# Eight instances share the *same* mesh/curve datablocks and material slots.
prize_parts = []
for index, entry in enumerate(DESIGN['prizes']):
    root = empty(entry['id'], group='prize')
    root['asset_id'] = DESIGN['prize_asset']['id']
    PRIZES[entry['id']] = root
    if index == 0:
        dimensions = DESIGN['prize_asset']['dimensions']
        prize_parts.append(sphere('prize_01_volume', (0, 0, 0), dimensions, 'prize', 'prize', root))
        points = [((dimensions[0]/2 + 0.0003) * math.cos(a), (dimensions[1]/2 + 0.0003) * math.sin(a), 0) for a in [j * math.tau / 96 for j in range(96)]]
        prize_parts.append(curve('prize_01_seam', points, 0.0013, 'seam', 'prize', root, closed=True))
        for j in range(16):
            a = j * math.tau / 16
            p = ((dimensions[0]/2 + 0.0008) * math.cos(a), (dimensions[1]/2 + 0.0008) * math.sin(a))
            prize_parts.append(curve('prize_01_stitch_%02d' % j, [(p[0], p[1], -0.0035), (p[0], p[1], 0.0035)], 0.00055, 'cream', 'prize', root))
    else:
        for source in prize_parts:
            obj = bpy.data.objects.new(entry['id'] + '_' + source.name.split('_', 2)[-1], source.data)
            SCENE.collection.objects.link(obj)
            register(obj, obj.name, None, 'prize', root)

cube('studio_ground', (0, 0, -0.009), (200, 200, 0.018), 'cream', 0, 'studio')


def area(name, loc, target, power, size, tint):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy = power
    data.shape = 'DISK'
    data.size = size
    data.color = tint
    obj = bpy.data.objects.new(name, data)
    SCENE.collection.objects.link(obj)
    obj.location = loc
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


area('studio_key', (3, -4, 5), (0, 0, 0.9), 550, 4.0, (1, 0.93, 0.83))
area('studio_fill', (-3, -1, 3), (0, 0, 1), 380, 3.5, (0.83, 0.95, 1))
area('studio_rim', (1, 3, 4), (0, 0, 1), 550, 3.0, (1, 1, 1))
area('cabinet_light', (0, 0, 1.66), (0, 0, 0.7), 5, 0.6, (1, 0.88, 0.69))
CAMERA_DATA = bpy.data.cameras.new('reference_camera')
CAMERA = bpy.data.objects.new('reference_camera', CAMERA_DATA)
SCENE.collection.objects.link(CAMERA)
SCENE.camera = CAMERA
CAMERA_DATA.type = 'ORTHO'
CAMERA_DATA.lens = 48

# Material samples are separate inspection geometry, never substituted into gameplay.
SWATCHES = []
for index, mat in enumerate(['cream', 'teal', 'orange', 'steel', 'prize', 'glass']):
    obj = sphere('sample_' + mat, ((index % 3 - 1) * 0.30, 0, 0.25 + (1 - index // 3) * 0.35), (0.22, 0.22, 0.22), mat, 'swatch')
    SWATCHES.append(obj)


def set_state(name):
    state = DESIGN['states'][name]
    BRIDGE.location.y = state['xy'][1]
    TROLLEY.location.x = state['xy'][0]
    HEAD.location.z = state['head_z']
    cable_top = 1.54
    cable_bottom = state['head_z'] + 0.04
    CABLE.location.z = (cable_top + cable_bottom) / 2
    CABLE.scale.z = (cable_top - cable_bottom) / 0.1
    for joint in JOINTS:
        joint.rotation_euler.y = math.radians(state['finger_deg'])
    for entry in DESIGN['prizes']:
        obj = PRIZES[entry['id']]
        obj.location = (*entry['xy'], floor_z + DESIGN['prize_asset']['dimensions'][2] / 2)
        obj.rotation_euler.z = math.radians(entry['yaw_deg'])
    target = PRIZES[DESIGN['prize_asset']['target_id']]
    if state['target'] == 'held':
        target.location = (*state['xy'], state['head_z'] + DESIGN['claw']['grip_offset_z'])
    elif state['target'] == 'falling':
        target.location = (*state['xy'], state['target_z'])
    elif state['target'] == 'delivered':
        target.location = (*DESIGN['chute']['delivery_xy'], DESIGN['chute']['delivery_z'])
    bpy.context.view_layer.update()


def asset_fingerprint():
    payload = {'objects': [], 'geometry': {}, 'materials': {}}
    for obj in PARTS.values():
        if obj.type in {'MESH', 'CURVE', 'FONT'}:
            payload['objects'].append([obj.name, obj.data.name, [m.name for m in obj.data.materials]])
            if obj.data.name in payload['geometry']:
                continue
            if obj.type == 'MESH':
                geometry = {'vertices': [[round(v, 8) for v in vertex.co] for vertex in obj.data.vertices], 'faces': [list(face.vertices) for face in obj.data.polygons]}
            elif obj.type == 'CURVE':
                geometry = {'radius': obj.data.bevel_depth, 'splines': []}
                for spline in obj.data.splines:
                    geometry['splines'].append({'type': spline.type, 'closed': spline.use_cyclic_u, 'points': [[list(p.co), list(p.handle_left), list(p.handle_right)] for p in spline.bezier_points] if spline.type == 'BEZIER' else [list(p.co) for p in spline.points]})
            else:
                geometry = {'text': obj.data.body, 'size': obj.data.size, 'extrude': obj.data.extrude}
            payload['geometry'][obj.data.name] = geometry
    for name, mat in MATS.items():
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        payload['materials'][name] = {'color': list(bsdf.inputs['Base Color'].default_value), 'roughness': bsdf.inputs['Roughness'].default_value, 'metallic': bsdf.inputs['Metallic'].default_value}
    return hashlib.sha256(json.dumps(payload, sort_keys=True).encode()).hexdigest()


def world_bounds(objects):
    points = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        if obj.type not in {'MESH', 'CURVE', 'FONT'}:
            continue
        evaluated = obj.evaluated_get(depsgraph)
        if obj.type == 'CURVE':
            mesh = evaluated.to_mesh()
            points.extend(evaluated.matrix_world @ vertex.co for vertex in mesh.vertices)
            evaluated.to_mesh_clear()
        else:
            points.extend(evaluated.matrix_world @ Vector(corner) for corner in evaluated.bound_box)
    return {'min': [min(p[i] for p in points) for i in range(3)],
            'max': [max(p[i] for p in points) for i in range(3)]}


BASE_FINGERPRINT = asset_fingerprint()
CHECKS = []
assert len(JOINTS) == DESIGN['claw']['finger_count'] == 3
finger_objects = [PARTS['finger_%s' % i] for i in range(3)]
assert len({o.data.as_pointer() for o in finger_objects}) == 1
volumes = [next(o for o in root.children if o.type == 'MESH') for root in PRIZES.values()]
assert len({o.data.as_pointer() for o in volumes}) == 1
bpy.context.view_layer.update()
expected_prize_size = DESIGN['prize_asset']['dimensions']
for volume in volumes:
    assert all(abs(volume.dimensions[i] - expected_prize_size[i]) < 0.0002 for i in range(3)), (volume.name, tuple(volume.dimensions), expected_prize_size)
assert len(PRIZES) == 8 and len({o.as_pointer() for o in PRIZES.values()}) == 8
for root in PRIZES.values():
    actual_materials = {m.name for obj in root.children for m in obj.data.materials}
    assert actual_materials == set(DESIGN['prize_asset']['material_ids']), ('prize material identity', actual_materials)
CHECKS.extend(['3 linked claw fingers', '8 prize instances share volume mesh', 'all prize dimensions equal canonical dimensions', 'one unique target instance'])

for state_id in DESIGN['states']:
    set_state(state_id)
    assert asset_fingerprint() == BASE_FINGERPRINT
    target = PRIZES[DESIGN['prize_asset']['target_id']]
    if DESIGN['states'][state_id]['target'] == 'held':
        head_world = HEAD.matrix_world.translation
        assert abs((target.location - head_world).z - DESIGN['claw']['grip_offset_z']) < 1e-6
    x, y = DESIGN['states'][state_id]['xy']
    assert DESIGN['cabinet']['rail_limit_x'][0] <= x <= DESIGN['cabinet']['rail_limit_x'][1]
    assert DESIGN['cabinet']['rail_limit_y'][0] <= y <= DESIGN['cabinet']['rail_limit_y'][1]
CHECKS.extend(['all 8 states preserve asset fingerprint', 'held target follows head offset', 'all poses stay inside gantry travel'])

# Test the swept envelope at every travel corner, not only the staged photos.
envelopes = []
for x in DESIGN['cabinet']['rail_limit_x']:
    for y in DESIGN['cabinet']['rail_limit_y']:
        for z in DESIGN['cabinet']['head_z']:
            for alpha in (0, 1/3, 2/3, 1):
                angle = (1-alpha) * DESIGN['claw']['finger_open_deg'] + alpha * DESIGN['claw']['finger_closed_deg']
                BRIDGE.location.y = y
                TROLLEY.location.x = x
                HEAD.location.z = z
                for joint in JOINTS:
                    joint.rotation_euler.y = math.radians(angle)
                bpy.context.view_layer.update()
                bounds = world_bounds(HEAD.children_recursive)
                envelopes.append({'head': [x, y, z], 'joint_deg': angle, 'bounds': bounds})
                for axis in (0, 1):
                    inner = DESIGN['cabinet']['inner_glass_bounds_xy'][axis]
                    assert bounds['min'][axis] > inner[0] and bounds['max'][axis] < inner[1], ('claw/glass clearance', axis, x, y, angle, bounds)
                assert bounds['min'][2] >= floor_z, ('claw/floor clearance', z, angle, bounds)
CHECKS.append('32 sampled extreme-travel poses keep evaluated claw envelope inside glass and above floor')
minimum_clearance = min(min(e['bounds']['min'][axis]-DESIGN['cabinet']['inner_glass_bounds_xy'][axis][0], DESIGN['cabinet']['inner_glass_bounds_xy'][axis][1]-e['bounds']['max'][axis]) for e in envelopes for axis in (0,1))

set_state('idle')
body_objects = [o for o in PARTS.values() if o.get('group') in {'body','roof','glass'}]
cabinet_bounds = world_bounds(body_objects)
measured_dimensions = [cabinet_bounds['max'][i]-cabinet_bounds['min'][i] for i in range(3)]
for actual, expected in zip(measured_dimensions, [DESIGN['cabinet']['width'],DESIGN['cabinet']['overall_depth'],DESIGN['cabinet']['height']]):
    assert abs(actual-expected) < 0.0005, ('cabinet dimension mismatch', measured_dimensions)
CHECKS.append('evaluated cabinet bounds equal canonical 0.90 x 0.91 x 1.80 metres')

def set_shot(shot):
    set_state(shot['state'])
    for obj in PARTS.values():
        obj.hide_render = obj.get('group') == 'swatch'
    for group in shot.get('hide', []):
        for obj in PARTS.values():
            if obj.get('group') == group:
                obj.hide_render = True
    if shot.get('isolate'):
        keep = set(HEAD.children_recursive) | {HEAD}
        target = PRIZES[DESIGN['prize_asset']['target_id']]
        keep |= set(target.children_recursive) | {target}
        if shot['isolate'] == 'prize':
            keep = set(target.children_recursive) | {target}
        for obj in PARTS.values():
            if obj not in keep:
                obj.hide_render = True
    if shot.get('materials_board'):
        for obj in PARTS.values():
            obj.hide_render = obj.get('group') not in {'studio', 'swatch'}
    CAMERA.location = shot['camera']
    target = Vector(shot['look_at'])
    CAMERA_DATA.ortho_scale = shot['ortho']
    CAMERA.rotation_euler = (target - CAMERA.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.view_layer.update()


set_shot(DESIGN['shots'][0])
SCENE['design_revision'] = DESIGN['revision']
SCENE['design_source_sha256'] = hashlib.sha256((ROOT / 'design-source.json').read_bytes()).hexdigest()
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'claw-lab-master.blend'))
selected = set(ARGS.only.split(',')) if ARGS.only else None
for shot in DESIGN['shots']:
    if selected is not None and shot['id'] not in selected:
        continue
    set_shot(shot)
    assert asset_fingerprint() == BASE_FINGERPRINT
    SCENE.render.filepath = str(RENDERS / (shot['id'] + '.png'))
    print('RENDER_START', shot['id'], flush=True)
    bpy.ops.render.render(write_still=True)
    print('RENDER_DONE', shot['id'], flush=True)

report = {
    'revision': DESIGN['revision'],
    'source_sha256': hashlib.sha256((ROOT / 'design-source.json').read_bytes()).hexdigest(),
    'asset_identity_fingerprint': BASE_FINGERPRINT,
    'blender': bpy.app.version_string,
    'checks': CHECKS,
    'measurements': {'cabinet_bounds': cabinet_bounds, 'overall_dimensions_m': measured_dimensions, 'minimum_sampled_glass_clearance_m': minimum_clearance, 'claw_envelope_poses': len(envelopes)},
    'rendered': [p.name for p in sorted(RENDERS.glob('*.png'))],
    'render_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(RENDERS.glob('*.png'))},
    'limits': ['mockups, not a running game', 'no dynamic contact/rigid body proof', 'Blender lighting is a look-development target, not OpenGL shader parity proof']
}
(ROOT / 'reference-checks.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('REFERENCE_CHECKS', json.dumps(report, ensure_ascii=False), flush=True)
