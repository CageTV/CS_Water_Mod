"""Builds ShipWake.nif: a flat V (apex at the origin, arms opening towards -Y) of two soft foam strips. The shader and alpha settings are
copied from a vanilla foam effect nif (BSEffectShaderProperty, fxwhitewater + rapid foam gradient, vertex alpha) so the engine path is known good."""
import sys, math, copy, ctypes
sys.path.insert(0, "E:/Tabula Rasa/mods/Skyrim-Claude Code Modder's Toolkit/tools/pynifly/io_scene_nifly")
from pyn.pynifly import NifFile, PynBufferTypes
TEMPLATE = "C:/Modlists/Tabula/mods/CS Water Mod/meshes/architecture/whiterun/wreffects/wrstairsstreamrapids01foam.nif"
OUT = sys.argv[1]

L, SEG, HALF = 1000.0, 10, math.radians(19.0)
verts, uvs, cols, tris = [], [], [], []
for side in (-1, 1):
    base = len(verts)
    for i in range(SEG + 1):
        t = i / SEG
        cx, cy = side * t * L * math.sin(HALF), -t * L * math.cos(HALF)
        w = 25 + 110 * t                      # strip half width grows towards the stern
        nx, ny = math.cos(HALF) * side, math.sin(HALF) * -1 * -1  # perpendicular to the arm direction (in the XY plane)
        # arm direction d = (side*sin, -cos); perpendicular p = (cos*side... ) -> use (cos, side*sin) normalised
        px, py = math.cos(HALF), side * math.sin(HALF)
        a = max(0.0, 1.0 - t) ** 0.8          # opacity fades along the arm
        for k, (off, edge) in enumerate(((-1, 0.0), (0, 1.0), (1, 0.0))):
            verts.append((cx + px * w * off, cy + py * w * off, 0.0))
            uvs.append((k / 2.0, t * 3.0))
            cols.append((1.0, 1.0, 1.0, a * edge * 0.9 + (0.0 if edge else 0.0)))
    for i in range(SEG):
        r0, r1 = base + i * 3, base + (i + 1) * 3
        for k in range(2):
            a0, a1, b0, b1 = r0 + k, r0 + k + 1, r1 + k, r1 + k + 1
            if side < 0:
                tris += [(a0, b0, a1), (a1, b0, b1)]
            else:
                tris += [(a0, a1, b0), (a1, b1, b0)]
normals = [(0.0, 0.0, 1.0)] * len(verts)

tpl = NifFile(TEMPLATE)
ts = tpl.shapes[0]
out = NifFile()
out.initialize('SKYRIMSE', OUT, root_type='NiNode', root_name='ShipWake')
props = copy.deepcopy(ts.properties)   # BSTriShape buffer
sh = out.createShapeFromData('ShipWakeV', verts, tris, uvs, normals, props=None, use_type=PynBufferTypes.BSTriShapeBufType)
sh.set_colors(cols)
# shader + alpha copied from the template
sh.shader._properties = copy.deepcopy(ts.shader.properties)
sh.shader.name = ''
sh.shader._properties.controllerID = 0xFFFFFFFF   # the template's UV-scroll controller is not copied; a stale id here crashed the engine
sh.save_shader_attributes()
if ts.has_alpha_property:
    sh.has_alpha_property = True
    sh.alpha_property._properties = copy.deepcopy(ts.alpha_property.properties)
    sh.save_alpha_property()
out.save()
print('wrote', OUT, len(verts), 'verts', len(tris), 'tris')
