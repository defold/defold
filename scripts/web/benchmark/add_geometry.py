"""Add repeatable model/mesh workloads to a disposable benchmark project."""
import argparse
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('project', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
fixture = root / 'engine/engine/src/test/web_component_2d'
dst = args.project
for pattern in ['model*', 'mesh*', '*.gltf', 'flat.fp', 'triangle.buffer', 'geometry.go']:
    for src in fixture.glob(pattern):
        if src.is_file():
            shutil.copy(src, dst / src.name)
shutil.copy(Path(__file__).with_name('geometry_benchmark.script'), dst / 'geometry_benchmark.script')
(dst / 'geometry.script').write_text('''function init(self)
    local colors = {mesh_world=vmath.vector4(1,1,0,1), mesh_local=vmath.vector4(0,1,1,1),
        static=vmath.vector4(0,0,1,1), cpu=vmath.vector4(1,0,1,1), gpu=vmath.vector4(1,0,0,1),
        instanced_a=vmath.vector4(0,1,0,1), instanced_b=vmath.vector4(0,1,0,1)}
    for name, color in pairs(colors) do go.set("#" .. name, "tint", color) end
    for _, name in ipairs({"cpu", "gpu", "instanced_a", "instanced_b"}) do
        model.play_anim("#" .. name, "move", go.PLAYBACK_LOOP_FORWARD)
    end
end
''')
(dst / 'geometry.factory').write_text('prototype: "/geometry.go"\n')
(dst / 'geometry_benchmark.go').write_text('''components { id: "script" component: "/geometry_benchmark.script" }
components { id: "factory" component: "/geometry.factory" }
''')
(dst / 'geometry_benchmark.collection').write_text('name: "geometry_benchmark"\ninstances { id: "controller" prototype: "/geometry_benchmark.go" }\n')
# Retain the alternate bootstrap scene through a dependency reachable from main.
(dst / 'main/geometry.collectionfactory').write_text('prototype: "/geometry_benchmark.collection"\nload_dynamically: false\n')
with (dst / 'main/unused.go').open('a') as f:
    f.write('components { id: "geometry_resources" component: "/main/geometry.collectionfactory" }\n')
with (dst / 'game.project').open('a') as f:
    f.write('\n[model]\nmax_count = 6000\n[mesh]\nmax_count = 3000\n')
p = dst / 'main/benchmark.render_script'
s = p.read_text().replace('    self.gui =', '    self.models = render.predicate({ "model" })\n    self.gui =')
s = s.replace('    render.enable_state(graphics.STATE_STENCIL_TEST)', '    render.draw(self.models, self.options)\n    render.enable_state(graphics.STATE_STENCIL_TEST)')
p.write_text(s)
