package com.dynamo.bob.archive;

public class ArchiveEntryData
{
	public int pathSize;
	public byte[] path;
	//public String path;
	public int hashSize;
	public byte[] hashDigest;
	public int resource_offset; // offset of resource data in data-file
	public int resource_size;
	public int resource_compressed_size;
	public int flags;
	
	public ArchiveEntryData() {
		this.pathSize = 0;
		this.path = null;
		this.hashSize = 0;
		this.hashDigest = null;
		this.resource_offset = 0;
		this.resource_size = 0;
		this.resource_compressed_size = 0;
		this.flags = 0;
	}
}