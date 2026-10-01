import os
import shutil
import tempfile
import zipfile
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor

import run
import s3


DOWNLOAD_WORKERS = 8


def _download_platform_sdk_zip(netloc, key, out_path):
    # Create a fresh bucket per thread to avoid shared boto3 state across threads.
    bucket = s3.get_bucket(netloc)
    entry = bucket.Object(key)
    print("Downloading", entry.key)
    entry.download_file(out_path)
    print("Downloaded", entry.key, "to", out_path)
    print("")
    return out_path


@contextmanager
def download_platform_sdk_zips(netloc, base_prefix, platforms):
    """
    Downloads per-platform defoldsdk.zip files in parallel.
    Yields an ordered list of (platform, local_zip_path) matching `platforms` order,
    and removes the temporary files afterwards.
    """
    keys = {platform: os.path.join(base_prefix, 'engine', platform, 'defoldsdk.zip') for platform in platforms}
    tmp_paths = {}
    try:
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

        yield [(platform, tmp_paths[platform]) for platform in platforms]
    finally:
        for zip_path in tmp_paths.values():
            try:
                os.unlink(zip_path)
            except Exception:
                pass


def _ordered_platform_sdk_zips(platform_zips, canonical_platform):
    """Order SDKs from highest to lowest precedence."""
    platform_to_zip = {p: z for p, z in platform_zips}
    if canonical_platform not in platform_to_zip:
        canonical_platform = platform_zips[0][0]

    ordered_platform_zips = [(canonical_platform, platform_to_zip[canonical_platform])]
    ordered_platform_zips.extend([(p, z) for p, z in platform_zips if p != canonical_platform])
    return ordered_platform_zips


def _merge_extender_configs(ordered_platform_zips):
    # build.py imports this module before install_release_dependencies runs.
    import yaml

    configs = {}
    contents = {}
    changed = set()
    for platform, zip_path in ordered_platform_zips:
        with zipfile.ZipFile(zip_path, 'r') as zf:
            for name in zf.namelist():
                if name != 'defoldsdk/extender/build.yml' and not (
                        name.startswith('defoldsdk/extender/variants/') and name.endswith('.appmanifest')):
                    continue
                data = zf.read(name)
                try:
                    config = yaml.safe_load(data)
                except yaml.YAMLError as error:
                    raise ValueError(f'Invalid extender config in {platform}: {name}: {error}') from error
                if not isinstance(config, dict) or not isinstance(config.get('platforms'), dict):
                    raise ValueError(f'Expected a platforms mapping in {platform}: {name}')
                if name not in configs:
                    configs[name] = config
                    contents[name] = data
                    continue
                # Keep common settings and existing platform sections from the
                # highest-precedence SDK. Do not concatenate flags or libraries.
                platforms = configs[name]['platforms']
                for target, settings in config['platforms'].items():
                    if target not in platforms:
                        platforms[target] = settings
                        changed.add(name)

    for name in changed:
        contents[name] = yaml.safe_dump(configs[name], sort_keys=False).encode('utf-8')
    return contents


def merge_platform_sdk_zips_into_tree(platform_zips, extract_dir, canonical_platform='x86_64-linux'):
    """
    Merge multiple per-platform SDK zips into `extract_dir`.
    Extender configurations combine platform sections; other files use the first
    available copy, with `canonical_platform` taking precedence.
    """
    ordered_platform_zips = _ordered_platform_sdk_zips(platform_zips, canonical_platform)
    extender_configs = _merge_extender_configs(ordered_platform_zips)

    selected_by_path = {}  # filename -> (zip_path, platform)
    for platform, zip_path in ordered_platform_zips:
        with zipfile.ZipFile(zip_path, 'r') as zf:
            for info in zf.infolist():
                name = info.filename.replace('\\', '/')
                if not name or name.startswith('/') or '..' in name.split('/'):
                    raise Exception(f"Invalid zip entry path in {platform}: {info.filename}")
                if bool(getattr(info, 'is_dir', lambda: name.endswith('/'))()):
                    continue

                if name not in selected_by_path:
                    selected_by_path[name] = (zip_path, platform)

    extract_map = {}  # zip_path -> [filename...]
    for filename, (zip_path, _) in selected_by_path.items():
        extract_map.setdefault(zip_path, []).append(filename)

    for zip_path, filenames in extract_map.items():
        with zipfile.ZipFile(zip_path, 'r') as zf:
            for filename in sorted(filenames):
                info = zf.getinfo(filename)
                if getattr(info, 'is_dir', lambda: filename.endswith('/'))():
                    continue
                out_path = os.path.join(extract_dir, filename)
                os.makedirs(os.path.dirname(out_path), exist_ok=True)
                with zf.open(filename, 'r') as src, open(out_path, 'wb') as dst:
                    if filename in extender_configs:
                        dst.write(extender_configs[filename])
                    else:
                        shutil.copyfileobj(src, dst)
                perm = (getattr(info, 'external_attr', 0) >> 16) & 0o7777
                if perm:
                    os.chmod(out_path, perm)

    print("Merged platform SDKs into", extract_dir)


def merge_platform_sdk_zips_with_zipmerge(zipmerge_path, platform_zips, output_zip_path, canonical_platform='x86_64-linux'):
    """
    Merge multiple per-platform SDK zips with zipmerge directly into `output_zip_path`.
    Later inputs overwrite earlier entries. Apply merged extender configurations last.
    """
    ordered_platform_zips = _ordered_platform_sdk_zips(platform_zips, canonical_platform)
    extender_configs = _merge_extender_configs(ordered_platform_zips)

    os.makedirs(os.path.dirname(output_zip_path), exist_ok=True)
    if os.path.exists(output_zip_path):
        os.unlink(output_zip_path)

    print("Merging platform SDKs with", zipmerge_path)
    print("Highest precedence SDK platform:", ordered_platform_zips[0][0])
    merge_inputs = ordered_platform_zips[1:] + ordered_platform_zips[:1]
    args = [zipmerge_path, '-s', output_zip_path] + [z for _, z in merge_inputs]
    if extender_configs:
        # A final overlay replaces entries rather than adding duplicate ZIP members.
        with tempfile.NamedTemporaryFile(suffix='.zip', delete=False) as temporary:
            config_zip_path = temporary.name
        try:
            with zipfile.ZipFile(config_zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
                for name, data in sorted(extender_configs.items()):
                    zf.writestr(name, data)
            run.command(args + [config_zip_path])
        finally:
            os.unlink(config_zip_path)
    else:
        run.command(args)
    print("Merged platform SDKs into", output_zip_path)


def build_combined_sdk_tree(netloc, base_prefix, platforms, extract_dir, canonical_platform='x86_64-linux'):
    """
    High-level helper used by scripts/build.py: downloads per-platform zips, merges them into a single tree,
    and cleans up the downloaded zip files.
    """
    with download_platform_sdk_zips(netloc, base_prefix, platforms) as platform_zips:
        merge_platform_sdk_zips_into_tree(platform_zips, extract_dir, canonical_platform=canonical_platform)


def build_combined_sdk_zip(netloc, base_prefix, platforms, output_zip_path, zipmerge_path, canonical_platform='x86_64-linux'):
    """
    High-level helper used by scripts/build.py: downloads per-platform zips, merges them into one zip archive,
    and cleans up the downloaded zip files.
    """
    with download_platform_sdk_zips(netloc, base_prefix, platforms) as platform_zips:
        merge_platform_sdk_zips_with_zipmerge(
            zipmerge_path,
            platform_zips,
            output_zip_path,
            canonical_platform=canonical_platform)
