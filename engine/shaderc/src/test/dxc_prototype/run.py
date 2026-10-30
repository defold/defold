"""SDK-free DXC command-line proof; requires Python 3 and a runnable DXC."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def chunks(path):
    data = path.read_bytes()
    require(data[:4] == b"DXBC", "Expected a DXIL container")
    count = struct.unpack_from("<I", data, 28)[0]
    result = {}
    for index in range(count):
        offset = struct.unpack_from("<I", data, 32 + index * 4)[0]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        result[data[offset:offset + 4].decode("ascii")] = data[offset + 8:offset + 8 + size]
    return result


# Verifies SDK-free SM6 artifacts and catches binding/layout and root-signature regressions.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dxc", required=True, help="DXC executable or wrapper")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--compare", type=Path, help="Other host's report.json")
    parser.add_argument("--reflect", help="Optional native reflection probe executable or wrapper")
    parser.add_argument("--dxcompiler", help="Native DXC library passed to --reflect")
    parser.add_argument("--root-version", choices=("1_0", "1_1"), default="1_0")
    args = parser.parse_args()
    if args.reflect and not args.dxcompiler:
        parser.error("--reflect requires --dxcompiler")
    source = Path(__file__).resolve().parent
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    commands = []

    def run(*options, fail=False):
        command = [args.dxc] + [str(option) for option in options]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                encoding="utf-8", errors="replace")
        commands.append({"command": command, "returncode": result.returncode, "output": result.stdout})
        (out / "commands.json").write_text(json.dumps(commands, indent=2), encoding="utf-8")
        require((result.returncode != 0) if fail else (result.returncode == 0),
                "Unexpected DXC result:\n" + " ".join(command) + "\n" + result.stdout)
        return result.stdout

    version = run("--version").strip()
    root = out / "merged.rootsig"
    bad = out / "incompatible.rootsig"
    for name, path in [("MERGED", root), ("INCOMPATIBLE", bad)]:
        run("-T", "rootsig_" + args.root_version, "-E", name, source / "roots.hlsl", "-Fo", path)
        require("RTS0" in chunks(path), "Missing serialized root signature")

    expected = {
        "vs": {"Params": ("cb0,space1", 1)},
        "ps": {"Params": ("cb0,space1", 1), "textures": ("t0,space2", 2),
               "texture_sampler": ("s0,space2", 1)},
        "cs": {"Params": ("cb0,space1", 1), "textures": ("t0,space2", 2),
               "output_buffer": ("u0,space3", 1)},
    }
    report = {}
    for stage in ("vs", "ps", "cs"):
        shader = out / (stage + ".dxil")
        reflection = out / (stage + ".reflection")
        run("-T", stage + "_6_0", "-E", stage + "_main", source / "shaders.hlsl",
            "-Fo", shader, "-Fre", reflection)
        require("DXIL" in chunks(shader), "Expected SM6 DXIL, not SM5 bytecode")
        require(reflection.stat().st_size > 0, "Missing separate reflection data")
        if args.reflect:
            subprocess.run([args.reflect, args.dxcompiler, str(reflection)], check=True)
        dump = run("-dumpbin", shader)
        (out / (stage + ".txt")).write_text(dump, encoding="utf-8")
        bindings = {}
        table = dump.split("; Resource Bindings:")[1].split("\n\n")[0]
        for line in table.splitlines():
            fields = line.lstrip(";").split()
            if len(fields) == 7 and fields[-1].isdigit():
                bindings[fields[0]] = (fields[-2], int(fields[-1]))
        require(bindings == expected[stage], "Unexpected bindings: " + repr(bindings))
        require(re.search(r"float4 tint;.*Offset:\s+0", dump), "Incorrect tint offset")
        require(re.search(r"float4x4 transform;.*Offset:\s+16", dump), "Incorrect matrix offset")
        require(re.search(r"Params;.*Size:\s+80", dump), "Incorrect buffer size")
        run("-dumpbin", shader, "-verifyrootsignature", root)
        failure = run("-dumpbin", shader, "-verifyrootsignature", bad, fail=True)
        require("root signature" in failure.lower(), "Negative case failed for an unrelated reason")
        attached = out / (stage + ".attached.dxil")
        run("-dumpbin", shader, "-setrootsignature", root, "-Fo", attached)
        extracted = out / (stage + ".extracted.rootsig")
        run("-dumpbin", attached, "-extractrootsignature", "-Fo", extracted)
        require(chunks(extracted)["RTS0"] == chunks(root)["RTS0"], "Root signature round-trip changed")
        run("-dumpbin", attached, "-verifyrootsignature", root)
        stripped = out / (stage + ".stripped.dxil")
        run("-dumpbin", attached, "-Qstrip_reflect", "-Fo", stripped)
        require("DXIL" in chunks(stripped) and "RTS0" in chunks(stripped),
                "Stripping reflection removed runtime data")
        run("-dumpbin", stripped, "-verifyrootsignature", root)
        report[stage] = {"bindings": bindings, "buffer_size": 80,
                         "member_offsets": {"tint": 0, "transform": 16},
                         "root_sha256": hashlib.sha256(chunks(extracted)["RTS0"]).hexdigest()}
    # Normalize tuples before comparing JSON written on another host.
    report = json.loads(json.dumps(report))
    if args.compare:
        other = json.loads(args.compare.read_text(encoding="utf-8"))
        require(report == other["results"], "Cross-host semantic results differ")
    (out / "report.json").write_text(json.dumps({"dxc_version": version, "results": report}, indent=2),
                                   encoding="utf-8")
    print("PASS: SM6 compilation, reflection layout, root signature serialization/attachment/"
          "extraction/validation, incompatible signature rejection, reflection stripping.")
    if args.compare:
        print("PASS: cross-host reflection and serialized root signatures match.")


if __name__ == "__main__":
    main()
