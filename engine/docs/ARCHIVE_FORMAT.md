# The game archive format

## What is it?

When building the game content, Defold outputs all the game content into a few central data files:

* The index file (`game.arci`)
* The data file (`game.arcd`)
* The manifest file (`game.dmanifest`)
* The public certificate (`game.public.der`)

This is not an uncommon practice, and some famous examples are the [.WAD](https://zdoom.org/wiki/WAD) and [.PAK](https://quakewiki.org/wiki/.pak) formats (Doom, Quake, Half Life, Far Cry and many others)

## Benefits

There are multiple benefits to using a "single" data file, as opposed to keeping each asset in a unique file.

### Fewer open file handles

File handles are OS resources, and should be used sparingly. Most systems has an upper limit on how many open file handles are allowed.

Also, the cost of opening a file is not insignificant, and may cause a delay in the game.

### Bootstrap / Seek times

If startup times are slow, we could sort the archive to store the files so that the resources needed at startup, are accessible first in the archive.
This has bigger impact on slower physical media such as blue rays.

_We currently don't support this, but we might in the future if we see the need for it._

### Memory mapping

We currently use memory mapping in our engine, which allows us to access the data just as if it was a regular memory buffer.

It allows us to delegate the memory management of this data to the OS.

### Downloading

On HTML5 we don't have to download each file individually.



## The format

We won't go into the per-byte detail of the formats here, since that may easily make the documentation obsolete.

Instead, for the latest source code, visit [ArchiveBuilder.java](https://github.com/defold/defold/blob/dev/com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/archive/ArchiveBuilder.java)

### The index file `.arci`

The index file is the "Table of contents" for the archive data.
This file is structured in a way so that it's directly mappable to a runtime struct, and ready to be used for quick lookups.

The file starts with a HEADER, specifying the file format version number, and also the offsets to the payload data.

The payload data, is an both a list of (sorted) hashes, and and a list of index entries.
These two lists are of the same length, are in fact a 1:1 match. This makes it easy to use the hash array as a quick lookup (binary search) into the array of entries.

The hashes are a 64bit hash (using [dmHashString64()](https://defold.com/ref/stable/dmHash/?q=dmhashstring64#dmHashString64:string)) of the relative file path of the resource.

The resource entry contains the resource size, and compressed size (if it is compressed). It also has a set of flags with meta data, such as if the resource is compressed and/or obfuscated.

<pre>
HEADER:
  header.version
  header.num_entries
  header.hashes_offset
  header.entries_offset
HASH0
HASH1
 ...
HASHn
ENTRY0
  entry.resource_offset
  entry.resource_size
  entry.resource_compressed_size
  entry.flags
ENTRY1
 ...
ENTRYn
CHECKSUM
</pre>


### The data file `.arcd`

The data file itself contains an array of `resources`.
Each resource is found at the offset specified by its index entry (`entry.resource_offset`)

At runtime, we reverse the compression/obfuscation as necessary. We currently use LZ4 for compression, due to it's decompression speed. We don't compress the archive file itself, since each resource is individually compressed.

We also make sure each resource starts at a good address by padding out the file accordingly between each entry.


<pre>
RESOURCE0
PAD
RESOURCE1
PAD
...
RESOURCEN
PAD
</pre>

### The manifest file `.dmanifest`

The manifest file contains cryptographic checksums of both the entire data set, as well as the individual files.
The manifest file is mainly used for Live Update content on the platforms that support this feature.
Each entry in the manifest may have a list of dependants, which is a list of url hashes. The dependants refer to which oother files are required to be loaded in order for the resource itself to be loaded.

See [](./LIVEUPDATE.md) for more detailed info about the liveupdate system.

See [ManifestBuilder.java](https://github.com/defold/defold/blob/dev/com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/archive/ManifestBuilder.java) for more detailed information.

#### Debugging

It is possible to print the contents of a `.dmanifest` by calling `<defold>/scripts/unpack_ddf.py /path/to/game.dmanifest`

### The public key `.public.der`

The public key is used to verify any downloaded manifest or resource file when using the Live Update feature.


