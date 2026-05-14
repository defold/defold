#!/usr/bin/env sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

export SCRIPT_DIR

python3 <<'PY'
import gzip
import io
import os
import pathlib
import zipfile
import zlib

script_dir = pathlib.Path(os.environ["SCRIPT_DIR"])

payloads = {
    "alpha": b"Alpha\nBeta\nGamma\n",
    "empty": b"",
    "binary_256": bytes(range(256)),
}

zip_entries = {
    "dir/": None,
    "hello.txt": b"Hello Zip\n",
    "dir/data.bin": bytes(range(8)),
    "empty.txt": b"",
}


def write(path: pathlib.Path, data: bytes) -> None:
    path.write_bytes(data)


def gzip_bytes(data: bytes) -> bytes:
    out = io.BytesIO()
    with gzip.GzipFile(filename="", mode="wb", fileobj=out, mtime=0) as gz:
        gz.write(data)
    return out.getvalue()


def zip_info(name: str, is_dir: bool) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name)
    info.date_time = (2020, 1, 1, 0, 0, 0)
    info.create_system = 3
    if is_dir:
        info.external_attr = (0o40755 << 16) | 0x10
        info.compress_type = zipfile.ZIP_STORED
    else:
        info.external_attr = 0o100644 << 16
    return info


for name, data in payloads.items():
    write(script_dir / f"{name}.deflate", zlib.compress(data, level=6))
    write(script_dir / f"{name}.gz", gzip_bytes(data))


for archive_name, default_compression in (
    ("archive_deflated.zip", zipfile.ZIP_DEFLATED),
    ("archive_stored.zip", zipfile.ZIP_STORED),
):
    archive_path = script_dir / archive_name
    with zipfile.ZipFile(archive_path, "w") as zf:
        for entry_name, data in zip_entries.items():
            is_dir = data is None
            info = zip_info(entry_name, is_dir)
            if not is_dir:
                info.compress_type = default_compression
            zf.writestr(info, b"" if data is None else data)
PY
