# The game archive format

## What is it?

When building the game content, Defold outputs all the game content into a few central data files:

* The index file (`game.arci`)
* The data file (`game.arcd`)
* The manifest file (`game.dmanifest`)

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

Instead, for the latest source code, visit [ArchiveBuilder.java](../../com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/archive/ArchiveBuilder.java)

### The index file `.arci`

The index file is the "Table of contents" for the archive data.
This file is structured in a way so that it's directly mappable to a runtime struct, and ready to be used for quick lookups.

The file starts with a HEADER, specifying the file format version number, and also the offsets to the payload data.

The payload contains a sorted array of hashes and an equally sized array of entries. A binary search in the hash array gives the index of the corresponding entry.

The manifest maps the 64-bit hash of a resource path to a digest of the stored resource bytes, after any compression and encryption. The engine looks up that digest in the sorted `.arci` hash table. Bob uses SHA-1 for game resources and writes each digest into a 64-byte slot.

The resource entry contains the archive data offset, resource size, compressed size (if it is compressed), and a set of flags with meta data, such as if the resource is compressed and/or obfuscated.

Archive index version 6 is the current supported version and uses the packed 64-bit entry layout: flags are stored in the high 4 bits of a 64-bit entry word and the low 60 bits store the archive data offset, so a `.arcd` file can grow beyond 4 GiB without increasing the entry size. Resource sizes remain 32-bit.

<pre>
HEADER:
  header.version
  header.padding
  header.user_data
  header.num_entries
  header.entries_offset
  header.hashes_offset
  header.hash_length
  header.index_md5
HASH0
HASH1
 ...
HASHn
ENTRY0
  entry.offset_and_flags             # flags in bits 63..60, offset in bits 59..0
  entry.resource_size
  entry.resource_compressed_size
ENTRY1
 ...
ENTRYn
</pre>


### The data file `.arcd`

The data file itself contains an array of `resources`.
The low 60 bits of `entry.offset_and_flags` give the byte offset of the resource in `.arcd`.

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

The base archive and Live Update archives use the manifest to resolve resource paths. It records content hashes and compatibility metadata, including supported engine versions. An entry can also list the path hashes of resources that need to be downloaded with it.

See the [Live Update overview](LIVEUPDATE.md) for how resources are mounted and loaded.

See [ManifestBuilder.java](../../com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/archive/ManifestBuilder.java) for more detailed information.

#### Debugging

It is possible to print the contents of a `.dmanifest` by calling `<defold>/scripts/unpack_ddf.py /path/to/game.dmanifest`
