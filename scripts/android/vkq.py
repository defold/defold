#!/usr/bin/env python3
"""Convert between VkQuality 1.2.0 files and list-editor JSON (no dependencies)."""

import argparse
import json
from pathlib import Path
import string
import struct
import sys


# Layouts from vkq_library/vkquality/src/main/cpp/vkquality_file_format.h.
# Files exported by the list editor use little-endian 32-bit integers.
HEADER = struct.Struct("<4Ii17I")
HEADER_FIELDS = (
    "file_identifier",
    "file_format_version",
    "library_minimum_version",
    "list_version",
    "min_future_vulkan_recommendation_api",
    "device_list_count",
    "driver_allow_count",
    "driver_deny_count",
    "gpu_allow_predict_count",
    "gpu_deny_predict_count",
    "soc_allow_count",
    "soc_deny_count",
    "string_table_count",
    "device_list_offset",
    "device_list_shortcuts_offset",
    "driver_allow_offset",
    "driver_deny_offset",
    "gpu_allow_predict_offset",
    "gpu_deny_predict_offset",
    "soc_allow_offset",
    "soc_deny_offset",
    "string_table_offset",
)
DEVICE_FIELDS = (
    "brand_string_index", "device_string_index", "min_api_version",
    "min_driver_version",
)
GPU_FIELDS = (
    "device_name_string_index", "min_api_version", "device_id", "vendor_id",
    "min_driver_version",
)
SOC_FIELDS = (
    "soc_fingerprint_count", "soc_fingerprint_offset", "soc_string_index",
)
TABLE_FIELDS = {
    "device_list": DEVICE_FIELDS,
    "driver_allow": ("driver_version_string_index",),
    "driver_deny": ("driver_version_string_index",),
    "gpu_allow_predict": GPU_FIELDS,
    "gpu_deny_predict": GPU_FIELDS,
    "soc_allow": SOC_FIELDS,
    "soc_deny": SOC_FIELDS,
}


def version_string(value):
    """Decode the format's 0xMMmmpp version numbers."""
    return "{}.{}.{}".format(value >> 16, (value >> 8) & 255, value & 255)


def encode_vkq(project):
    """Encode editor schema 1 as a little-endian VkQuality 1.2.0 file."""
    if not isinstance(project, dict):
        raise ValueError("JSON project must be an object")

    def integer(record, field, context, minimum=0, maximum=0xffffffff):
        value = record.get(field)
        if type(value) is not int or not minimum <= value <= maximum:
            raise ValueError("{}.{} must be an integer between {} and {}".format(
                context, field, minimum, maximum
            ))
        return value

    def text_value(record, field, context):
        if field not in record:
            raise ValueError("{}.{} is required".format(context, field))
        value = record[field]
        if value is None:
            return ""
        if not isinstance(value, str) or "\0" in value:
            raise ValueError("{}.{} must be a string without null characters".format(
                context, field
            ))
        try:
            value.encode("utf-8")
        except UnicodeEncodeError as error:
            raise ValueError("{}.{} is not valid UTF-8".format(context, field)) from error
        # The editor treats null, empty and "none" as the index-zero string.
        return "" if value.casefold() == "none" else value

    def records(name):
        values = project.get(name)
        if values is None:
            return
        if not isinstance(values, list):
            raise ValueError("{} must be an array or null".format(name))
        for index, record in enumerate(values):
            context = "{}[{}]".format(name, index)
            if not isinstance(record, dict):
                raise ValueError("{} must be an object".format(context))
            yield record, context

    schema = integer(project, "ProjectSchemaVersion", "project")
    if schema != 1:
        raise ValueError("unsupported ProjectSchemaVersion {} (expected 1)".format(schema))
    header = dict.fromkeys(HEADER_FIELDS, 0)
    header.update(
        file_identifier=0x564B5141,
        file_format_version=0x010200,
        library_minimum_version=0x010200,
        list_version=integer(project, "ExportedListFileVersion", "project"),
        min_future_vulkan_recommendation_api=integer(
            project, "MinApiForFutureRecommendation", "project", -0x80000000, 0x7fffffff
        ),
    )
    devices = [
        (text_value(row, "brand", context), text_value(row, "device", context),
         integer(row, "minapi", context), integer(row, "driverversion", context))
        for row, context in records("DeviceAllowList")
    ]
    gpus = {}
    drivers = {}
    for kind in ("allow", "deny"):
        gpus[kind] = [
            (text_value(row, "devicename", context), integer(row, "minapi", context),
             integer(row, "deviceid", context), integer(row, "vendorid", context),
             integer(row, "driverversion", context))
            for row, context in records("GpuPredict" + kind.title() + "List")
        ]
        drivers[kind] = [
            (text_value(row, "soc", context), text_value(row, "driverfingerprint", context))
            for row, context in records("Driver" + kind.title() + "List")
        ]

    def sort_key(value):
        # DeviceStringSorter orders letters, digits, then other characters.
        first = value[:1].lower()
        group = 0 if "a" <= first <= "z" else 1 if "0" <= first <= "9" else 2
        return group, value.casefold()

    # As in the editor, shared strings are case-insensitive; first casing wins.
    unique_strings = {}
    for rows, fields in (
        (devices, (0, 1)), (gpus["allow"], (0,)), (gpus["deny"], (0,)),
        (drivers["allow"], (0, 1)), (drivers["deny"], (0, 1)),
    ):
        for row in rows:
            for field in fields:
                value = row[field]
                if value:
                    unique_strings.setdefault(value.casefold(), value)
    strings = [""] + sorted(unique_strings.values(), key=sort_key)
    string_ids = {value.casefold(): index for index, value in enumerate(strings)}

    def string_id(value):
        return string_ids[value.casefold()]

    header["string_table_offset"] = HEADER.size
    header["string_table_count"] = len(strings)
    data = bytearray(HEADER.size + len(strings) * 4)
    for index, value in enumerate(strings):
        struct.pack_into("<I", data, HEADER.size + index * 4, len(data))
        data.extend(value.encode("utf-8") + b"\0")

    def append_table(name, rows):
        header[name + "_count"] = len(rows)
        header[name + "_offset"] = len(data) if rows else 0
        record = struct.Struct("<" + "I" * len(TABLE_FIELDS[name]))
        for row in rows:
            data.extend(record.pack(*row))

    append_table("device_list", [
        (string_id(brand), string_id(device), min_api, driver)
        for brand, device, min_api, driver in sorted(
            devices, key=lambda row: sort_key(row[0] + "," + row[1])
        )
    ])
    # Zero shortcuts are valid and match the editor's unoptimized table.
    header["device_list_shortcuts_offset"] = len(data)
    data.extend(bytes(27 * 4))

    for kind in ("allow", "deny"):
        append_table("gpu_" + kind + "_predict", [
            (string_id(name), min_api, device_id, vendor_id, driver)
            for name, min_api, device_id, vendor_id, driver in sorted(
                gpus[kind], key=lambda row: sort_key(row[0])
            )
        ])
    for kind in ("allow", "deny"):
        by_soc = {}
        for soc, fingerprint in drivers[kind]:
            by_soc.setdefault(string_id(soc), set()).add(string_id(fingerprint))
        soc_rows = []
        fingerprint_rows = []
        for soc_id in sorted(by_soc, key=lambda index: strings[index].casefold()):
            fingerprints = sorted(by_soc[soc_id], key=lambda index: strings[index].casefold())
            soc_rows.append((len(fingerprints), len(fingerprint_rows), soc_id))
            fingerprint_rows.extend((index,) for index in fingerprints)
        append_table("soc_" + kind, soc_rows)
        append_table("driver_" + kind, fingerprint_rows)

    # VkQualityPredictionFile::ValidateFile rejects files larger than 1 MiB.
    if len(data) > 1024 * 1024:
        raise ValueError("generated file exceeds VkQuality's 1 MiB size limit")
    HEADER.pack_into(data, 0, *(header[field] for field in HEADER_FIELDS))
    return bytes(data)


def parse_vkq(data):
    """Decode file bytes into editor schema 1; reject invalid or unsupported data.

    GPU brand labels are not stored in .vkq files and are emitted as empty
    strings. Records retain the ordering and string casing found in the binary.
    """
    if len(data) < HEADER.size:
        raise ValueError("file is too short for the 88-byte VkQuality header")
    header = dict(zip(HEADER_FIELDS, HEADER.unpack_from(data)))
    if header["file_identifier"] != 0x564B5141:
        raise ValueError("invalid VkQuality file identifier (expected 0x564b5141)")
    if header["file_format_version"] != 0x010200:
        raise ValueError(
            "unsupported file format version {} (expected 1.2.0)".format(
                version_string(header["file_format_version"])
            )
        )

    def read_table(name, offset, count, width):
        record = struct.Struct("<" + "I" * width)
        if offset > len(data) or count * record.size > len(data) - offset:
            raise ValueError("{} extends beyond the end of the file".format(name))
        if count and offset < HEADER.size:
            raise ValueError("{} overlaps the file header".format(name))
        return record.iter_unpack(memoryview(data)[offset:offset + count * record.size])

    strings = []
    for index, (offset,) in enumerate(read_table(
        "string_table", header["string_table_offset"], header["string_table_count"], 1
    )):
        if offset < HEADER.size or offset >= len(data):
            raise ValueError("string {} has an invalid byte offset {}".format(index, offset))
        end = data.find(b"\0", offset)
        if end == -1:
            raise ValueError("string {} has no null terminator".format(index))
        try:
            strings.append(data[offset:end].decode("utf-8"))
        except UnicodeDecodeError as error:
            raise ValueError("string {} is not valid UTF-8".format(index)) from error

    result = {}
    for name, fields in TABLE_FIELDS.items():
        entries = []
        for index, values in enumerate(read_table(
            name, header[name + "_offset"], header[name + "_count"], len(fields)
        )):
            entry = dict(zip(fields, values))
            for field, value in zip(fields, values):
                if field.endswith("_string_index"):
                    if value >= len(strings):
                        raise ValueError("{}[{}].{} references invalid string {}".format(
                            name, index, field, value
                        ))
                    entry[field[:-len("_string_index")]] = strings[value]
            entries.append(entry)
        result[name] = entries

    # SoC offsets are indices into the matching fingerprint table, not bytes.
    for kind in ("allow", "deny"):
        fingerprints = result["driver_" + kind]
        for index, entry in enumerate(result["soc_" + kind]):
            start = entry["soc_fingerprint_offset"]
            count = entry["soc_fingerprint_count"]
            if start > len(fingerprints) or count > len(fingerprints) - start:
                raise ValueError("soc_{}[{}] references invalid fingerprint range".format(
                    kind, index
                ))
            entry["fingerprints"] = [
                item["driver_version"] for item in fingerprints[start:start + count]
            ]

    labels = list(string.ascii_uppercase) + ["other"]
    for label, (index,) in zip(labels, read_table(
        "device_list_shortcuts", header["device_list_shortcuts_offset"], 27, 1
    )):
        if index > header["device_list_count"]:
            raise ValueError("device shortcut {} references invalid index {}".format(label, index))

    # Match DeviceListProject and its records' JsonPropertyName attributes.
    project = {
        "ProjectSchemaVersion": 1,
        "ExportedListFileVersion": header["list_version"],
        "MinApiForFutureRecommendation": header["min_future_vulkan_recommendation_api"],
        "DeviceAllowList": [
            {
                "brand": entry["brand"],
                "device": entry["device"],
                "minapi": entry["min_api_version"],
                "driverversion": entry["min_driver_version"],
            }
            for entry in result["device_list"]
        ],
    }
    for kind in ("allow", "deny"):
        project["Driver" + kind.title() + "List"] = [
            {"soc": entry["soc"], "driverfingerprint": fingerprint}
            for entry in result["soc_" + kind]
            for fingerprint in entry["fingerprints"]
        ]
    for kind in ("allow", "deny"):
        project["GpuPredict" + kind.title() + "List"] = [
            {
                "brand": "",  # Editor metadata omitted by the binary exporter.
                "devicename": entry["device_name"],
                "deviceid": entry["device_id"],
                "vendorid": entry["vendor_id"],
                "minapi": entry["min_api_version"],
                "driverversion": entry["min_driver_version"],
            }
            for entry in result["gpu_" + kind + "_predict"]
        ]
    return project


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog="Defaults to the opposite extension beside the input. Existing output files are replaced.",
    )
    parser.add_argument("file", type=Path, help="input .vkq or .json file (extension selects conversion)")
    parser.add_argument("output", type=Path, nargs="?", help="optional output path and filename")
    args = parser.parse_args()
    try:
        extension = args.file.suffix.lower()
        if extension not in (".vkq", ".json"):
            raise ValueError("input file must have a .vkq or .json extension")
        output = args.output if args.output is not None else args.file.with_suffix(
            ".json" if extension == ".vkq" else ".vkq"
        )
        if args.file.resolve() == output.resolve():
            raise ValueError("input and output must be different files")
        if extension == ".json":
            project = json.loads(args.file.read_text(encoding="utf-8-sig"))
            data = encode_vkq(project)
            output.write_bytes(data)
        else:
            contents = parse_vkq(args.file.read_bytes())
            output.write_text(json.dumps(contents, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError) as error:
        print("{}: error: {}".format(parser.prog, error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
