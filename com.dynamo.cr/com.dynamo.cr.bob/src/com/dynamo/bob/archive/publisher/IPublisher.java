package com.dynamo.bob.archive.publisher;

import java.io.InputStream;
import java.util.List;

import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.fs.IResource;

public interface IPublisher {

	public void Initialize(PublisherSettings settings);
	public void Finalize();
	
	public List<String> getOutputs(IResource input);
	public List<InputStream> getOutputResults();
	
	public void AddEntry(String hexDigest, ArchiveEntry entry);
	
}
