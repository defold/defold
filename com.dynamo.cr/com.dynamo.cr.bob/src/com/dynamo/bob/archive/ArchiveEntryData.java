package com.dynamo.bob.archive;

public class ArchiveEntryData
{
	
	public int resource_offset; // offset of resource data in data-file
	public int resource_size;
	public int resource_compressed_size;
	public int flags;
	public int pathSize;
	public int hashSize;
	public byte[] path;
	public byte[] hashDigest;
	
	public ArchiveEntryData() {
		this.resource_offset = 0;
		this.resource_size = 0;
		this.resource_compressed_size = 0;
		this.flags = 0;
		this.pathSize = 0;
		this.hashSize = 0;
		this.path = null;
		this.hashDigest = null;
	}
}