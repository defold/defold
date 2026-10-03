"""Prepare a disposable web port of the existing native workloads and Bunnymark."""
import argparse
from pathlib import Path
import shutil,re
parser=argparse.ArgumentParser()
parser.add_argument('native_project',type=Path)
parser.add_argument('bunnymark_project',type=Path)
parser.add_argument('output',type=Path)
parser.add_argument('--memory-probe', action='store_true')
args=parser.parse_args()
src=args.native_project
dst=args.output;dst.mkdir(parents=True,exist_ok=False)
for sub in ['assets','benchmark','main','input']:
 shutil.copytree(src/sub,dst/sub,dirs_exist_ok=True)
shutil.copy(src/'game.project',dst/'game.project')
p=dst/'main/benchmark.go';s=p.read_text();s=re.sub(r'components \{\s*id: "gui".*?\}', '',s,flags=re.S);p.write_text(s)
p=dst/'main/benchmark.script';s=p.read_text().replace('function init(self)','function init(self)\n    sys.set_error_handler(function(_, message, traceback) io.write("WEB_ERROR " .. message .. " " .. traceback .. "\\n"); io.flush(); sys.exit(1) end)',1)
s=s.replace('    self.phase = name','    io.write("WEB_PHASE " .. name .. "\\n"); io.flush()\n    self.phase = name',1)
s=s.replace('            print("BENCHMARK_SAVED " .. self.config.output_path)', '            io.write("WEB_RESULT " .. json.encode(self.result) .. "\\n"); io.flush()')
p.write_text(s)
s=(dst/'game.project').read_text().replace('mixed_preparation = 1','mixed_preparation = 0')+'\n[sound]\nuse_thread = 0\nmax_sound_instances = 0\n'
(dst/'game.project').write_text(s)
bunny=args.bunnymark_project
shutil.copytree(bunny/'example',dst/'example',dirs_exist_ok=True)
p=dst/'example/bunnymark.script';s=p.read_text();s=s.replace('function init(self)','function init(self)\n\tsys.set_error_handler(function(_, message, traceback) io.write("WEB_ERROR " .. message .. " " .. traceback .. "\\n"); io.flush(); sys.exit(1) end)\n\tself.bench_start = sprite._snapshot_clock()\n\tself.bench_phase = "warmup"\n\tself.bench_frames = 0',1)
s=s.replace('function update(self, dt)', '''function update(self, dt)
    local clock = sprite._snapshot_clock()
    local warmup = sys.get_config_number("benchmark.warmup_seconds", 5)
    local duration = sys.get_config_number("benchmark.measure_seconds", 15)
    if self.bench_phase == "warmup" and clock - self.bench_start >= warmup then
        self.bench_phase = "measure"
        self.bench_start = clock
        io.write("WEB_PHASE measure\\n"); io.flush()
    elseif self.bench_phase == "measure" then
        self.bench_frames = self.bench_frames + 1
        if clock - self.bench_start >= duration then
            io.write("WEB_RESULT " .. json.encode({valid=true, measurement_clock_start=self.bench_start, elapsed_seconds=clock-self.bench_start, update_intervals_per_second=self.bench_frames/(clock-self.bench_start), engine=sys.get_engine_info(), count=self.bunnies}) .. "\\n"); io.flush()
            self.bench_phase = "done"
            sys.exit(0)
        end
    end''',1)
p.write_text(s)
# Include bunny entry point in the archive without instantiating it in synthetic scenes.
s=(dst/'game.project').read_text().replace('[project]','[project]\ncustom_resources = /example')
(dst/'game.project').write_text(s)
# Bob only compiles dependency-reachable resources: a collection factory retains the bunny collection.
(dst/'main/bunny.collectionfactory').write_text('prototype: "/example/bunnymark.collection"\nload_dynamically: false\n')
(dst/'main/unused.factory').write_text('prototype: "/main/unused.go"\n')
(dst/'main/unused.go').write_text('components { id: "bunny_resources" component: "/main/bunny.collectionfactory" }\n')
with (dst/'main/benchmark.go').open('a') as f:f.write('\ncomponents { id: "bunny_resources" component: "/main/unused.factory" }\n')
if args.memory_probe:
 shutil.copy(Path(__file__).with_name('memory_probe.lua'), dst/'benchmark/memory_probe.lua')
 for name in ['main/benchmark.script', 'example/bunnymark.script']:
  with (dst/name).open('a') as f:
   f.write('''
local memory_probe = require "benchmark.memory_probe"
local original_init, original_update = init, update
function init(self)
    memory_probe.init(self)
    original_init(self)
end
function update(self, dt)
    memory_probe.update(self)
    original_update(self, dt)
    memory_probe.finish(self)
end
''')
