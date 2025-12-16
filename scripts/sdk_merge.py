import os
import shutil
import tempfile
import zipfile
from dataclasses import dataclass
from concurrent.futures import ThreadPoolExecutor

import s3


DOWNLOAD_WORKERS = 4


@dataclass(frozen=True)
class _SdkZipEntry(object):
    platform: str
    zip_path: str
    filename: str
    file_size: int
    crc: int
    external_attr: int
    is_dir: bool


def _platform_prefixes(platform):
    return (
        f"defoldsdk/lib/{platform}/",
        f"defoldsdk/ext/lib/{platform}/",
        f"defoldsdk/ext/bin/{platform}/",
    )


def _download_platform_sdk_zip(netloc, key, out_path):
    # Create a fresh bucket per thread to avoid shared boto3 state across threads.
    bucket = s3.get_bucket(netloc)
    entry = bucket.Object(key)
    print("Downloading", entry.key)
    entry.download_file(out_path)
    print("Downloaded", entry.key, "to", out_path)
    print("")
    return out_path


def download_platform_sdk_zips(netloc, base_prefix, platforms):
    """
    Downloads per-platform defoldsdk.zip files in parallel.
    Returns an ordered list of (platform, local_zip_path) matching `platforms` order.
    """
    keys = {platform: os.path.join(base_prefix, 'engine', platform, 'defoldsdk.zip') for platform in platforms}
    tmp_paths = {}
    for platform in platforms:
        tmp = tempfile.NamedTemporaryFile(delete=False)
        tmp.close()
        tmp_paths[platform] = tmp.name

    with ThreadPoolExecutor(max_workers=DOWNLOAD_WORKERS) as ex:
        futures = {platform: ex.submit(_download_platform_sdk_zip, netloc, keys[platform], tmp_paths[platform])
                   for platform in platforms}
        # Preserve order for deterministic merge tie-breaking.
        for platform in platforms:
            futures[platform].result()

    return [(platform, tmp_paths[platform]) for platform in platforms]


def merge_platform_sdk_zips_into_tree(platform_zips, extract_dir, strict_merge=False, canonical_platform='x86_64-linux'):
    """
    Merge multiple per-platform SDK zips into `extract_dir` by selecting a single source zip per output path.
    - Includes unique paths from any zip.
    - For platform-specific prefixes, always selects that platform's zip.
    - For identical duplicates (same size+CRC), prefers `canonical_platform` for determinism.
    - For non-identical duplicates, either errors (strict_merge=True) or warns and uses the
      last platform in `platform_zips` order (historical overwrite semantics).
    """
    platform_rank = {p: i for i, (p, _) in enumerate(platform_zips)}
    if not any(p == canonical_platform for p, _ in platform_zips):
        canonical_platform = platform_zips[-1][0]

    candidates_by_path = {}  # filename -> [_SdkZipEntry...]
    for platform, zip_path in platform_zips:
        with zipfile.ZipFile(zip_path, 'r') as zf:
            for info in zf.infolist():
                name = info.filename.replace('\\', '/')
                if not name or name.startswith('/') or '..' in name.split('/'):
                    raise Exception(f"Invalid zip entry path in {platform}: {info.filename}")

                candidates_by_path.setdefault(name, []).append(_SdkZipEntry(
                    platform=platform,
                    zip_path=zip_path,
                    filename=name,
                    file_size=getattr(info, 'file_size', 0),
                    crc=getattr(info, 'CRC', 0),
                    external_attr=getattr(info, 'external_attr', 0),
                    is_dir=bool(getattr(info, 'is_dir', lambda: name.endswith('/'))()),
                ))

    selected_by_path = {}  # filename -> _SdkZipEntry
    conflicts = []         # (filename, [entries], chosen_entry)

    for filename, entries in candidates_by_path.items():
        file_entries = [e for e in entries if not e.is_dir]
        if not file_entries:
            continue

        expected_platform = None
        for p, _ in platform_zips:
            if any(filename.startswith(pref) for pref in _platform_prefixes(p)):
                expected_platform = p
                break

        chosen = None
        if expected_platform is not None:
            for e in file_entries:
                if e.platform == expected_platform:
                    chosen = e
                    break
            if chosen is None:
                chosen = max(file_entries, key=lambda e: platform_rank.get(e.platform, -1))
        else:
            unique_payloads = {(e.file_size, e.crc) for e in file_entries}
            if len(unique_payloads) == 1:
                chosen = next((e for e in file_entries if e.platform == canonical_platform), None)
                if chosen is None:
                    chosen = max(file_entries, key=lambda e: platform_rank.get(e.platform, -1))
            else:
                chosen = max(file_entries, key=lambda e: platform_rank.get(e.platform, -1))
                conflicts.append((filename, file_entries, chosen))

        selected_by_path[filename] = chosen

    if conflicts:
        msg_lines = [
            "Found conflicting SDK paths across platforms (same path, different payload):",
        ]
        for filename, entries, chosen in conflicts[:50]:
            details = ", ".join([f"{e.platform}(size={e.file_size},crc={e.crc:08x})" for e in entries])
            msg_lines.append(f"  - {filename}: {details} -> using {chosen.platform}")
        if len(conflicts) > 50:
            msg_lines.append(f"  ... and {len(conflicts) - 50} more")

        full_msg = "\n".join(msg_lines)
        if strict_merge:
            raise Exception(full_msg + "\nSet DEFOLD_SDK_MERGE_STRICT=0 to preserve previous overwrite behaviour.")
        else:
            print("WARNING:", full_msg)

    extract_map = {}  # zip_path -> [filename...]
    for filename, entry in selected_by_path.items():
        extract_map.setdefault(entry.zip_path, []).append(filename)

    for zip_path, filenames in extract_map.items():
        with zipfile.ZipFile(zip_path, 'r') as zf:
            for filename in sorted(filenames):
                info = zf.getinfo(filename)
                if getattr(info, 'is_dir', lambda: filename.endswith('/'))():
                    continue
                out_path = os.path.join(extract_dir, filename)
                os.makedirs(os.path.dirname(out_path), exist_ok=True)
                with zf.open(filename, 'r') as src, open(out_path, 'wb') as dst:
                    shutil.copyfileobj(src, dst)
                perm = (getattr(info, 'external_attr', 0) >> 16) & 0o7777
                if perm:
                    os.chmod(out_path, perm)

    print("Merged platform SDKs into", extract_dir)


def build_combined_sdk_tree(netloc, base_prefix, platforms, extract_dir, strict_merge=False, canonical_platform='x86_64-linux'):
    """
    High-level helper used by scripts/build.py: downloads per-platform zips, merges them into a single tree,
    and cleans up the downloaded zip files.
    """
    platform_zips = download_platform_sdk_zips(netloc, base_prefix, platforms)
    try:
        merge_platform_sdk_zips_into_tree(platform_zips, extract_dir, strict_merge=strict_merge, canonical_platform=canonical_platform)
    finally:
        for _, zip_path in platform_zips:
            try:
                os.unlink(zip_path)
            except Exception:
                pass

