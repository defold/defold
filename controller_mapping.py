#!/usr/bin/env python3
"""Convert a single SDL-style GLFW mapping entry into Defold .gamepads format."""

from pathlib import Path

SDL_MAPPING = (
    "050000005e040000e002000030110000,8BitDo Zero 2 (XInput),"
    "a:b0,b:b1,back:b6,dpdown:+a1,dpleft:-a0,dpright:+a0,dpup:-a1,"
    "leftshoulder:b4,rightshoulder:b5,start:b7,x:b2,y:b3,platform:Linux,"
)

BUTTON_MAP = {
    # Match Defold's logical face-button orientation (A = south, B = east, etc.)
    "a": "GAMEPAD_RPAD_DOWN",
    "b": "GAMEPAD_RPAD_RIGHT",
    "x": "GAMEPAD_RPAD_LEFT",
    "y": "GAMEPAD_RPAD_UP",
    "back": "GAMEPAD_BACK",
    "start": "GAMEPAD_START",
    "guide": "GAMEPAD_GUIDE",
    "leftshoulder": "GAMEPAD_LSHOULDER",
    "rightshoulder": "GAMEPAD_RSHOULDER",
    "leftstick": "GAMEPAD_LSTICK_CLICK",
    "rightstick": "GAMEPAD_RSTICK_CLICK",
    "dpup": "GAMEPAD_LPAD_UP",
    "dpdown": "GAMEPAD_LPAD_DOWN",
    "dpleft": "GAMEPAD_LPAD_LEFT",
    "dpright": "GAMEPAD_LPAD_RIGHT",
}

AXIS_MAP = {
    "leftx": ("GAMEPAD_LSTICK_LEFT", "GAMEPAD_LSTICK_RIGHT"),
    "lefty": ("GAMEPAD_LSTICK_UP", "GAMEPAD_LSTICK_DOWN"),
    "rightx": ("GAMEPAD_RSTICK_LEFT", "GAMEPAD_RSTICK_RIGHT"),
    "righty": ("GAMEPAD_RSTICK_UP", "GAMEPAD_RSTICK_DOWN"),
    "lefttrigger": "GAMEPAD_LTRIGGER",
    "righttrigger": "GAMEPAD_RTRIGGER",
}

HAT_DIRECTION = {
    "leftx": {8: "-",
              2: "+"},
    "rightx": {8: "-",
               2: "+"},
    "lefty": {1: "-",
              4: "+"},
    "righty": {1: "-",
               4: "+"},
}

# SDL encodes hats as h<index>.<bit> where bit is already the mask value.
HAT_MASK = {0: 0, 1: 1, 2: 2, 4: 4, 8: 8}

PLATFORM_ALIASES = {
    "mac os x": "osx",
}


def normalize_platform(platform):
    slug = platform.strip().lower()
    slug = PLATFORM_ALIASES.get(slug, slug)
    return slug.replace(" ", "")


def parse_input_descriptor(raw_value):
    if not raw_value:
        raise ValueError("Missing input descriptor")
    invert = False
    if raw_value.endswith("~"):
        invert = True
        raw_value = raw_value[:-1]
    sign = None
    if raw_value and raw_value[0] in "+-":
        sign = raw_value[0]
        raw_value = raw_value[1:]
    if not raw_value:
        raise ValueError("Incomplete input descriptor")
    kind = raw_value[0]
    payload = raw_value[1:]
    descriptor = {"invert": invert, "sign": sign}
    if kind == "h":
        index_str, mask_str = payload.split(".")
        descriptor.update({"kind": "hat", "index": int(index_str), "hat_mask": int(mask_str)})
        return descriptor
    if kind == "b":
        descriptor.update({"kind": "button", "index": int(payload)})
        return descriptor
    if kind == "a":
        descriptor.update({"kind": "axis", "index": int(payload)})
        return descriptor
    raise ValueError(f"Unsupported descriptor '{raw_value}'")


def axis_modifiers(sign=None, *, invert=False, clamp=True, scale=False):
    effective_sign = sign if sign in ("+", "-") else "+"
    if invert:
        effective_sign = "-" if effective_sign == "+" else "+"
    mods = []
    if effective_sign == "-":
        mods.append("GAMEPAD_MODIFIER_NEGATE")
    if clamp:
        mods.append("GAMEPAD_MODIFIER_CLAMP")
    if scale:
        mods.append("GAMEPAD_MODIFIER_SCALE")
    return mods


def format_map(input_name, type_name, index, *, mods=None, hat_mask=None):
    mods = mods or []
    lines = [f"    map {{ input: {input_name} type: {type_name} index: {index}"]
    for mod in mods:
        lines.append(f"        mod {{ mod: {mod} }}")
    if hat_mask is not None:
        lines.append(f"        hat_mask: {hat_mask}")
    lines.append("    }")
    return "\n".join(lines)


def convert(mapping_line):
    tokens = [token for token in mapping_line.strip().split(",") if token]
    guid, device = tokens[:2]
    fields = dict(entry.split(":", 1) for entry in tokens[2:])
    platform = fields.pop("platform", "")

    normalized_platform = normalize_platform(platform)
    output = ["driver", "{", f"    device: \"{device}\"",
              f"    guid: \"{guid}\"",
              f"    platform: \"{normalized_platform}\"",
              "    dead_zone: 0.2"]
    unhandled = []

    for raw_key, value in fields.items():
        base_key = raw_key
        direction_hint = None
        if base_key and base_key[0] in "+-":
            direction_hint = base_key[0]
            base_key = base_key[1:]
        descriptor = parse_input_descriptor(value)
        handled = False

        if base_key in BUTTON_MAP:
            logical_input = BUTTON_MAP[base_key]
            handled = True
            if descriptor["kind"] == "button":
                output.append(format_map(logical_input, "GAMEPAD_TYPE_BUTTON", descriptor["index"]))
            elif descriptor["kind"] == "hat":
                mask = HAT_MASK.get(descriptor["hat_mask"], descriptor["hat_mask"])
                output.append(format_map(logical_input, "GAMEPAD_TYPE_HAT", descriptor["index"], hat_mask=mask))
            elif descriptor["kind"] == "axis":
                mods = axis_modifiers(descriptor["sign"], invert=descriptor["invert"])
                output.append(format_map(logical_input, "GAMEPAD_TYPE_AXIS", descriptor["index"], mods=mods))
        elif base_key in AXIS_MAP and isinstance(AXIS_MAP[base_key], tuple):
            neg_name, pos_name = AXIS_MAP[base_key]
            handled = True

            def emit_axis_entry(target_name, descriptor_kind, target_direction):
                if descriptor_kind == "axis":
                    mods = axis_modifiers(target_direction, invert=descriptor["invert"])
                    output.append(format_map(target_name, "GAMEPAD_TYPE_AXIS", descriptor["index"], mods=mods))
                elif descriptor_kind == "hat":
                    mask = HAT_MASK.get(descriptor["hat_mask"], descriptor["hat_mask"])
                    output.append(format_map(target_name, "GAMEPAD_TYPE_HAT", descriptor["index"], hat_mask=mask))
                elif descriptor_kind == "button":
                    output.append(format_map(target_name, "GAMEPAD_TYPE_BUTTON", descriptor["index"]))
                else:
                    return False
                return True

            if descriptor["kind"] == "axis":
                if direction_hint:
                    target_name = neg_name if direction_hint == "-" else pos_name
                    emit_axis_entry(target_name, descriptor["kind"], direction_hint)
                else:
                    for sign, target_name in (("-", neg_name), ("+", pos_name)):
                        emit_axis_entry(target_name, descriptor["kind"], sign)
            elif descriptor["kind"] in ("button", "hat"):
                target_direction = direction_hint
                if not target_direction:
                    mask_map = HAT_DIRECTION.get(base_key, {})
                    if descriptor["kind"] == "hat":
                        target_direction = mask_map.get(descriptor["hat_mask"])
                if target_direction:
                    target_name = neg_name if target_direction == "-" else pos_name
                    emit_axis_entry(target_name, descriptor["kind"], target_direction)
                else:
                    handled = False
            else:
                handled = False
        elif base_key in AXIS_MAP:
            logical_input = AXIS_MAP[base_key]
            handled = True
            if descriptor["kind"] == "axis":
                sign = "-" if descriptor["invert"] else "+"
                output.append(format_map(logical_input, "GAMEPAD_TYPE_AXIS", descriptor["index"],
                                         mods=axis_modifiers(sign, invert=False, clamp=True, scale=True)))
            elif descriptor["kind"] == "button":
                output.append(format_map(logical_input, "GAMEPAD_TYPE_BUTTON", descriptor["index"]))
            else:
                handled = False

        if not handled:
            unhandled.append(raw_key)

    output.append("}")
    return "\n".join(output), unhandled


def iter_glfw_entries(path):
    import ast
    for raw_line in Path(path).read_text().splitlines():
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("//") or not stripped.startswith("\""):
            continue
        literal = stripped.rstrip(",")
        try:
            yield ast.literal_eval(literal)
        except SyntaxError as exc:
            raise ValueError(f"Failed to parse mapping line: {raw_line}") from exc


def iter_sdl_entries(path):
    for raw_line in Path(path).read_text().splitlines():
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        yield stripped


def process_file(input_path, output_path, source_format="glfw"):
    if source_format == "sdl":
        entries = iter_sdl_entries(input_path)
    else:
        entries = iter_glfw_entries(input_path)

    all_unhandled = {}
    outputs = []
    for entry in entries:
        converted, unhandled = convert(entry)
        outputs.append(converted)
        for token in unhandled:
            all_unhandled[token] = all_unhandled.get(token, 0) + 1
    Path(output_path).write_text("\n\n".join(outputs) + ("\n" if outputs else ""))
    return all_unhandled


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Convert SDL/GLFW mapping tables into Defold .gamepads format.")
    parser.add_argument("input", nargs="?", help="Path to mappings.h (or similar) file to convert.")
    parser.add_argument("output", nargs="?", help="Destination .gamepads file.")
    parser.add_argument("--format", choices=("glfw", "sdl"), default="glfw", help="Input mapping format (default: glfw).")
    args = parser.parse_args()

    if args.input and args.output:
        stats = process_file(args.input, args.output, args.format)
        if stats:
            print("Unhandled tokens detected:")
            for token, count in sorted(stats.items(), key=lambda item: item[1], reverse=True):
                print(f"  {token}: {count}")
    else:
        converted, missing = convert(SDL_MAPPING)
        print(converted)
        if missing:
            print("Unhandled tokens:", ", ".join(missing))
