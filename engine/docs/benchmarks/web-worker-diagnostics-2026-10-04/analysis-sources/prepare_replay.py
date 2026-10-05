"""Adapt an existing replay-instrumented project into a disposable web evaluation copy."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Replay SDK differs; review adaptation: ' + old)
    return text.replace(old, new, 1)


def prepare(source, destination, manifest_path, long_melon=False):
    source, destination = source.resolve(), destination.resolve()
    if destination == source or source in destination.parents:
        raise ValueError('Destination must be outside the source project')
    if destination.exists():
        raise ValueError('Use a new destination; source and previous evidence are preserved')
    if not (source / 'game.project').is_file():
        raise ValueError('Source must contain game.project')
    manifest = json.loads(manifest_path.read_text())
    if long_melon and manifest['expected']['id'] != 'underwatermelon-offline-v1':
        raise ValueError('--long-melon requires the reviewed offline Underwatermelon adapter')
    runtime = source / 'replay/runtime.lua'
    text = runtime.read_text()
    # Logging is outside the measured interval. The complete result retains all
    # gameplay checkpoints and cleanup evidence, not just an update counter.
    text = replace_once(text, '    session.phase = value',
                        '    session.phase = value\n    io.write("WEB_PHASE " .. value .. "\\n"); io.flush()')
    text = replace_once(text, '            phase(session, "done")',
                        '            phase(session, "done")\n            io.write("WEB_RESULT " .. json.encode(session.result) .. "\\n"); io.flush()')
    text = replace_once(text, '    assert(M.enabled(),',
                        '    sys.set_error_handler(function(_, message, traceback) io.write("WEB_ERROR " .. message .. " " .. traceback .. "\\n"); io.flush(); sys.exit(1) end)\n    assert(M.enabled(),')
    # Diagnostics are explicitly marked and excluded by the runner/report contracts.
    text = replace_once(text, '    assert(get("render.sprite_trace", "__unset__") == "__unset__", "Disable tracing for replay comparisons")',
                        '    assert(get("render.sprite_trace", "__unset__") == "__unset__" or get("render.poc_web_diagnostics", "0") == "1", "Tracing requires explicit diagnostic mode")')
    text = replace_once(text, '                diagnostic_pixels =',
                        '                diagnostic_timing = get("render.poc_web_diagnostics", "0") == "1",\n                diagnostic_pixels =')
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns('build', '.git', '.internal', '.DS_Store'))
    (destination / 'replay/runtime.lua').write_text(text)
    if long_melon:
        # Keep the game active by spreading twelve drops over 250 simulated
        # seconds. Movement, physics, merging, GUI, sound and cleanup are real.
        total, warmup, period = 15000, 600, 1200
        events = []
        for tick in range(1, total + 1):
            if tick % 240 < 75:
                events.append({'tick': tick, 'action': 'left'})
            elif 120 <= tick % 240 < 195:
                events.append({'tick': tick, 'action': 'right'})
            if tick % period == 0:
                events.append({'tick': tick, 'action': 'fire', 'pressed': True})
        scenario = '''local events = {}
for tick = 1, 15000 do
    if tick % 240 < 75 then events[#events+1] = {tick=tick, action="left"}
    elseif tick % 240 >= 120 and tick % 240 < 195 then events[#events+1] = {tick=tick, action="right"} end
    if tick % 1200 == 0 then events[#events+1] = {tick=tick, action="fire", pressed=true} end
end
return {version=1, id="underwatermelon-offline-web-long-v1", seed=42, tick_hz=60,
    warmup_ticks=600, measure_ticks=14400, checkpoint_interval=600, events=events}
'''
        (destination / 'replay/game_scenario.lua').write_text(scenario)
        manifest['expected'] = {'id': 'underwatermelon-offline-web-long-v1', 'ticks': total,
                                'measure_ticks': total-warmup, 'events': len(events)}
    case = {'name': 'gameplay_long' if long_melon else 'gameplay', 'scene': 'replay',
            'expected': manifest['expected'], 'config': manifest.get('config', {}), 'timeout_seconds': 1200}
    (destination / 'web-cases.json').write_text(json.dumps([case], indent=2) + '\n')
    provenance = {'source': str(source.resolve()), 'manifest': manifest,
                  'files': {str(p.relative_to(destination)): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in sorted(destination.rglob('*')) if p.is_file()}}
    (destination / 'web-source-manifest.json').write_text(json.dumps(provenance, indent=2) + '\n')
    return case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--long-melon', action='store_true')
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.destination, args.manifest, args.long_melon), indent=2))


if __name__ == '__main__':
    main()
