# Liveupdate

This document is intended to document the technical details of our live update system.
For a more high level overview, refer to [the manual](https://defold.com/manuals/live-update/).

## Resource system

### Resource providers

[Source](https://github.com/defold/defold/tree/dev/engine/resource/src/providers)

We use an interface called Provider that allows us to add or remove different reading capabilities to the engine.

These providers are added using a provider extension system using the macro `DM_DECLARE_ARCHIVE_LOADER`.
The declared C++ symbol must also be added to the build system or it will be optimized away.

Each provider type is then registered at global scope when the engine starts.

The engine can read content from multiple sources:

* `file` - Used during development to read built files from the project folder
* `archive` - Used for the base game archive (`.dmanifest`, `.arci`, `.arcd`)
* `zip` - Used to read content from regular .zip files (e.g. .obb on Android)
* `http` - Used to read files over http when the engine is attached to the editor.

### Mounts

Our resource system uses a list of "mounts".
Each mount is an instance of a resource provider type, with a priority.
As each mount is added, the list is sorted on priority, with the highest priority first.

At engine start, we create the mounts:
```
    Create "_base" mount from project URI
    Create "_builtin" archive using embedded connect project
```

These mounts are held by an instance of `dmResourceMounts::HContext` from `resource_mounts.h`.

### Reading resources

When the resource system (the `dmResource::HFactory`) need to read a resource, we call into the `dmResourceMounts` api.
It will in turn loop over each mount, to see if the mount contains the resource in question:

```
    foreach mount in mounts:
        contains = mount.contains(path, path_hash)
        if contains:
            return mount.readfile(path, path_hash, resource_buffer)
    return result.NOT_FOUND
```

## Liveupdate library

[Source](https://github.com/defold/defold/tree/dev/engine/liveupdate/src)

Separate from the resource system, we have a library called `liveupdate`. It contains the [Lua scripting api](https://defold.com/ref/stable/liveupdate/) for the liveupdate functionality as well as some code to handle our legacy liveupdate system.

### Content

Currently, `bob.jar` (`ArchiveBuilder.java`/`ZipPublisher.java`) is responsible for generating the liveupdate content.
It produces a .zip file with the content, as well as the base game archive.

The developer may provide this excluded content to the game any way they wish as a mounted archive.
They handle the download (usually using our http.request() api), and subsequently call our scripting api to add the mount.

The .zip file contains a `.dmanifest` file which contains metadata about the resources.
Most notably, it contains some dependencies between resources, in order for the developer to determine if more liveupdate content needs to be downloaded.

### User defined mounts

The mount scripting api consists of three main functions: `liveupdate.add_mount()`/`liveupdate.remove_mount()`/`liveupdate.get_mounts()`
Mounts are active for the current session only. Applications that need mounts after restart must add them again.

Mount names are accepted as strings or hashes by `liveupdate.add_mount()` and `liveupdate.remove_mount()`, and stored internally as hashes. The reserved base mount names `_base` and `_builtin` cannot be added or removed from script. The `name` field returned by `liveupdate.get_mounts()` is a hash, and the `liveupdate.add_mount()` callback receives `(self, name, uri, result)`, where `name` is the mount name hash.

The api also exposes `liveupdate.is_built_with_excluded_files()`, which reports whether the bundled manifest was produced with excluded resources. This is a build metadata check, not a check for currently mounted or downloaded content.

### Resources

Before a collection proxy is loaded, it may be that some content is missing.
For archive/mount-based liveupdate flows, the normal approach is to mount the relevant archive before loading. If the developer needs to inspect the full excluded resource set for a collection proxy, they can use [collectionproxy.get_resources()](https://defold.com/ref/stable/collectionproxy/#collectionproxy.get_resources:collectionproxy).

Using this list, the developer will determine which content to download and add to the resource system.

The list of dependencies is stored in the `.dmanifest` of each mount (if it has one).

## Debugging

For easier debugging liveupdate content, there is a define `DM_RESOURCE_DBG_LOG_LEVEL` in `resource_private.h` that toggles a logging macro `DM_RESOURCE_DBG_LOG(level, ...)`.

It is also possible to print the contents of a `.dmanifest` by calling `<defold>/scripts/unpack_ddf.py /path/to/game.dmanifest`
